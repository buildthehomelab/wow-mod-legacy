/*
 * mod-legacy: the perk trees, every character's ranks, and the challenges that earn points.
 *
 * Released under the MIT License.
 */

#include "Legacy.h"
#include "AchievementMgr.h"
#include "Chat.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "GameTime.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ReputationMgr.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "StringConvert.h"
#include "Tokenize.h"
#include "WorldSession.h"
#include <algorithm>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>

namespace
{
    using namespace Legacy;

    Config config;

    std::array<TreeInfo, MAX_TREES> const TREES =
    {{
        { TREE_ADVENTURE,       "adventure",       "Adventure",       "inv_misc_map_01" },
        { TREE_RESOURCEFULNESS, "resourcefulness", "Resourcefulness", "inv_misc_bag_08" },
        { TREE_PROFESSIONS,     "professions",     "Professions",     "trade_engineering" },
    }};

    // Rows unlock at 0, 5 and 10 points in the rest of the tree, like WoW Forever's trees. Five of
    // Forever's perks don't fit 3.3.5 or this realm and are replaced (see the README).
    std::array<PerkInfo, MAX_PERKS> const PERKS =
    {{
        { PERK_HIGH_ALERT, "high_alert", TREE_ADVENTURE, "High Alert", "spell_shadow_detectinvisibility", 2, 0, 0, 0, NO_PERK, 5,
            "You see stealthed enemies from further away, as if you were higher level. Not in battlegrounds or arenas." },
        { PERK_WELL_RESTED, "well_rested", TREE_ADVENTURE, "Well Rested", "spell_nature_sleep", 5, 0, 1, 0, NO_PERK, 4,
            "Rested experience builds up faster, and you can store more of it." },
        { PERK_THRILL_OF_ADVENTURE, "thrill_of_adventure", TREE_ADVENTURE, "Thrill of Adventure", "ability_warrior_rampage", 5, 0, 2, 0, NO_PERK, 1,
            "Killing an enemy that gives experience (you or your pet) restores health and mana over 10 sec. Not in dungeons, raids or battlegrounds." },
        { PERK_FIELD_MEDICINE, "field_medicine", TREE_ADVENTURE, "Field Medicine", "inv_misc_bandage_15", 2, 1, 0, 5, NO_PERK, 5,
            "Recently Bandaged wears off sooner. Not in dungeons, raids or battlegrounds." },
        { PERK_EXPLORERS_STRIDE, "explorers_stride", TREE_ADVENTURE, "Explorer's Stride", "ability_rogue_sprint", 5, 1, 1, 5, PERK_WELL_RESTED, 2,
            "You run faster on foot while out of combat. Not in dungeons, raids or battlegrounds." },
        { PERK_SECOND_WIND, "second_wind", TREE_ADVENTURE, "Second Wind", "spell_holy_resurrection", 3, 1, 2, 5, NO_PERK, 0,
            "Resurrection Sickness wears off sooner." },
        { PERK_FREQUENT_FLIER, "frequent_flier", TREE_ADVENTURE, "Frequent Flier", "ability_mount_gryphon_01", 1, 2, 1, 10, NO_PERK, 50,
            "Flight paths cost less and fly faster." },
        { PERK_SUMMONING_STONE, "summoning_stone", TREE_ADVENTURE, "Summoning Stone", "spell_shadow_twilight", 1, 2, 0, 10, NO_PERK, 0,
            "Raid Finder's Summon to Stone takes you to the raid's meeting stone from anywhere." },

        { PERK_FOR_GREAT_HONOR, "for_great_honor", TREE_RESOURCEFULNESS, "For Great Honor", "inv_jewelry_trinketpvp_01", 5, 0, 0, 0, NO_PERK, 2,
            "You gain more honor from honorable kills." },
        { PERK_GOURMAND, "gourmand", TREE_RESOURCEFULNESS, "Gourmand", "spell_misc_food", 3, 0, 1, 0, NO_PERK, 0,
            "Well Fed lasts longer." },
        { PERK_DIPLOMAT, "diplomat", TREE_RESOURCEFULNESS, "Diplomat", "achievement_reputation_01", 5, 0, 2, 0, NO_PERK, 2,
            "You gain more reputation from killing monsters and completing quests." },
        { PERK_REINFORCE, "reinforce", TREE_RESOURCEFULNESS, "Reinforce", "trade_blacksmithing", 5, 1, 0, 5, NO_PERK, 8,
            "Your items lose less durability when you die." },
        { PERK_PERMANENCE, "permanence", TREE_RESOURCEFULNESS, "Permanence", "spell_holy_magicalsentry", 2, 1, 1, 5, PERK_GOURMAND, 25,
            "Your long class buffs (10 min or more) that you can cast on others last longer." },
        { PERK_QUICK_AND_DEAD, "the_quick_and_the_dead", TREE_RESOURCEFULNESS, "The Quick and the Dead", "spell_shadow_ghostkey", 2, 1, 2, 5, NO_PERK, 5,
            "You move faster as a ghost. At rank 2, your helpful spells also cost nothing for 1 min after you come back to life, until you enter combat. Not in battlegrounds or arenas." },
        { PERK_REAGENT_ECONOMY, "reagent_economy", TREE_RESOURCEFULNESS, "Reagent Economy", "inv_misc_bag_10", 1, 2, 1, 10, NO_PERK, 0,
            "Your class abilities no longer need reagents that vendors sell." },

        { PERK_WORKING_OVERTIME, "working_overtime", TREE_PROFESSIONS, "Working Overtime", "inv_misc_pocketwatch_01", 5, 0, 0, 0, NO_PERK, 4,
            "An extra chance of a skill-up when you craft or gather, while the recipe or node isn't gray." },
        { PERK_MASTER_CHEF, "master_chef", TREE_PROFESSIONS, "Master Chef", "inv_misc_food_15", 5, 0, 1, 0, NO_PERK, 10,
            "A chance to cook one more of whatever you make." },
        { PERK_LUREMASTER, "luremaster", TREE_PROFESSIONS, "Luremaster", "trade_fishing", 2, 0, 2, 0, NO_PERK, 25,
            "While your fishing pole has a lure, a chance to catch one more fish." },
        { PERK_BOUNTIFUL_HARVEST, "bountiful_harvest", TREE_PROFESSIONS, "Bountiful Harvest", "trade_herbalism", 5, 1, 0, 5, PERK_WORKING_OVERTIME, 4,
            "A chance to gather twice as much from Mining, Herbalism and Skinning." },
        { PERK_BARTERING, "bartering", TREE_PROFESSIONS, "Bartering", "inv_misc_coin_01", 2, 1, 1, 5, NO_PERK, 5,
            "Prices and repairs at vendors are lower." },
        { PERK_SALVAGER, "salvager", TREE_PROFESSIONS, "Salvager", "inv_enchant_disenchant", 3, 1, 2, 5, NO_PERK, 5,
            "A chance of one more material when you disenchant, prospect or mill." },
        { PERK_DEDICATED_STUDY, "dedicated_study", TREE_PROFESSIONS, "Dedicated Study", "inv_misc_book_09", 1, 2, 1, 10, NO_PERK, 1,
            "Once a day, your lowest profession (primary or secondary) that isn't at its maximum gains 1 skill." },
    }};

