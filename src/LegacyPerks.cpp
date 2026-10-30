/*
 * mod-legacy: what the perks do.
 *
 * Most perks are a hook that looks up the player's rank when it fires. Four are hidden auras
 * (High Alert, Explorer's Stride, The Quick and the Dead's ghost speed, Reagent Economy) that a
 * player's own map update brings in line with their ranks every second, which also covers a
 * build changed from the launcher.
 *
 * Changes from WoW Forever, for systems 3.3.5 or this realm doesn't have:
 * - Talented keeps WoW Forever's rule (an earlier start, the same total); Prodigy, after it, adds
 *   points on top. Explorer's Stride is an extra perk of this realm's.
 * - Second Wind replaces Field Guide (no camps).
 * - Bountiful Harvest doubles a gather instead of adding Scarce materials.
 * - Salvager replaces Performance Bonus (no commerce crates).
 * - Dedicated Study does nothing once every profession is at its maximum (no Elemental Essence).
 * - For Great Honor counts honorable kills; battleground bonus honor has no hook.
 * - Reinforce gives back part of the durability lost between a killing blow and the update after
 *   resurrection (the death and the spirit healer), since the core has no hook on durability loss.
 * - The Quick and the Dead refunds what a helpful spell took just after the cast, so a spell
 *   still needs the mana to start.
 * - Working Overtime is a second skill-up roll, so at orange recipes it can give two points.
 * - Hasty Hearth is new: the realm's Hearthstone is back at 30 min, and each rank takes 5 min off
 *   it (and off the other ways home, never below 5 min).
 * - Well Rested needs the core's OnPlayerGainRestBonus / OnPlayerGetRestBonusMax hooks, and
 *   Frequent Flier the OnPlayerTaxiCostDiscount / OnPlayerTaxiFlightSpeed hooks. On a core
 *   without them Well Rested and the faster flights do nothing, and only a flight's first leg is
 *   cheaper (the flight master's price).
 *
 * Released under the MIT License.
 */

#include "Legacy.h"
#include "AreaDefines.h"
#include "Bag.h"
#include "Chat.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "DataMap.h"
#include "DBCStores.h"
#include "Formulas.h"
#include "GameObject.h"
#include "GameTime.h"
#include "Item.h"
#include "Log.h"
#include "LootMgr.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include <functional>

using namespace Legacy;

namespace
{
    constexpr uint32 REFRESH_MS = 1000;
    constexpr uint32 CHALLENGE_CHECK_MS = 30000;

    constexpr uint32 SPELL_HIGH_ALERT = 90480;
    constexpr uint32 SPELL_EXPLORERS_STRIDE = 90481;
    constexpr uint32 SPELL_QUICK_AND_DEAD = 90482;
    constexpr uint32 SPELL_REAGENT_ECONOMY_BASE = 90489;    // + class; Druid (11) is 90499
    constexpr uint32 SPELL_RECENTLY_BANDAGED = 11196;
    constexpr uint32 SPELL_RESURRECTION_SICKNESS = 15007;

    constexpr uint32 THRILL_TICKS = 5;
    constexpr uint32 THRILL_TICK_MS = 2000;
    constexpr Seconds QUICK_AND_DEAD_WINDOW = 60s;
    constexpr Seconds SNAPSHOT_STALE = 3s;

    uint32 ReagentSpell(uint8 cls)
    {
        return cls == CLASS_DRUID ? SPELL_REAGENT_ECONOMY_BASE + 10 : SPELL_REAGENT_ECONOMY_BASE + cls;
    }

    std::array<bool, MAX_CLASSES> reagentMask = {};

    // Per player. Touched from that player's map update, or from the world thread, which never
    // runs alongside the map updates.
    struct PerkData : public DataMap::Base
    {
        uint32 timer = 0;
        uint32 challengeTimer = 0;
        bool checkChallenges = true;

        // Thrill of Adventure: ticks left and percent of health and mana per tick (x100).
        uint32 thrillTicks = 0;
        uint32 thrillTimer = 0;
        uint32 thrillPerTick = 0;

        // Hardcore (mod-challenges), from the database at login, level-up and resurrection.
        bool hardcore = false;

        // Dedicated Study: when this character last got its skill point.
        uint32 studyAt = 0;
        bool studyLoaded = false;

        // Second Wind: shorten the sickness once the resurrection has applied it.
        bool shortenSickness = false;

        // The Quick and the Dead: free helpful spells until then. Per power, what the player had
        // before the casts since the last update and what those casts cost; what was really
        // taken (up to the cost) goes back at the next update, after TakePower.
        Seconds freeUntil = 0s;
        std::array<int32, MAX_POWERS> powerBefore = {};
        std::array<uint32, MAX_POWERS> cost = {};

        // Reinforce: durability of every item at the killing blow, given back at the update
        // after resurrection (after the spirit healer's loss too).
        std::vector<std::pair<ObjectGuid, uint32>> snapshot;
        Seconds snapshotAt = 0s;
        bool restore = false;
    };

    PerkData* DataOf(Player* player)
    {
        return player->CustomData.GetDefault<PerkData>("Legacy");
    }

    bool InInstance(Player const* player)
    {
        Map const* map = player->FindMap();
        return map && map->Instanceable();
    }

    bool InPvp(Player const* player)
    {
        Map const* map = player->FindMap();
        return map && map->IsBattlegroundOrArena();
    }

    // The perk's number (percent, seconds) at the player's rank.
    uint32 Percent(Player const* player, Perk perk)
    {
        return Rank(player, perk) * Perks()[perk].perRank;
    }

    // Second Wind and Gourmand go in thirds.
    uint32 Thirds(uint8 rank)
    {
        return rank >= 3 ? 100 : rank * 33 + (rank == 2 ? 1 : 0);
    }

    // Guard so items the perks hand out don't trigger the perks again.
    thread_local bool granting = false;