    // Second Wind and Gourmand go in thirds.
    uint32 Thirds(uint8 rank)
    {
        return rank >= 3 ? 100 : rank * 33 + (rank == 2 ? 1 : 0);
    }

    std::shared_mutex buildLock;
    std::unordered_map<uint32, Build> builds;
    std::unordered_set<uint32> suspended;

    // Challenges.
    enum ChallengeType : uint8
    {
        CHALLENGE_LEVEL = 1,
        CHALLENGE_SKILL = 2,
        CHALLENGE_ALL_ACHIEVEMENTS = 3,
        CHALLENGE_ANY_ACHIEVEMENT = 4,
        CHALLENGE_EXALTED = 5,
    };

    struct Challenge
    {
        uint32 id = 0;
        std::string category;
        std::string name;
        std::string description;
        uint8 type = 0;
        uint32 value1 = 0;
        uint32 value2 = 0;
        std::vector<uint32> list;
    };

    struct ProfessionDefinition
    {
        uint32 SkillId;
        std::string_view Name;
        std::array<uint32, 6> RankSpells;
    };

    constexpr std::array<ProfessionDefinition, 11> PrimaryProfessionDefinitions =
    {{
        { SKILL_ALCHEMY,       "alchemy",       { 2259, 3101, 3464, 11611, 28596, 51304 } },
        { SKILL_BLACKSMITHING, "blacksmithing", { 2018, 3100, 3538, 9785, 29844, 51300 } },
        { SKILL_ENCHANTING,   "enchanting",    { 7411, 7412, 7413, 13920, 28029, 51313 } },
        { SKILL_ENGINEERING,  "engineering",   { 4036, 4037, 4038, 12656, 30350, 51306 } },
        { SKILL_HERBALISM,    "herbalism",     { 2366, 2368, 3570, 11993, 28695, 50300 } },
        { SKILL_INSCRIPTION,  "inscription",   { 45357, 45358, 45359, 45360, 45361, 45363 } },
        { SKILL_JEWELCRAFTING,"jewelcrafting", { 25229, 25230, 28894, 28895, 28897, 51311 } },
        { SKILL_LEATHERWORKING,"leatherworking",{ 2108, 3104, 3811, 10662, 32549, 51302 } },
        { SKILL_MINING,       "mining",        { 2575, 2576, 3564, 10248, 29354, 50310 } },
        { SKILL_SKINNING,     "skinning",      { 8613, 8617, 8618, 10768, 32678, 50305 } },
        { SKILL_TAILORING,    "tailoring",     { 3908, 3909, 3910, 12180, 26790, 51309 } },
    }};

    constexpr std::array<ProfessionDefinition, 3> SecondaryProfessionDefinitions =
    {{
        { SKILL_COOKING,   "cooking",  { 2550, 3102, 3413, 18260, 33359, 51296 } },
        { SKILL_FIRST_AID, "firstaid", { 3273, 3274, 7924, 10846, 27028, 45542 } },
        { SKILL_FISHING,   "fishing",  { 7620, 7731, 7732, 18248, 33095, 51294 } },
    }};

    ProfessionDefinition const* GetProfessionDefinition(uint32 skillId)
    {
        for (ProfessionDefinition const& profession : PrimaryProfessionDefinitions)
            if (profession.SkillId == skillId)
                return &profession;

        for (ProfessionDefinition const& profession : SecondaryProfessionDefinitions)
            if (profession.SkillId == skillId)
                return &profession;

        return nullptr;
    }

    uint32 GetBestRankSpellForSkill(uint32 skillId, uint16 maxValue)
    {
        ProfessionDefinition const* profession = GetProfessionDefinition(skillId);
        if (!profession)
            return 0;

        uint32 rankIndex = 0;
        if (maxValue > 375)
            rankIndex = 5;
        else if (maxValue > 300)
            rankIndex = 4;
        else if (maxValue > 225)
            rankIndex = 3;
        else if (maxValue > 150)
            rankIndex = 2;
        else if (maxValue > 75)
            rankIndex = 1;

        return profession->RankSpells[rankIndex];
    }

    bool IsProfessionRankSpell(uint32 spellId)
    {
        for (auto const& profession : PrimaryProfessionDefinitions)
            if (std::find(profession.RankSpells.begin(), profession.RankSpells.end(), spellId) != profession.RankSpells.end())
                return true;
        for (auto const& profession : SecondaryProfessionDefinitions)
            if (std::find(profession.RankSpells.begin(), profession.RankSpells.end(), spellId) != profession.RankSpells.end())
                return true;
        return false;
    }

    // Protected by challengeLock together with the loaded challenge definitions.
    std::unordered_map<uint32, uint8> ProfessionTrainerLevels;

    std::unordered_map<uint32, uint8> LoadProfessionTrainerLevels()
    {
        std::unordered_map<uint32, uint8> levels;
        if (QueryResult result = WorldDatabase.Query("SELECT SpellId, MIN(ReqLevel) FROM trainer_spell GROUP BY SpellId"))
        {
            auto record = [&levels](uint32 spellId, uint8 level)
            {
                if (!IsProfessionRankSpell(spellId))
                    return;
                auto [itr, inserted] = levels.emplace(spellId, level);
                if (!inserted)
                    itr->second = std::min(itr->second, level);
            };
            do
            {
                Field* fields = result->Fetch();
                uint32 spellId = fields[0].Get<uint32>();
                uint8 level = fields[1].Get<uint8>();
                record(spellId, level);
                // Trainers may cast a wrapper which teaches the actual rank spell.
                if (SpellInfo const* spell = sSpellMgr->GetSpellInfo(spellId))
                    for (SpellEffectInfo const& effect : spell->Effects)
                        if (effect.Effect == SPELL_EFFECT_LEARN_SPELL)
                            record(effect.TriggerSpell, level);
            } while (result->NextRow());
        }
        return levels;
    }

    bool MeetsHardcoreProfessionLevel(Challenge const& challenge, uint8 level)
    {
        if (challenge.type != CHALLENGE_SKILL || !GetProfessionDefinition(challenge.value1))
            return true;
        if (challenge.value2 > 450)
            return false;
        uint32 spellId = GetBestRankSpellForSkill(challenge.value1, uint16(challenge.value2));
        auto itr = ProfessionTrainerLevels.find(spellId);
        return itr != ProfessionTrainerLevels.end() && level >= itr->second;
    }

    struct Done
    {
        uint32 guid = 0;
        uint32 at = 0;
        uint32 hardcoreGuid = 0;
        uint32 hardcoreAt = 0;
    };

    std::shared_mutex challengeLock;
    std::vector<Challenge> challenges;

    std::mutex doneLock;
    std::unordered_map<uint32, std::unordered_map<uint32, Done>> doneByAccount;

    bool hardcoreTable = false;

    // What a character has, for checking challenges: from a Player or from the database.
    struct Progress
    {
        uint8 race = 0;
        uint8 cls = 0;
        uint8 level = 0;
        std::unordered_map<uint32, uint32> skills;
        std::unordered_set<uint32> achievements;
        std::unordered_map<uint32, int32> standing;
    };

    constexpr int32 EXALTED = 42000;

    // ReputationMgr::GetBaseReputation for a character that isn't loaded: the database keeps
    // standing above this base.
    int32 BaseReputation(FactionEntry const* entry, uint8 race, uint8 cls)
    {
        uint32 raceMask = race ? 1 << (race - 1) : 0;
        uint32 classMask = cls ? 1 << (cls - 1) : 0;
        for (uint8 i = 0; i < 4; ++i)
            if ((entry->BaseRepRaceMask[i] & raceMask || (entry->BaseRepRaceMask[i] == 0 && entry->BaseRepClassMask[i] != 0))
                && (entry->BaseRepClassMask[i] & classMask || entry->BaseRepClassMask[i] == 0))
                return entry->BaseRepValue[i];
        return 0;
    }