    void GrantExtra(Player* player, uint32 entry, uint32 count, char const* perk)
    {
        if (granting || !count)
            return;
        granting = true;
        if (player->AddItem(entry, count))
            ChatHandler(player->GetSession()).PSendSysMessage("|cffe6cc80{}:|r you get {} more.", perk, count == 1 ? "one" : std::to_string(count));
        granting = false;
    }

    void ForEachItem(Player* player, std::function<void(Item*)> const& fn)
    {
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
            if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                fn(item);
        for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
            if (Bag* bag = player->GetBagByPos(bagSlot))
                for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
                    if (Item* item = bag->GetItemByPos(uint8(slot)))
                        fn(item);
    }

    // Deep Pockets' extra slots for the player's bags in the four bag slots. They stay with a
    // suspended build: the perk is permanent, and shrinking would push items out of the bags.
    uint32 PocketSlots(Player const* player)
    {
        if (!GetConfig().enabled || !GetConfig().deepPockets)
            return 0;
        return SavedRank(player->GetGUID().GetCounter(), PERK_DEEP_POCKETS) * Perks()[PERK_DEEP_POCKETS].perRank;
    }

    // A bag's size is its own plus `extra`, but never less than the last slot holding an item.
    // Returns whether the size changed.
    bool FitBag(Bag* bag, uint32 extra)
    {
        uint32 size = std::min<uint32>(bag->GetTemplate()->ContainerSlots + extra, MAX_BAG_SIZE);
        for (uint32 slot = bag->GetBagSize(); slot > size; --slot)
            if (bag->GetItemByPos(uint8(slot - 1)))
            {
                size = slot;
                break;
            }
        if (bag->GetBagSize() == size)
            return false;
        bag->SetUInt32Value(CONTAINER_FIELD_NUM_SLOTS, size);
        return true;
    }

    // Whether a bag has an item past its own size (in Deep Pockets' slots).
    bool HoldsExtra(Bag const* bag)
    {
        for (uint32 slot = bag->GetTemplate()->ContainerSlots; slot < bag->GetBagSize(); ++slot)
            if (bag->GetItemByPos(uint8(slot)))
                return true;
        return false;
    }

    void SetAuraAmounts(Aura* aura, int32 amount)
    {
        if (!aura)
            return;
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
            if (AuraEffect* effect = aura->GetEffect(i))
                if (effect->GetAmount() != amount)
                    effect->ChangeAmount(amount);
    }

    // Adds or removes a hidden aura so the player has it exactly when `want`.
    void Keep(Player* player, uint32 spellId, bool want, int32 amount)
    {
        Aura* aura = player->GetAura(spellId);
        if (want && !aura)
            SetAuraAmounts(player->AddAura(spellId, player), amount);
        else if (want)
            SetAuraAmounts(aura, amount);
        else if (aura)
            player->RemoveAurasDueToSpell(spellId);
    }

    bool IsWellFed(SpellInfo const* spell)
    {
        char const* name = spell->SpellName[0];
        return name && std::string_view(name) == "Well Fed";
    }

    // Permanence: a class buff of 10 min or more that can go on someone else.
    bool IsLongGroupBuff(SpellInfo const* spell, Player const* caster, int32 maxDuration)
    {
        if (maxDuration < 10 * MINUTE * IN_MILLISECONDS || spell->IsPassive() || !spell->IsPositive())
            return false;
        ChrClassesEntry const* cls = sChrClassesStore.LookupEntry(caster->getClass());
        if (!cls || spell->SpellFamilyName != cls->spellfamily)
            return false;
        for (SpellEffectInfo const& effect : spell->Effects)
            if (effect.Effect == SPELL_EFFECT_APPLY_AURA && effect.TargetA.GetTarget() != TARGET_UNIT_CASTER)
                return true;
        return false;
    }

    // A herb or ore node: its lock needs Herbalism or Mining.
    bool IsGatheringNode(GameObject const* go)
    {
        LockEntry const* lock = sLockStore.LookupEntry(go->GetGOInfo()->GetLockId());
        if (!lock)
            return false;
        for (uint8 i = 0; i < MAX_LOCK_CASE; ++i)
            if (lock->Type[i] == LOCK_KEY_SKILL && (lock->Index[i] == LOCKTYPE_HERBALISM || lock->Index[i] == LOCKTYPE_MINING))
                return true;
        return false;
    }

    // Bountiful Harvest: was this loot from a herb or ore node, or skinning?
    bool IsGathered(Player* player, ObjectGuid guid)
    {
        if (guid.IsGameObject())
        {
            GameObject* go = player->GetMap()->GetGameObject(guid);
            return go && go->loot.loot_type == LOOT_SKINNING && IsGatheringNode(go);
        }
        if (guid.IsCreatureOrVehicle())
        {
            Creature* creature = player->GetMap()->GetCreature(guid);
            return creature && creature->loot.loot_type == LOOT_SKINNING;
        }
        return false;
    }

    // Salvager: disenchanting, prospecting or milling.
    bool IsSalvaged(Player* player, ObjectGuid guid)
    {
        if (!guid.IsItem())
            return false;
        Item* item = player->GetItemByGuid(guid);
        return item && (item->loot.loot_type == LOOT_DISENCHANTING || item->loot.loot_type == LOOT_PROSPECTING
            || item->loot.loot_type == LOOT_MILLING);
    }

    // Luremaster: a fishing bobber or pool, with a lure on the fishing pole.
    bool IsLuredFishing(Player* player, ObjectGuid guid)
    {
        if (!guid.IsGameObject())
            return false;
        GameObject* go = player->GetMap()->GetGameObject(guid);
        if (!go || (go->GetGoType() != GAMEOBJECT_TYPE_FISHINGNODE && go->GetGoType() != GAMEOBJECT_TYPE_FISHINGHOLE))
            return false;
        Item* pole = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
        return pole && pole->GetTemplate()->Class == ITEM_CLASS_WEAPON && pole->GetTemplate()->SubClass == ITEM_SUBCLASS_WEAPON_FISHING_POLE
            && pole->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT);
    }

    bool IsFish(ItemTemplate const* proto)
    {
        if (proto->Quality < ITEM_QUALITY_NORMAL || proto->StartQuest || proto->Bonding == BIND_QUEST_ITEM)
            return false;
        return proto->Class == ITEM_CLASS_TRADE_GOODS || (proto->Class == ITEM_CLASS_CONSUMABLE && proto->SubClass == ITEM_SUBCLASS_FOOD);
    }

    bool IsCooking(Spell const* spell)
    {
        if (!spell)
            return false;
        SkillLineAbilityMapBounds bounds = sSpellMgr->GetSkillLineAbilityMapBounds(spell->GetSpellInfo()->Id);
        for (auto itr = bounds.first; itr != bounds.second; ++itr)
            if (itr->second->SkillLine == SKILL_COOKING)
                return true;
        return false;
    }

    void DedicatedStudy(Player* player, PerkData* data)
    {
        uint32 guid = player->GetGUID().GetCounter();
        if (!data->studyLoaded)
        {
            data->studyLoaded = true;
            if (QueryResult result = CharacterDatabase.Query("SELECT study_at FROM mod_legacy_character WHERE guid = {}", guid))
                data->studyAt = result->Fetch()[0].Get<uint32>();
        }
        uint32 dayStart = uint32(sWorld->GetNextDailyQuestsResetTime().count()) - DAY;
        if (data->studyAt >= dayStart)
            return;

        uint32 lowest = 0;
        uint16 lowestValue = 0;
        for (SkillLineEntry const* skill : sSkillLineStore)
        {
            if (!skill || (skill->categoryId != SKILL_CATEGORY_PROFESSION && skill->categoryId != SKILL_CATEGORY_SECONDARY))
                continue;
            if (skill->id == SKILL_RIDING || !player->HasSkill(skill->id))
                continue;
            uint16 value = player->GetPureSkillValue(skill->id);
            if (value >= player->GetPureMaxSkillValue(skill->id))
                continue;
            if (!lowest || value < lowestValue)
            {
                lowest = skill->id;
                lowestValue = value;
            }
        }
        if (!lowest)
            return;

        player->SetSkill(lowest, player->GetSkillStep(lowest), lowestValue + 1, player->GetPureMaxSkillValue(lowest));
        data->studyAt = uint32(GameTime::GetGameTime().count());
        // Direct, so a quick relog can't read the old time back. At most once a day.
        CharacterDatabase.DirectExecute("REPLACE INTO mod_legacy_character (guid, study_at) VALUES ({}, {})", guid, data->studyAt);
        SkillLineEntry const* skill = sSkillLineStore.LookupEntry(lowest);
        ChatHandler(player->GetSession()).PSendSysMessage("|cffe6cc80Dedicated Study:|r your {} rises to {}.", skill->name[0], lowestValue + 1);
    }

    // Hasty Hearth: the spells that take a player home (the Hearthstone, Astral Recall, Scrolls of
    // Recall, the Ruby Slippers, ...).
    constexpr std::array<uint32, 10> HEARTH_SPELLS = { 556, 8690, 14055, 18034, 22563, 22564, 48129, 60320, 60321, 75136 };
    constexpr uint32 HEARTH_FLOOR_MS = 5 * MINUTE * IN_MILLISECONDS;

    bool IsHearth(uint32 spellId)
    {
        return std::find(HEARTH_SPELLS.begin(), HEARTH_SPELLS.end(), spellId) != HEARTH_SPELLS.end();
    }

    // The cast spell and the spells sharing its category, which AddSpellAndCategoryCooldowns put
    // on cooldown with it.
    std::vector<uint32> HearthCooldownSpells(Player* player, SpellInfo const* spellInfo)
    {
        std::vector<uint32> spells = { spellInfo->Id };
        if (uint32 category = spellInfo->GetCategory())
        {
            SpellCategoryStore::const_iterator itr = sSpellsByCategoryStore.find(category);
            if (itr != sSpellsByCategoryStore.end())
                for (auto const& [byItem, siblingId] : itr->second)
                    if (siblingId != spellInfo->Id && (byItem || player->HasSpell(siblingId))
                        && std::find(spells.begin(), spells.end(), siblingId) == spells.end())
                        spells.push_back(siblingId);
        }
        return spells;
    }

    // Takes `cut` ms off each of the spells' cooldowns, leaving at least 5 min.
    void ShortenHearth(Player* player, std::vector<uint32> const& spells, uint32 cut)
    {
        uint32 now = GameTime::GetGameTimeMS().count();
        SpellCooldowns& cooldowns = player->GetSpellCooldownMap();
        for (uint32 spellId : spells)
        {
            auto itr = cooldowns.find(spellId);
            if (itr == cooldowns.end() || itr->second.end <= now + HEARTH_FLOOR_MS)
                continue;
            uint32 take = std::min(cut, itr->second.end - now - HEARTH_FLOOR_MS);
            itr->second.end -= take;
            itr->second.maxduration -= std::min(take, itr->second.maxduration);
        }
    }

    // The client starts the cooldown its own Spell.dbc names (this realm's patch once made the
    // Hearthstone's 5 min), so the server's real ones replace it: the time left, or none.
    void SyncHearth(Player* player, std::vector<uint32> const& spells)
    {
        uint32 now = GameTime::GetGameTimeMS().count();
        SpellCooldowns const& cooldowns = player->GetSpellCooldownMap();
        PacketCooldowns left;
        for (uint32 spellId : spells)
        {
            auto itr = cooldowns.find(spellId);
            if (itr != cooldowns.end() && itr->second.end > now)
                left[spellId] = itr->second.end - now;
            else
                player->SendClearCooldown(spellId, player);
        }
        if (left.empty())
            return;
        WorldPacket data;
        player->BuildCooldownPacket(data, SPELL_COOLDOWN_FLAG_NONE, left);
        player->SendDirectMessage(&data);
    }
}