    bool Meets(Challenge const& challenge, Progress const& progress)
    {
        switch (challenge.type)
        {
            case CHALLENGE_LEVEL:
                return progress.cls == challenge.value1 && progress.level >= challenge.value2;
            case CHALLENGE_SKILL:
            {
                auto itr = progress.skills.find(challenge.value1);
                return itr != progress.skills.end() && itr->second >= challenge.value2;
            }
            case CHALLENGE_ALL_ACHIEVEMENTS:
                if (challenge.list.empty())
                    return false;
                for (uint32 id : challenge.list)
                    if (!progress.achievements.count(id))
                        return false;
                return true;
            case CHALLENGE_ANY_ACHIEVEMENT:
                for (uint32 id : challenge.list)
                    if (progress.achievements.count(id))
                        return true;
                return false;
            case CHALLENGE_EXALTED:
                for (uint32 id : challenge.list)
                {
                    auto itr = progress.standing.find(id);
                    if (itr != progress.standing.end() && itr->second >= EXALTED)
                        return true;
                }
                return false;
            default:
                return false;
        }
    }

    // The achievements, skills and factions any challenge looks at, so a player check only asks
    // about those.
    struct Watched
    {
        std::unordered_set<uint32> skills;
        std::unordered_set<uint32> achievements;
        std::unordered_set<uint32> factions;
    };

    Watched watched;

    struct Fresh
    {
        Challenge challenge;
        bool newForAccount = false;     // false: only new for the account's hardcore points
    };

    // Records what `guid` did. Returns the challenges that were new for the account (or new for
    // its hardcore points).
    std::vector<Fresh> Record(uint32 account, uint32 guid, bool hardcore, uint8 level, std::vector<Challenge const*> const& met)
    {
        std::vector<Fresh> fresh;
        uint32 now = uint32(GameTime::GetGameTime().count());
        CharacterDatabaseTransaction trans;

        std::lock_guard<std::mutex> guard(doneLock);
        auto& done = doneByAccount[account];
        for (Challenge const* challenge : met)
        {
            auto itr = done.find(challenge->id);
            bool isNew = itr == done.end();
            bool newHardcore = hardcore && MeetsHardcoreProfessionLevel(*challenge, level)
                && (isNew || !itr->second.hardcoreGuid);
            if (!isNew && !newHardcore)
                continue;

            Done& row = done[challenge->id];
            if (isNew)
            {
                row.guid = guid;
                row.at = now;
            }
            if (newHardcore)
            {
                row.hardcoreGuid = guid;
                row.hardcoreAt = now;
            }

            if (!trans)
                trans = CharacterDatabase.BeginTransaction();
            trans->Append("INSERT INTO mod_legacy_challenge_done (account, challenge, guid, done_at, hardcore_guid, hardcore_at) "
                "VALUES ({}, {}, {}, {}, {}, {}) ON DUPLICATE KEY UPDATE "
                "hardcore_guid = IF(hardcore_guid = 0, VALUES(hardcore_guid), hardcore_guid), "
                "hardcore_at = IF(hardcore_at = 0, VALUES(hardcore_at), hardcore_at)",
                account, challenge->id, row.guid, row.at, row.hardcoreGuid, row.hardcoreAt);
            fresh.push_back({ *challenge, isNew });
        }

        if (trans)
            CharacterDatabase.CommitTransaction(trans);
        return fresh;
    }

    std::vector<Challenge const*> Unmet(uint32 account, bool hardcore, Progress const& progress)
    {
        std::vector<Challenge const*> met;
        std::lock_guard<std::mutex> guard(doneLock);
        auto accountItr = doneByAccount.find(account);
        for (Challenge const& challenge : challenges)
        {
            if (accountItr != doneByAccount.end())
            {
                auto itr = accountItr->second.find(challenge.id);
                if (itr != accountItr->second.end() && (!hardcore || itr->second.hardcoreGuid))
                    continue;
            }
            if (Meets(challenge, progress))
                met.push_back(&challenge);
        }
        return met;
    }

    std::vector<uint32> ParseList(std::string_view text)
    {
        std::vector<uint32> ids;
        for (std::string_view token : Acore::Tokenize(text, ',', false))
            if (Optional<uint32> id = Acore::StringTo<uint32>(token))
                ids.push_back(*id);
        return ids;
    }

    bool TableExists(char const* table)
    {
        return CharacterDatabase.Query("SELECT 1 FROM information_schema.tables WHERE table_schema = DATABASE() AND table_name = '{}'", table) != nullptr;
    }
}

namespace Legacy
{
    std::array<TreeInfo, MAX_TREES> const& Trees() { return TREES; }
    std::array<PerkInfo, MAX_PERKS> const& Perks() { return PERKS; }
    Config const& GetConfig() { return config; }

    PerkInfo const* FindPerk(std::string_view key)
    {
        for (PerkInfo const& perk : PERKS)
            if (key == perk.key)
                return &perk;
        return nullptr;
    }