namespace Legacy
{
    // A bag leaves the four bag slots empty (the core) or at least empty of its extra slots
    // (OnPlayerCanUnequipItem), so every other bag gets its own size back; the bank bag slots, the
    // backpack, the bank and empty bags kept inside bags are all looked at.
    void UpdateBags(Player* player, Optional<uint32> extra)
    {
        uint32 pockets = extra.value_or(PocketSlots(player));
        // At login the bags aren't sent yet: they go out with the new size. In the world only the
        // value changes: a stock 3.3.5 client keeps the size it read when it created the bag (and
        // re-enables that bag if it's sent again), so it needs the launcher's Wow.exe edit (wow-server
        // tools/client-patches/live_bag_slots.py) to see it before a relog.
        auto fitInside = [](Bag* bag)
        {
            for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
                if (Item* item = bag->GetItemByPos(uint8(slot)))
                    if (Bag* inner = item->ToBag())
                        FitBag(inner, 0);
        };
        for (uint8 slot = INVENTORY_SLOT_BAG_START; slot < BANK_SLOT_BAG_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            Bag* bag = item ? item->ToBag() : nullptr;
            if (!bag)
                continue;
            bool worn = slot < INVENTORY_SLOT_BAG_END;
            FitBag(bag, worn ? pockets : 0);
            if (worn || slot >= BANK_SLOT_BAG_START)
                fitInside(bag);
        }
    }

    void RefreshPerks(Player* player)
    {
        // Every logout runs Unit::CleanupBeforeRemoveFromMap: it takes the player out of the
        // world, removes all auras, locks the aura list and then calls CombatStop, which fires
        // OnPlayerLeaveCombat. Adding an aura after that lock is a core ASSERT (a server crash),
        // so nothing is refreshed for a player who is not in the world.
        if (!player->IsInWorld())
            return;

        bool pvp = InPvp(player);
        bool alive = player->IsAlive() && !player->HasPlayerFlag(PLAYER_FLAGS_GHOST);

        uint8 highAlert = Rank(player, PERK_HIGH_ALERT);
        Keep(player, SPELL_HIGH_ALERT, highAlert && !pvp, int32(highAlert * Perks()[PERK_HIGH_ALERT].perRank));

        uint32 stride = Percent(player, PERK_EXPLORERS_STRIDE);
        Keep(player, SPELL_EXPLORERS_STRIDE, stride && alive && !player->IsInCombat() && !player->IsMounted() && !InInstance(player), int32(stride));

        uint32 ghost = Percent(player, PERK_QUICK_AND_DEAD);
        if (pvp)
            DataOf(player)->freeUntil = 0s;
        Keep(player, SPELL_QUICK_AND_DEAD, ghost && !pvp && player->HasPlayerFlag(PLAYER_FLAGS_GHOST), int32(ghost));

        uint8 cls = player->getClass();
        if (cls < MAX_CLASSES && reagentMask[cls])
            Keep(player, ReagentSpell(cls), Rank(player, PERK_REAGENT_ECONOMY) > 0, 0);
    }

    // Which class spells need only reagents a vendor sells: their spell family flags go in that
    // class's Reagent Economy aura, minus any flag a spell with another reagent (a Soul Shard)
    // also uses.
    void LoadReagentMasks()
    {
        std::unordered_set<uint32> sold;
        for (auto const& [entry, creature] : *sObjectMgr->GetCreatureTemplates())
            if (VendorItemData const* items = sObjectMgr->GetNpcVendorItemList(entry))
                for (VendorItem const* item : items->m_items)
                    if (item && !item->ExtendedCost)    // for gold, not honor or tokens
                        sold.insert(item->item);

        for (uint8 cls = CLASS_WARRIOR; cls < MAX_CLASSES; ++cls)
        {
            ChrClassesEntry const* entry = sChrClassesStore.LookupEntry(cls);
            SpellInfo const* aura = entry ? sSpellMgr->GetSpellInfo(ReagentSpell(cls)) : nullptr;
            if (!aura)
                continue;

            flag96 good, bad;
            uint32 spells = 0;
            for (uint32 id = 0; id < sSpellMgr->GetSpellInfoStoreSize(); ++id)
            {
                SpellInfo const* spell = sSpellMgr->GetSpellInfo(id);
                if (!spell || spell->SpellFamilyName != entry->spellfamily || !spell->SpellFamilyFlags)
                    continue;
                bool any = false, allSold = true;
                for (uint8 i = 0; i < MAX_SPELL_REAGENTS; ++i)
                {
                    if (spell->Reagent[i] <= 0 || !spell->ReagentCount[i])
                        continue;
                    any = true;
                    if (!sold.count(uint32(spell->Reagent[i])))
                        allSold = false;
                }
                if (!any)
                    continue;
                if (allSold)
                {
                    good |= spell->SpellFamilyFlags;
                    ++spells;
                }
                else
                    bad |= spell->SpellFamilyFlags;
            }

            flag96 mask = good & ~bad;
            const_cast<SpellInfo*>(aura)->Effects[EFFECT_0].SpellClassMask = mask;
            reagentMask[cls] = bool(mask);
            if (spells)
                LOG_INFO("module", "mod-legacy: Reagent Economy for {}: {} spell ranks with vendor reagents{}.", entry->name[0], spells,
                    good == mask ? "" : " (some share a spell flag with a non-vendor reagent and keep their reagents)");
        }
    }

    void OnLogin(Player* player)
    {
        PerkData* data = DataOf(player);
        data->checkChallenges = true;

        uint32 guid = player->GetGUID().GetCounter();
        data->hardcore = IsHardcore(guid);
        Build build = GetBuild(guid);
        // The build was fine when it was saved; the point cap, the challenges or hardcore (taken
        // after the perks were picked) may have changed since. One that no longer fits is
        // switched off, not deleted, so fixing a config or data mistake brings it back. With no
        // challenges loaded at all (the world SQL is missing), nothing is judged.
        if (Spent(build) && ChallengeCount())
        {
            uint32 points = SpendablePoints(GetEarned(player->GetSession()->GetAccountId()), data->hardcore, CharacterEra(player));
            auto [code, message] = Validate(build, points, data->hardcore);
            if (!code.empty())
            {
                SuspendBuild(guid);
                // The core counted talent points while loading the character, with the perks on.
                player->InitTalentForLevel();
                ChatHandler(player->GetSession()).PSendSysMessage("|cffe6cc80Legacy:|r your perks are switched off: {} Change them on the launcher's LEGACY page.", message);
            }
        }

        RefreshPerks(player);
        if (Rank(player, PERK_DEDICATED_STUDY))
            DedicatedStudy(player, data);
    }
}

class LegacyPerksPlayerScript : public PlayerScript
{
public:
    LegacyPerksPlayerScript() : PlayerScript("LegacyPerksPlayerScript", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_MAP_CHANGED,
        PLAYERHOOK_ON_LEVEL_CHANGED,
        PLAYERHOOK_ON_CALCULATE_TALENTS_POINTS,
        PLAYERHOOK_ON_ACHI_COMPLETE,
        PLAYERHOOK_ON_REPUTATION_RANK_CHANGE,
        PLAYERHOOK_ON_UPDATE_SKILL,
        PLAYERHOOK_ON_PLAYER_ENTER_COMBAT,
        PLAYERHOOK_ON_PLAYER_LEAVE_COMBAT,
        PLAYERHOOK_ON_PLAYER_RELEASED_GHOST,
        PLAYERHOOK_ON_PLAYER_RESURRECT,
        PLAYERHOOK_ON_CREATURE_KILL,
        PLAYERHOOK_ON_CREATURE_KILLED_BY_PET,
        PLAYERHOOK_ON_SPELL_CAST,
        PLAYERHOOK_ON_GIVE_REPUTATION,
        PLAYERHOOK_ON_VICTIM_REWARD_AFTER,
        PLAYERHOOK_ON_UPDATE_CRAFTING_SKILL,
        PLAYERHOOK_ON_UPDATE_GATHERING_SKILL,
        PLAYERHOOK_ON_CREATE_ITEM,
        PLAYERHOOK_ON_LOOT_ITEM,
        PLAYERHOOK_ON_GET_REPUTATION_PRICE_DISCOUNT,
        PLAYERHOOK_ON_DELETE_FROM_DB,
        PLAYERHOOK_ON_EQUIP,
        PLAYERHOOK_ON_UNEQUIP_ITEM,
        PLAYERHOOK_CAN_UNEQUIP_ITEM,
#ifdef LEGACY_CORE_HAS_TAXI_SPEED
        PLAYERHOOK_ON_TAXI_FLIGHT_SPEED,
#endif
#ifdef LEGACY_CORE_HAS_REST_BONUS
        PLAYERHOOK_ON_GAIN_REST_BONUS,
        PLAYERHOOK_ON_GET_REST_BONUS_MAX,
#endif
#ifdef LEGACY_CORE_HAS_TAXI_COST
        PLAYERHOOK_ON_TAXI_COST_DISCOUNT,
#endif
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        // Bots log in by the thousand; their accounts earn and spend nothing.
        if (!GetConfig().enabled || IsBotSession(player->GetSession()))
            return;
        BackfillAccount(player->GetSession()->GetAccountId());
        OnLogin(player);
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (!GetConfig().enabled)
            return;

        PerkData* data = DataOf(player);

        // Thrill of Adventure
        if (data->thrillTicks)
        {
            if (data->thrillTimer > diff)
                data->thrillTimer -= diff;
            else if (player->IsAlive())
            {
                data->thrillTimer = THRILL_TICK_MS;
                --data->thrillTicks;
                player->ModifyHealth(int32(uint64(player->GetMaxHealth()) * data->thrillPerTick / 10000));
                if (uint32 maxMana = player->GetMaxPower(POWER_MANA))
                    player->ModifyPower(POWER_MANA, int32(uint64(maxMana) * data->thrillPerTick / 10000));
            }
            else
                data->thrillTicks = 0;
        }

        // Second Wind
        if (data->shortenSickness)
        {
            data->shortenSickness = false;
            if (Aura* sickness = player->GetAura(SPELL_RESURRECTION_SICKNESS))
            {
                int32 duration = int32(int64(sickness->GetDuration()) * (100 - Thirds(Rank(player, PERK_SECOND_WIND))) / 100);
                sickness->SetMaxDuration(std::max(duration, 1000));
                sickness->SetDuration(std::max(duration, 1000));
            }
        }

        // The Quick and the Dead: what the casts since the last update really took. A cast that
        // failed after the hook took nothing and gets nothing back.
        for (uint8 power = 0; power < MAX_POWERS; ++power)
            if (uint32 cost = data->cost[power])
            {
                data->cost[power] = 0;
                int32 taken = data->powerBefore[power] - int32(player->GetPower(Powers(power)));
                if (taken > 0)
                    player->ModifyPower(Powers(power), int32(std::min<uint32>(cost, uint32(taken))));
            }

        // Reinforce, once the resurrection (and the spirit healer's loss) is done.
        if (data->restore)
        {
            data->restore = false;
            std::vector<std::pair<ObjectGuid, uint32>> before;
            before.swap(data->snapshot);
            if (uint32 pct = Percent(player, PERK_REINFORCE))
                for (auto const& [itemGuid, durability] : before)
                {
                    Item* item = player->GetItemByGuid(itemGuid);
                    if (!item)
                        continue;
                    uint32 now = item->GetUInt32Value(ITEM_FIELD_DURABILITY);
                    if (now < durability)
                        if (int32 back = int32((durability - now) * pct / 100))
                            player->DurabilityPointsLoss(item, -back);
                }
        }

        // Reinforce: a killing blow the player survived (a duel) leaves a snapshot behind.
        else if (!data->snapshot.empty() && player->IsAlive() && GameTime::GetGameTime() - data->snapshotAt > SNAPSHOT_STALE)
            data->snapshot.clear();

        if (data->challengeTimer > diff)
            data->challengeTimer -= diff;
        else
        {
            data->challengeTimer = CHALLENGE_CHECK_MS;
            data->checkChallenges = true;
        }

        if (data->timer > diff)
        {
            data->timer -= diff;
            return;
        }
        data->timer = REFRESH_MS;
        RefreshPerks(player);

        if (data->checkChallenges)
        {
            data->checkChallenges = false;
            CheckChallenges(player, data->hardcore);
            // The same 30-second beat catches the daily reset for Dedicated Study.
            if (Rank(player, PERK_DEDICATED_STUDY) && player->IsInWorld())
                DedicatedStudy(player, data);
        }
    }