    std::string RankText(PerkInfo const& perk, uint8 rank)
    {
        uint32 value = perk.perRank * rank;
        switch (perk.perk)
        {
            case PERK_HIGH_ALERT:
                return Acore::StringFormat("You see stealthed enemies as if you were {} level{} higher. Not in battlegrounds or arenas.", rank, rank > 1 ? "s" : "");
            case PERK_WELL_RESTED:
                return Acore::StringFormat("Rested experience builds up {}% faster, and you can store {}% more of it.", value, value);
            case PERK_THRILL_OF_ADVENTURE:
                return Acore::StringFormat("Killing an enemy that gives experience (you or your pet) restores {}% of your health and mana over 10 sec. Not in dungeons, raids or battlegrounds.", value);
            case PERK_FIELD_MEDICINE:
                return Acore::StringFormat("Recently Bandaged lasts {} sec less. Not in dungeons, raids or battlegrounds.", value);
            case PERK_EXPLORERS_STRIDE:
                return Acore::StringFormat("You run {}% faster on foot while out of combat. Not in dungeons, raids or battlegrounds.", value);
            case PERK_SECOND_WIND:
                return rank >= 3 ? std::string("You no longer get Resurrection Sickness.")
                    : Acore::StringFormat("Resurrection Sickness wears off {}% sooner.", Thirds(rank));
            case PERK_FREQUENT_FLIER:
                return "Flight paths cost 50% less, and you fly 20% faster.";
            case PERK_SUMMONING_STONE:
                return "Raid Finder's Summon to Stone takes you to the raid's meeting stone from anywhere. Without it, you travel there yourself; the stone still gathers your raid's bots.";
            case PERK_FOR_GREAT_HONOR:
                return Acore::StringFormat("You gain {}% more honor from honorable kills.", value);
            case PERK_GOURMAND:
                return Acore::StringFormat("Well Fed lasts {}% longer.", Thirds(rank));
            case PERK_DIPLOMAT:
                return Acore::StringFormat("You gain {}% more reputation from killing monsters and completing quests.", value);
            case PERK_REINFORCE:
                return Acore::StringFormat("Your items lose {}% less durability when you die.", value);
            case PERK_PERMANENCE:
                return Acore::StringFormat("Your class buffs of 10 min or more that you can cast on others last {}% longer.", value);
            case PERK_QUICK_AND_DEAD:
                return rank >= 2
                    ? Acore::StringFormat("You move {}% faster as a ghost, and your helpful spells cost nothing for 1 min after you come back to life, until you enter combat. Not in battlegrounds or arenas.", value)
                    : Acore::StringFormat("You move {}% faster as a ghost. Not in battlegrounds or arenas.", value);
            case PERK_REAGENT_ECONOMY:
                return "Your class abilities no longer need reagents that vendors sell.";
            case PERK_WORKING_OVERTIME:
                return Acore::StringFormat("An extra {}% chance of a skill-up when you craft or gather, while the recipe or node isn't gray.", value);
            case PERK_MASTER_CHEF:
                return Acore::StringFormat("A {}% chance to cook one more of whatever you make.", value);
            case PERK_LUREMASTER:
                return Acore::StringFormat("While your fishing pole has a lure, a {}% chance to catch one more fish.", value);
            case PERK_BOUNTIFUL_HARVEST:
                return Acore::StringFormat("A {}% chance to gather twice as much from Mining, Herbalism and Skinning.", value);
            case PERK_BARTERING:
                return Acore::StringFormat("Prices and repairs at vendors are {}% lower.", value);
            case PERK_SALVAGER:
                return Acore::StringFormat("A {}% chance of one more material when you disenchant, prospect or mill.", value);
            case PERK_DEDICATED_STUDY:
                return "Once a day, your lowest profession (primary or secondary) that isn't at its maximum gains 1 skill. Once all of them are, it does nothing.";
            default:
                return perk.description;
        }
    }

    Build GetBuild(uint32 guid)
    {
        std::shared_lock<std::shared_mutex> guard(buildLock);
        auto itr = builds.find(guid);
        return itr != builds.end() ? itr->second : Build{};
    }

    uint8 Rank(Player const* player, Perk perk)
    {
        if (!config.enabled || !player)
            return 0;
        uint32 guid = player->GetGUID().GetCounter();
        std::shared_lock<std::shared_mutex> guard(buildLock);
        auto itr = builds.find(guid);
        return itr != builds.end() && !suspended.count(guid) ? itr->second[perk] : 0;
    }

    void SetBuild(uint32 guid, Build const& build)
    {
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        trans->Append("DELETE FROM mod_legacy_rank WHERE guid = {}", guid);
        for (PerkInfo const& perk : PERKS)
            if (build[perk.perk])
                trans->Append("INSERT INTO mod_legacy_rank (guid, perk, `rank`) VALUES ({}, '{}', {})", guid, perk.key, build[perk.perk]);
        CharacterDatabase.CommitTransaction(trans);

        std::unique_lock<std::shared_mutex> guard(buildLock);
        suspended.erase(guid);
        if (Spent(build))
            builds[guid] = build;
        else
            builds.erase(guid);
    }

    void SuspendBuild(uint32 guid)
    {
        std::unique_lock<std::shared_mutex> guard(buildLock);
        suspended.insert(guid);
    }

    bool IsSuspended(uint32 guid)
    {
        std::shared_lock<std::shared_mutex> guard(buildLock);
        return suspended.count(guid);
    }

    void ForgetCharacter(uint32 guid)
    {
        std::unique_lock<std::shared_mutex> guard(buildLock);
        builds.erase(guid);
        suspended.erase(guid);
    }

    uint32 Spent(Build const& build)
    {
        uint32 spent = 0;
        for (uint8 rank : build)
            spent += rank;
        return spent;
    }