    void OnPlayerMapChanged(Player* player) override
    {
        if (GetConfig().enabled && !IsBotSession(player->GetSession()))
            RefreshPerks(player);
    }

    // Challenges: checked at the next refresh after anything they look at changes.
    void OnPlayerLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        if (!GetConfig().enabled || IsBotSession(player->GetSession()))
            return;
        PerkData* data = DataOf(player);
        data->checkChallenges = true;
        // Hardcore is taken at the starting level, so a level-up is when it's worth asking again.
        data->hardcore = IsHardcore(player->GetGUID().GetCounter());
    }

    // Talented and Prodigy. The core counts a point a level from level 10, plus bonus points.
    // Talented starts that a level earlier a rank, up to the same total at the top level (WoW
    // Forever's rule); Prodigy adds a point a rank once points have started. Builds are loaded at
    // startup, so the count made while a character loads already has them. Death Knights in Ebon
    // Hold keep their quest-based count.
    void OnPlayerCalculateTalentsPoints(Player const* player, uint32& talentPointsForLevel) override
    {
        if (!GetConfig().enabled || (player->IsClass(CLASS_DEATH_KNIGHT, CLASS_CONTEXT_TALENT_POINT_CALC) && player->GetMapId() == MAP_EBON_HOLD))
            return;

        uint8 talented = Rank(player, PERK_TALENTED);
        uint8 prodigy = Rank(player, PERK_PRODIGY);
        if (!talented && !prodigy)
            return;

        uint32 maxLevel = sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL);
        uint32 level = std::min<uint32>(player->GetLevel(), maxLevel);
        uint32 stock = level < 10 ? 0 : level - 9;
        uint32 early = level + talented < 10 ? 0 : std::min<uint32>(level + talented - 9, maxLevel - 9);
        if (!early)
            return;
        talentPointsForLevel += early - std::min(stock, early) + prodigy;
    }

    void OnPlayerAchievementComplete(Player* player, AchievementEntry const* /*achievement*/) override
    {
        DataOf(player)->checkChallenges = true;
    }

    void OnPlayerReputationRankChange(Player* player, uint32 /*factionID*/, ReputationRank /*newRank*/, ReputationRank /*oldRank*/, bool /*increased*/) override
    {
        DataOf(player)->checkChallenges = true;
    }

    void OnPlayerUpdateSkill(Player* player, uint32 /*skillId*/, uint32 /*value*/, uint32 /*max*/, uint32 /*step*/, uint32 /*newValue*/) override
    {
        DataOf(player)->checkChallenges = true;
    }

    // Explorer's Stride ends the moment combat starts; so does free casting.
    void OnPlayerEnterCombat(Player* player, Unit* /*enemy*/) override
    {
        if (!GetConfig().enabled)
            return;
        DataOf(player)->freeUntil = 0s;
        player->RemoveAurasDueToSpell(SPELL_EXPLORERS_STRIDE);
    }

    // Also fires on every logout, after the player has left the world (see RefreshPerks).
    void OnPlayerLeaveCombat(Player* player) override
    {
        if (GetConfig().enabled && !IsBotSession(player->GetSession()))
            RefreshPerks(player);
    }

    // The Quick and the Dead: ghost speed.
    void OnPlayerReleasedGhost(Player* player) override
    {
        if (GetConfig().enabled && !IsBotSession(player->GetSession()))
            RefreshPerks(player);
    }

    void OnPlayerResurrect(Player* player, float /*restorePercent*/, bool& applySickness) override
    {
        if (!GetConfig().enabled)
            return;

        player->RemoveAurasDueToSpell(SPELL_QUICK_AND_DEAD);
        PerkData* data = DataOf(player);
        // A death ends Hardcore.
        if (data->hardcore)
            data->hardcore = IsHardcore(player->GetGUID().GetCounter());

        // Second Wind
        if (uint8 rank = Rank(player, PERK_SECOND_WIND))
        {
            if (rank >= 3)
                applySickness = false;
            else if (applySickness)
                data->shortenSickness = true;
        }

        // The Quick and the Dead
        if (Rank(player, PERK_QUICK_AND_DEAD) >= 2 && !InPvp(player))
            data->freeUntil = GameTime::GetGameTime() + QUICK_AND_DEAD_WINDOW;

        // Reinforce: the spirit healer takes its share after this hook, so the update after does it.
        if (!data->snapshot.empty())
            data->restore = true;
    }

    // Thrill of Adventure, for the player's own kills and their pet's or totem's.
    void OnPlayerCreatureKill(Player* player, Creature* killed) override
    {
        Thrill(player, killed);
    }

    void OnPlayerCreatureKilledByPet(Player* owner, Creature* killed) override
    {
        Thrill(owner, killed);
    }

    static void Thrill(Player* player, Creature* killed)
    {
        uint8 rank = Rank(player, PERK_THRILL_OF_ADVENTURE);
        if (!rank || !killed || InInstance(player) || !Acore::XP::Gain(player, killed))
            return;
        PerkData* data = DataOf(player);
        data->thrillTicks = THRILL_TICKS;
        data->thrillTimer = THRILL_TICK_MS;
        data->thrillPerTick = rank * Perks()[PERK_THRILL_OF_ADVENTURE].perRank * 100 / THRILL_TICKS;
    }

    // The Quick and the Dead: the spell's cost goes back at the next update, after TakePower.
    void OnPlayerSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        if (!spell || spell->GetPowerCost() <= 0)
            return;
        PerkData* data = DataOf(player);
        if (data->freeUntil <= GameTime::GetGameTime() || player->IsInCombat())
            return;
        SpellInfo const* info = spell->GetSpellInfo();
        if (!info->IsPositive() || info->PowerType >= MAX_POWERS)
            return;
        if (!data->cost[info->PowerType])
            data->powerBefore[info->PowerType] = int32(player->GetPower(Powers(info->PowerType)));
        data->cost[info->PowerType] += uint32(spell->GetPowerCost());
    }

    // Diplomat
    void OnPlayerGiveReputation(Player* player, int32 /*factionID*/, float& amount, ReputationSource repSource) override
    {
        if (amount <= 0.0f || repSource == REPUTATION_SOURCE_SPELL)
            return;
        if (uint32 pct = Percent(player, PERK_DIPLOMAT))
            amount += amount * pct / 100.0f;
    }

    // For Great Honor
    void OnPlayerVictimRewardAfter(Player* player, Player* /*victim*/, uint32& /*killer_title*/, int32& /*victim_rank*/, float& honor_f) override
    {
        if (uint32 pct = Percent(player, PERK_FOR_GREAT_HONOR))
            honor_f += honor_f * pct / 100.0f;
    }

    // Working Overtime: one more roll for a skill-up. The core's own roll comes after this one,
    // so both can land.
    void OnPlayerUpdateCraftingSkill(Player* player, SkillLineAbilityEntry const* skill, uint32 currentLevel, uint32& gain) override
    {
        if (!gain || currentLevel >= skill->TrivialSkillLineRankHigh)
            return;
        if (uint32 pct = Percent(player, PERK_WORKING_OVERTIME))
            if (roll_chance_i(int32(pct)))
                player->UpdateSkillPro(skill->SkillLine, 1000, gain);
    }

    void OnPlayerUpdateGatheringSkill(Player* player, uint32 skillId, uint32 current, uint32 gray, uint32 /*green*/, uint32 /*yellow*/, uint32& gain) override
    {
        if (!gain || current >= gray || skillId == SKILL_LOCKPICKING)
            return;
        if (uint32 pct = Percent(player, PERK_WORKING_OVERTIME))
            if (roll_chance_i(int32(pct)))
                player->UpdateSkillPro(skillId, 1000, gain);
    }

    // Master Chef
    void OnPlayerCreateItem(Player* player, Item* item, uint32 /*count*/) override
    {
        ItemTemplate const* proto = item->GetTemplate();
        if (granting || proto->Class != ITEM_CLASS_CONSUMABLE || proto->SubClass != ITEM_SUBCLASS_FOOD || proto->HasFlag(ITEM_FLAG_CONJURED))
            return;
        uint32 pct = Percent(player, PERK_MASTER_CHEF);
        if (!pct || !IsCooking(player->GetCurrentSpell(CURRENT_GENERIC_SPELL)))
            return;
        if (roll_chance_i(int32(pct)))
            GrantExtra(player, item->GetEntry(), 1, "Master Chef");
    }

    // Luremaster, Bountiful Harvest, Salvager
    void OnPlayerLootItem(Player* player, Item* item, uint32 count, ObjectGuid lootguid) override
    {
        if (granting || !item || !GetConfig().enabled)
            return;
        ItemTemplate const* proto = item->GetTemplate();

        if (uint32 pct = Percent(player, PERK_LUREMASTER))
            if (IsFish(proto) && IsLuredFishing(player, lootguid) && roll_chance_i(int32(pct)))
                GrantExtra(player, item->GetEntry(), 1, "Luremaster");

        if (proto->Class != ITEM_CLASS_TRADE_GOODS && proto->Class != ITEM_CLASS_GEM)
            return;
        if (proto->StartQuest || proto->Bonding == BIND_QUEST_ITEM)
            return;

        if (uint32 pct = Percent(player, PERK_BOUNTIFUL_HARVEST))
            if (IsGathered(player, lootguid) && roll_chance_i(int32(pct)))
                GrantExtra(player, item->GetEntry(), count, "Bountiful Harvest");

        if (uint32 pct = Percent(player, PERK_SALVAGER))
            if (IsSalvaged(player, lootguid) && roll_chance_i(int32(pct)))
                GrantExtra(player, item->GetEntry(), 1, "Salvager");
    }

    // Bartering (vendors) and Frequent Flier (flight masters). Only the Creature overload: it
    // calls the FactionTemplate one first, so taking both would count twice.
    void OnPlayerGetReputationPriceDiscount(Player const* player, Creature const* creature, float& discount) override
    {
        if (!creature)
            return;
        if (creature->HasNpcFlag(UNIT_NPC_FLAG_VENDOR))
            if (uint32 pct = Percent(player, PERK_BARTERING))
                discount *= 1.0f - pct / 100.0f;
#ifndef LEGACY_CORE_HAS_TAXI_COST
        // Without the taxi cost hook only the flight master's own price, the first leg.
        if (creature->HasNpcFlag(UNIT_NPC_FLAG_FLIGHTMASTER))
            if (uint32 pct = Percent(player, PERK_FREQUENT_FLIER))
                discount *= 1.0f - pct / 100.0f;
#endif
    }