    uint32 SpentInTree(Build const& build, Tree tree)
    {
        uint32 spent = 0;
        for (PerkInfo const& perk : PERKS)
            if (perk.tree == tree)
                spent += build[perk.perk];
        return spent;
    }

    std::pair<std::string, std::string> Validate(Build const& build, uint32 points)
    {
        for (PerkInfo const& perk : PERKS)
        {
            uint8 rank = build[perk.perk];
            if (!rank)
                continue;
            if (rank > perk.maxRank)
                return { "invalid", Acore::StringFormat("{} has only {} rank{}.", perk.name, perk.maxRank, perk.maxRank > 1 ? "s" : "") };
            if (SpentInTree(build, perk.tree) - rank < perk.requiresTreePoints)
                return { "locked", Acore::StringFormat("{} needs {} points in {} first.", perk.name, perk.requiresTreePoints, TREES[perk.tree].name) };
            if (perk.requiresPerk != NO_PERK && build[perk.requiresPerk] < PERKS[perk.requiresPerk].maxRank)
                return { "locked", Acore::StringFormat("{} needs every rank of {} first.", perk.name, PERKS[perk.requiresPerk].name) };
        }

        uint32 spent = Spent(build);
        if (spent > points)
            return { "points", Acore::StringFormat("That needs {} Legacy points; this character has {}.", spent, points) };
        return {};
    }

    Earned GetEarned(uint32 account)
    {
        Earned earned;
        std::shared_lock<std::shared_mutex> challengeGuard(challengeLock);
        std::lock_guard<std::mutex> guard(doneLock);
        auto accountItr = doneByAccount.find(account);
        if (accountItr == doneByAccount.end())
            return earned;
        // Only challenges that still count: a row switched off takes its point with it.
        for (Challenge const& challenge : challenges)
        {
            auto itr = accountItr->second.find(challenge.id);
            if (itr == accountItr->second.end())
                continue;
            ++earned.total;
            if (itr->second.hardcoreGuid)
                ++earned.hardcore;
        }
        return earned;
    }

    uint32 SpendablePoints(Earned const& earned, bool hardcore)
    {
        uint32 points = hardcore && config.hardcorePool ? earned.hardcore : earned.total;
        return std::min(points, config.pointCap);
    }

    bool HardcoreTracked()
    {
        return hardcoreTable;
    }

    // Hardcore = mod-challenges' Hardcore still running (bit 1, state 0). Asked from the
    // database each time: it changes when the character dies, and this is only needed when a
    // challenge is done or the build is checked.
    bool IsHardcore(uint32 guid)
    {
        if (!hardcoreTable || !config.hardcorePool)
            return false;
        return CharacterDatabase.Query("SELECT 1 FROM mod_challenges_character WHERE guid = {} AND (challenge & 1) AND state = 0", guid) != nullptr;
    }

    size_t ChallengeCount()
    {
        std::shared_lock<std::shared_mutex> guard(challengeLock);
        return challenges.size();
    }

    void CheckChallenges(Player* player, bool hardcore)
    {
        if (!config.enabled || !player->GetSession() || IsBotSession(player->GetSession()))
            return;

        Progress progress;
        progress.cls = player->getClass();
        progress.level = player->GetLevel();
        for (uint32 skill : watched.skills)
            if (player->HasSkill(skill))
                progress.skills[skill] = player->GetPureSkillValue(skill);
        for (uint32 achievement : watched.achievements)
            if (player->HasAchieved(achievement))
                progress.achievements.insert(achievement);
        for (uint32 faction : watched.factions)
            if (FactionEntry const* entry = sFactionStore.LookupEntry(faction))
                progress.standing[faction] = player->GetReputationMgr().GetReputation(entry);

        uint32 account = player->GetSession()->GetAccountId();
        uint32 guid = player->GetGUID().GetCounter();

        hardcore = hardcore && hardcoreTable && config.hardcorePool;
        std::vector<Fresh> fresh;
        {
            std::shared_lock<std::shared_mutex> guard(challengeLock);
            std::vector<Challenge const*> met = Unmet(account, hardcore, progress);
            if (met.empty())
                return;
            fresh = Record(account, guid, hardcore, progress.level, met);
        }

        if (fresh.empty() || !config.announce)
            return;

        Earned earned = GetEarned(account);
        ChatHandler chat(player->GetSession());
        for (Fresh const& done : fresh)
        {
            if (done.newForAccount)
                chat.PSendSysMessage("|cffe6cc80Legacy:|r {} complete. +1 Legacy point ({} earned on your account).", done.challenge.name, earned.total);
            else
                chat.PSendSysMessage("|cffe6cc80Legacy:|r {} complete on a hardcore character. +1 hardcore Legacy point ({} earned by hardcore characters).",
                    done.challenge.name, earned.hardcore);
        }
    }