#ifdef LEGACY_CORE_HAS_TAXI_COST
    // Frequent Flier: every leg of the flight.
    void OnPlayerTaxiCostDiscount(Player* player, float& discount) override
    {
        if (uint32 pct = Percent(player, PERK_FREQUENT_FLIER))
            discount *= 1.0f - pct / 100.0f;
    }
#endif

    void OnPlayerDeleteFromDB(CharacterDatabaseTransaction trans, uint32 guid) override
    {
        // The challenges it did stay with the account.
        trans->Append("DELETE FROM mod_legacy_rank WHERE guid = {}", guid);
        trans->Append("DELETE FROM mod_legacy_character WHERE guid = {}", guid);
        ForgetCharacter(guid);
    }

    // Deep Pockets. At login the core equips the bags before it loads what's in them, so this
    // also gives a bag its extra slots in time for the items saved there.
    void OnPlayerEquip(Player* player, Item* item, uint8 /*bag*/, uint8 /*slot*/, bool /*update*/) override
    {
        if (item->IsBag() && !IsBotSession(player->GetSession()))
            UpdateBags(player);
    }

    void OnPlayerUnequip(Player* player, Item* item) override
    {
        if (item->IsBag() && !IsBotSession(player->GetSession()))
            UpdateBags(player);
    }

    // A bag with items in its extra slots stays in the four bag slots: in a bank bag slot it would
    // lose those slots at the next login, and the core would mail the items.
    bool OnPlayerCanUnequipItem(Player* player, uint16 pos, bool /*swap*/) override
    {
        if ((pos >> 8) != INVENTORY_SLOT_BAG_0 || (pos & 255) < INVENTORY_SLOT_BAG_START || (pos & 255) >= INVENTORY_SLOT_BAG_END)
            return true;
        Bag* bag = player->GetBagByPos(uint8(pos & 255));
        if (!bag || !HoldsExtra(bag))
            return true;
        ChatHandler(player->GetSession()).PSendSysMessage("|cffe6cc80Deep Pockets:|r take the items out of this bag's last {} slots before you move it.",
            bag->GetBagSize() - bag->GetTemplate()->ContainerSlots);
        return false;
    }

#ifdef LEGACY_CORE_HAS_TAXI_SPEED
    // Frequent Flier
    void OnPlayerTaxiFlightSpeed(Player* player, float& speed) override
    {
        if (Rank(player, PERK_FREQUENT_FLIER))
            speed *= 1.2f;
    }
#endif

#ifdef LEGACY_CORE_HAS_REST_BONUS
    // Well Rested
    void OnPlayerGainRestBonus(Player* player, float& bonus) override
    {
        if (uint32 pct = Percent(player, PERK_WELL_RESTED))
            bonus += bonus * pct / 100.0f;
    }

    void OnPlayerGetRestBonusMax(Player* player, float& restBonusMax) override
    {
        if (uint32 pct = Percent(player, PERK_WELL_RESTED))
            restBonusMax += restBonusMax * pct / 100.0f;
    }
#endif
};

class LegacyPerksUnitScript : public UnitScript
{
public:
    LegacyPerksUnitScript() : UnitScript("LegacyPerksUnitScript", true, { UNITHOOK_ON_DAMAGE }) { }

    // Reinforce: what everything was worth at the killing blow.
    void OnDamage(Unit* /*attacker*/, Unit* victim, uint32& damage) override
    {
        if (!victim || !victim->IsPlayer() || damage < victim->GetHealth() || !victim->IsAlive())
            return;
        Player* player = victim->ToPlayer();
        if (!Rank(player, PERK_REINFORCE))
            return;

        PerkData* data = DataOf(player);
        data->snapshot.clear();
        data->snapshotAt = GameTime::GetGameTime();
        ForEachItem(player, [data](Item* item)
        {
            if (item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY))
                data->snapshot.emplace_back(item->GetGUID(), item->GetUInt32Value(ITEM_FIELD_DURABILITY));
        });
    }
};

// Field Medicine, Gourmand, Permanence, Hasty Hearth
class LegacyPerksSpellScript : public AllSpellScript
{
public:
    LegacyPerksSpellScript() : AllSpellScript("LegacyPerksSpellScript", { ALLSPELLHOOK_ON_CALC_MAX_DURATION, ALLSPELLHOOK_ON_CAST }) { }

    // Runs after Spell::SendSpellCooldown, so the cooldowns are on the player already.
    void OnSpellCast(Spell* /*spell*/, Unit* caster, SpellInfo const* spellInfo, bool /*skipCheck*/) override
    {
        Player* player = caster ? caster->ToPlayer() : nullptr;
        if (!player || !spellInfo || !IsHearth(spellInfo->Id) || !player->GetSession() || IsBotSession(player->GetSession()))
            return;

        std::vector<uint32> spells = HearthCooldownSpells(player, spellInfo);
        if (uint8 rank = Rank(player, PERK_HASTY_HEARTH))
            ShortenHearth(player, spells, rank * Perks()[PERK_HASTY_HEARTH].perRank * MINUTE * IN_MILLISECONDS);
        SyncHearth(player, spells);

        // Again once the client has handled SPELL_GO, which starts its own Spell.dbc timer.
        ObjectGuid guid = player->GetGUID();
        player->m_Events.AddEventAtOffset([guid, spells]()
        {
            if (Player* p = ObjectAccessor::FindConnectedPlayer(guid))
                SyncHearth(p, spells);
        }, 150ms);
    }

    void OnCalcMaxDuration(Aura const* aura, int32& maxDuration) override
    {
        if (maxDuration <= 0 || !GetConfig().enabled)
            return;
        SpellInfo const* spell = aura->GetSpellInfo();

        if (Player* owner = aura->GetOwner() ? aura->GetOwner()->ToPlayer() : nullptr)
        {
            if (spell->Id == SPELL_RECENTLY_BANDAGED && !InInstance(owner))
                if (uint32 seconds = Percent(owner, PERK_FIELD_MEDICINE))
                    maxDuration = std::max<int32>(maxDuration - int32(seconds * IN_MILLISECONDS), IN_MILLISECONDS);

            if (IsWellFed(spell))
                if (uint8 rank = Rank(owner, PERK_GOURMAND))
                    maxDuration = int32(std::min<int64>(int64(maxDuration) * (100 + Thirds(rank)) / 100, INT32_MAX));
        }

        Unit* caster = aura->GetCaster();
        if (Player* player = caster ? caster->ToPlayer() : nullptr)
            if (uint32 pct = Percent(player, PERK_PERMANENCE))
                if (IsLongGroupBuff(spell, player, maxDuration))
                    maxDuration = int32(std::min<int64>(int64(maxDuration) * (100 + pct) / 100, INT32_MAX));
    }
};

void AddLegacyPerkScripts()
{
    new LegacyPerksPlayerScript();
    new LegacyPerksUnitScript();
    new LegacyPerksSpellScript();
}