    void BackfillAccount(uint32 account)
    {
        if (!config.enabled)
            return;

        std::map<uint32, Progress> characters;
        if (QueryResult result = CharacterDatabase.Query("SELECT guid, race, class, level FROM characters WHERE account = {}", account))
        {
            do
            {
                Field* fields = result->Fetch();
                Progress& progress = characters[fields[0].Get<uint32>()];
                progress.race = fields[1].Get<uint8>();
                progress.cls = fields[2].Get<uint8>();
                progress.level = fields[3].Get<uint8>();
            } while (result->NextRow());
        }
        if (characters.empty())
            return;

        std::string guids;
        for (auto const& [guid, progress] : characters)
            guids += (guids.empty() ? "" : ",") + std::to_string(guid);

        std::shared_lock<std::shared_mutex> guard(challengeLock);
        auto inList = [](std::unordered_set<uint32> const& ids)
        {
            std::string list;
            for (uint32 id : ids)
                list += (list.empty() ? "" : ",") + std::to_string(id);
            return list;
        };

        if (!watched.skills.empty())
            if (QueryResult result = CharacterDatabase.Query("SELECT guid, skill, value FROM character_skills WHERE guid IN ({}) AND skill IN ({})", guids, inList(watched.skills)))
                do
                {
                    Field* fields = result->Fetch();
                    characters[fields[0].Get<uint32>()].skills[fields[1].Get<uint16>()] = fields[2].Get<uint16>();
                } while (result->NextRow());

        if (!watched.achievements.empty())
            if (QueryResult result = CharacterDatabase.Query("SELECT guid, achievement FROM character_achievement WHERE guid IN ({}) AND achievement IN ({})", guids, inList(watched.achievements)))
                do
                {
                    Field* fields = result->Fetch();
                    characters[fields[0].Get<uint32>()].achievements.insert(fields[1].Get<uint16>());
                } while (result->NextRow());

        if (!watched.factions.empty())
            if (QueryResult result = CharacterDatabase.Query("SELECT guid, faction, standing FROM character_reputation WHERE guid IN ({}) AND faction IN ({})", guids, inList(watched.factions)))
                do
                {
                    Field* fields = result->Fetch();
                    Progress& progress = characters[fields[0].Get<uint32>()];
                    uint32 faction = fields[1].Get<uint16>();
                    int32 standing = fields[2].Get<int32>();
                    if (FactionEntry const* entry = sFactionStore.LookupEntry(faction))
                        standing += BaseReputation(entry, progress.race, progress.cls);
                    progress.standing[faction] = standing;
                } while (result->NextRow());

        std::unordered_set<uint32> hardcore;
        if (hardcoreTable && config.hardcorePool)
            if (QueryResult result = CharacterDatabase.Query("SELECT guid FROM mod_challenges_character WHERE guid IN ({}) AND (challenge & 1) AND state = 0", guids))
                do
                {
                    hardcore.insert(result->Fetch()[0].Get<uint32>());
                } while (result->NextRow());

        for (auto const& [guid, progress] : characters)
        {
            // An online character is checked from its own map update; its saved data may be older.
            if (ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(guid)))
                continue;
            bool isHardcore = hardcore.count(guid);
            std::vector<Challenge const*> met = Unmet(account, isHardcore, progress);
            if (!met.empty())
                Record(account, guid, isHardcore, progress.level, met);
        }
    }

    void LoadChallenges()
    {
        auto trainerLevels = LoadProfessionTrainerLevels();
        std::vector<Challenge> loaded;
        Watched watching;
        uint32 skipped = 0;
        if (QueryResult result = WorldDatabase.Query("SELECT id, category, name, description, type, value1, value2, list "
            "FROM mod_legacy_challenge WHERE enabled = 1 ORDER BY sort, id"))
        {
            do
            {
                Field* fields = result->Fetch();
                Challenge challenge;
                challenge.id = fields[0].Get<uint16>();
                challenge.category = fields[1].Get<std::string>();
                challenge.name = fields[2].Get<std::string>();
                challenge.description = fields[3].Get<std::string>();
                challenge.type = fields[4].Get<uint8>();
                challenge.value1 = fields[5].Get<uint32>();
                challenge.value2 = fields[6].Get<uint32>();
                challenge.list = ParseList(fields[7].Get<std::string>());

                bool ok = true;
                switch (challenge.type)
                {
                    case CHALLENGE_LEVEL:
                        ok = sChrClassesStore.LookupEntry(challenge.value1) && challenge.value2;
                        break;
                    case CHALLENGE_SKILL:
                        ok = sSkillLineStore.LookupEntry(challenge.value1) && challenge.value2;
                        if (ok)
                            watching.skills.insert(challenge.value1);
                        break;
                    case CHALLENGE_ALL_ACHIEVEMENTS:
                    case CHALLENGE_ANY_ACHIEVEMENT:
                        ok = !challenge.list.empty();
                        for (uint32 id : challenge.list)
                        {
                            if (!sAchievementStore.LookupEntry(id))
                                ok = false;
                            watching.achievements.insert(id);
                        }
                        break;
                    case CHALLENGE_EXALTED:
                        ok = !challenge.list.empty();
                        for (uint32 id : challenge.list)
                        {
                            if (!sFactionStore.LookupEntry(id))
                                ok = false;
                            watching.factions.insert(id);
                        }
                        break;
                    default:
                        ok = false;
                }

                if (!ok)
                {
                    LOG_ERROR("module", "mod-legacy: challenge {} ({}) has an unknown type, class, skill, achievement or faction; skipped.", challenge.id, challenge.name);
                    ++skipped;
                    continue;
                }
                loaded.push_back(std::move(challenge));
            } while (result->NextRow());
        }

        std::unique_lock<std::shared_mutex> guard(challengeLock);
        ProfessionTrainerLevels = std::move(trainerLevels);
        challenges = std::move(loaded);
        watched = std::move(watching);
        LOG_INFO("module", "mod-legacy: {} challenges loaded ({} skipped).", challenges.size(), skipped);
    }

    void LoadRanks()
    {
        std::unordered_map<uint32, Build> loaded;
        if (QueryResult result = CharacterDatabase.Query("SELECT guid, perk, `rank` FROM mod_legacy_rank"))
        {
            do
            {
                Field* fields = result->Fetch();
                PerkInfo const* perk = FindPerk(fields[1].Get<std::string>());
                if (!perk)
                    continue;
                loaded[fields[0].Get<uint32>()][perk->perk] = std::min(fields[2].Get<uint8>(), perk->maxRank);
            } while (result->NextRow());
        }

        std::unique_lock<std::shared_mutex> guard(buildLock);
        builds = std::move(loaded);
        LOG_INFO("module", "mod-legacy: perks loaded for {} characters.", builds.size());
    }

    void LoadDone()
    {
        std::unordered_map<uint32, std::unordered_map<uint32, Done>> loaded;
        if (QueryResult result = CharacterDatabase.Query("SELECT account, challenge, guid, done_at, hardcore_guid, hardcore_at FROM mod_legacy_challenge_done"))
        {
            do
            {
                Field* fields = result->Fetch();
                Done& done = loaded[fields[0].Get<uint32>()][fields[1].Get<uint16>()];
                done.guid = fields[2].Get<uint32>();
                done.at = fields[3].Get<uint32>();
                done.hardcoreGuid = fields[4].Get<uint32>();
                done.hardcoreAt = fields[5].Get<uint32>();
            } while (result->NextRow());
        }

        hardcoreTable = TableExists("mod_challenges_character");

        std::lock_guard<std::mutex> guard(doneLock);
        doneByAccount = std::move(loaded);
        LOG_INFO("module", "mod-legacy: challenges done loaded for {} accounts; hardcore points {}.", doneByAccount.size(),
            hardcoreTable ? "tracked (mod-challenges found)" : "not tracked (no mod-challenges)");
    }

    std::string JsonString(std::string_view text)
    {
        std::string out = "\"";
        for (char c : text)
        {
            switch (c)
            {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20)
                        out += Acore::StringFormat("\\u{:04x}", c);
                    else
                        out += c;
            }
        }
        return out + "\"";
    }

    std::string ChallengesJson(uint32 account)
    {
        std::unordered_map<uint32, Done> done;
        {
            std::lock_guard<std::mutex> guard(doneLock);
            auto itr = doneByAccount.find(account);
            if (itr != doneByAccount.end())
                done = itr->second;
        }

        std::unordered_map<uint32, std::string> names;
        {
            std::string guids;
            for (auto const& [id, row] : done)
                guids += (guids.empty() ? "" : ",") + std::to_string(row.guid);
            if (!guids.empty())
                if (QueryResult result = CharacterDatabase.Query("SELECT guid, name FROM characters WHERE guid IN ({})", guids))
                    do
                    {
                        names[result->Fetch()[0].Get<uint32>()] = result->Fetch()[1].Get<std::string>();
                    } while (result->NextRow());
        }

        std::string json = "[";
        std::shared_lock<std::shared_mutex> guard(challengeLock);
        for (size_t i = 0; i < challenges.size(); ++i)
        {
            Challenge const& c = challenges[i];
            auto itr = done.find(c.id);
            bool isDone = itr != done.end();
            std::string by;
            if (isDone)
            {
                auto name = names.find(itr->second.guid);
                // A deleted character keeps its points for the account but has no name (an
                // unlinked one keeps its row with the name cleared).
                by = name != names.end() && !name->second.empty() ? name->second : "a deleted character";
            }
            json += Acore::StringFormat("{}{{\"id\":{},\"category\":{},\"name\":{},\"description\":{},\"done\":{},\"hardcore\":{},\"by\":{},\"at\":{}}}",
                i ? "," : "", c.id, JsonString(c.category), JsonString(c.name), JsonString(c.description),
                isDone ? "true" : "false", isDone && itr->second.hardcoreGuid ? "true" : "false",
                JsonString(by), isDone ? itr->second.at : 0);
        }
        return json + "]";
    }
}

class LegacyWorldScript : public WorldScript
{
public:
    LegacyWorldScript() : WorldScript("LegacyWorldScript", { WORLDHOOK_ON_AFTER_CONFIG_LOAD, WORLDHOOK_ON_STARTUP }) { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        config.enabled = sConfigMgr->GetOption<bool>("Legacy.Enable", true);
        config.pointCap = sConfigMgr->GetOption<uint32>("Legacy.PointCap", 16);
        config.hardcorePool = sConfigMgr->GetOption<bool>("Legacy.Hardcore.SeparatePoints", true);
        config.announce = sConfigMgr->GetOption<bool>("Legacy.AnnounceChallenges", true);
    }

    void OnStartup() override
    {
        LoadChallenges();
        LoadRanks();
        LoadDone();
        LoadReagentMasks();
    }
};

void AddLegacyDataScripts()
{
    new LegacyWorldScript();
}
