/*
 * mod-legacy: the commands.
 *
 * `.legacy list <account>` and `.legacy set <account> <guid> <ranks>` are for the portal, which
 * runs them over SOAP for the launcher's LEGACY page. Each answers one line of JSON. They are
 * SEC_CONSOLE: no account in the game is that high, so only the console and SOAP run them.
 * `.legacy show` tells a player their points and perks; `.legacy reload` reloads the challenges;
 * `.legacy bagprobe` checks the client draws Deep Pockets' extra bag slots.
 *
 * Released under the MIT License.
 */

#include "Legacy.h"
#include "Bag.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "DatabaseEnv.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "StringConvert.h"
#include "Tokenize.h"
#include "WorldSession.h"
#include <algorithm>
#include <unordered_set>

using namespace Acore::ChatCommands;
using namespace Legacy;

namespace
{
    struct Character
    {
        uint32 guid = 0;
        std::string name;
        uint8 race = 0;
        uint8 cls = 0;
        uint8 gender = 0;
        uint8 level = 0;
        Player* online = nullptr;
        bool hardcore = false;
    };

    std::vector<Character> LoadCharacters(uint32 account, uint32 onlyGuid = 0)
    {
        std::vector<Character> characters;
        QueryResult result = onlyGuid
            ? CharacterDatabase.Query("SELECT guid, name, race, class, gender, level FROM characters WHERE account = {} AND guid = {}", account, onlyGuid)
            : CharacterDatabase.Query("SELECT guid, name, race, class, gender, level FROM characters WHERE account = {} ORDER BY guid", account);
        if (result)
        {
            do
            {
                Field* fields = result->Fetch();
                Character character;
                character.guid = fields[0].Get<uint32>();
                character.name = fields[1].Get<std::string>();
                character.race = fields[2].Get<uint8>();
                character.cls = fields[3].Get<uint8>();
                character.gender = fields[4].Get<uint8>();
                character.level = fields[5].Get<uint8>();
                character.online = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(character.guid));
                if (character.online)
                    character.level = character.online->GetLevel();
                characters.push_back(std::move(character));
            } while (result->NextRow());
        }

        if (HardcoreTracked() && GetConfig().hardcorePool && !characters.empty())
        {
            std::string guids;
            for (Character const& character : characters)
                guids += (guids.empty() ? "" : ",") + std::to_string(character.guid);
            std::unordered_set<uint32> hardcore;
            if (QueryResult hc = CharacterDatabase.Query("SELECT guid FROM mod_challenges_character WHERE guid IN ({}) AND (challenge & 1) AND state = 0", guids))
                do
                {
                    hardcore.insert(hc->Fetch()[0].Get<uint32>());
                } while (hc->NextRow());
            for (Character& character : characters)
                character.hardcore = hardcore.count(character.guid);
        }
        return characters;
    }

    std::string CharacterJson(Character const& c, Earned const& earned)
    {
        Build build = GetBuild(c.guid);
        std::string ranks;
        for (PerkInfo const& perk : Perks())
            if (build[perk.perk])
                ranks += Acore::StringFormat("{}\"{}\":{}", ranks.empty() ? "" : ",", perk.key, build[perk.perk]);
        return Acore::StringFormat("{{\"guid\":{},\"name\":{},\"race\":{},\"class\":{},\"gender\":{},\"level\":{},\"online\":{},"
            "\"hardcore\":{},\"points\":{},\"spent\":{},\"ranks\":{{{}}}}}",
            c.guid, JsonString(c.name), c.race, c.cls, c.gender, c.level, c.online ? "true" : "false",
            c.hardcore ? "true" : "false", SpendablePoints(earned, c.hardcore), Spent(build), ranks);
    }

    // The General tree is a "strip": one row drawn above the three trees. A tree with no perk
    // offered (Deep Pockets switched off) isn't listed.
    std::string TreesJson()
    {
        std::string json = "[";
        for (TreeInfo const& tree : Trees())
        {
            if (std::none_of(Perks().begin(), Perks().end(), [&](PerkInfo const& perk) { return perk.tree == tree.tree && Offered(perk); }))
                continue;
            json += Acore::StringFormat("{}{{\"id\":\"{}\",\"name\":{},\"icon\":\"{}\",\"strip\":{},\"perks\":[", json.size() > 1 ? "," : "",
                tree.key, JsonString(tree.name), tree.icon, tree.tree == TREE_GENERAL ? "true" : "false");
            bool first = true;
            for (PerkInfo const& perk : Perks())
            {
                if (perk.tree != tree.tree || !Offered(perk))
                    continue;
                std::string ranks;
                for (uint8 rank = 1; rank <= perk.maxRank; ++rank)
                    ranks += (rank > 1 ? "," : "") + JsonString(RankText(perk, rank));
                json += Acore::StringFormat("{}{{\"id\":\"{}\",\"name\":{},\"icon\":\"{}\",\"maxRank\":{},\"row\":{},\"column\":{},"
                    "\"requiresTreePoints\":{},\"requiresPerk\":\"{}\",\"permanent\":{},\"description\":{},\"ranks\":[{}]}}",
                    first ? "" : ",", perk.key, JsonString(perk.name), perk.icon, perk.maxRank, perk.row, perk.column,
                    perk.requiresTreePoints, perk.requiresPerk == NO_PERK ? "" : Perks()[perk.requiresPerk].key,
                    Permanent(perk.perk) ? "true" : "false", JsonString(perk.description), ranks);
                first = false;
            }
            json += "]}";
        }
        return json + "]";
    }

    void Answer(ChatHandler* handler, std::string const& json)
    {
        handler->SendSysMessage(json);
    }

    bool Refuse(ChatHandler* handler, std::string_view error, std::string_view message)
    {
        Answer(handler, Acore::StringFormat("{{\"ok\":false,\"error\":{},\"message\":{}}}", JsonString(error), JsonString(message)));
        return true;    // an answer, not a failed command: SOAP passes it on as it is
    }

    // "well_rested=5,diplomat=3", or "-" for none.
    bool ParseBuild(std::string_view text, Build& build, std::string& error)
    {
        build = {};
        if (text == "-")
            return true;
        for (std::string_view token : Acore::Tokenize(text, ',', false))
        {
            std::vector<std::string_view> parts = Acore::Tokenize(token, '=', true);
            PerkInfo const* perk = parts.size() == 2 ? FindPerk(parts[0]) : nullptr;
            Optional<uint8> rank = perk ? Acore::StringTo<uint8>(parts[1]) : std::nullopt;
            if (!perk)
            {
                error = Acore::StringFormat("Unknown perk: {}.", parts.empty() ? token : parts[0]);
                return false;
            }
            if (!rank || !*rank || *rank > perk->maxRank || build[perk->perk])
            {
                error = Acore::StringFormat("{} can't be rank {}.", perk->name, parts[1]);
                return false;
            }
            build[perk->perk] = *rank;
        }
        return true;
    }
}

class LegacyCommandScript : public CommandScript
{
public:
    LegacyCommandScript() : CommandScript("LegacyCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable legacyCommandTable =
        {
            { "list",   HandleListCommand,   SEC_CONSOLE,       Console::Yes },
            { "set",    HandleSetCommand,    SEC_CONSOLE,       Console::Yes },
            { "show",   HandleShowCommand,   SEC_PLAYER,        Console::No  },
            { "reload", HandleReloadCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "bagprobe", HandleBagProbeCommand, SEC_GAMEMASTER, Console::No },
        };

        static ChatCommandTable commandTable =
        {
            { "legacy", legacyCommandTable },
        };

        return commandTable;
    }

    static bool HandleListCommand(ChatHandler* handler, uint32 account)
    {
        if (!GetConfig().enabled)
            return Refuse(handler, "closed", "Legacy is switched off on this realm.");

        BackfillAccount(account);
        std::vector<Character> characters = LoadCharacters(account);
        Earned earned = GetEarned(account);

        std::string json = Acore::StringFormat("{{\"ok\":true,\"cap\":{},\"earned\":{{\"total\":{},\"hardcore\":{}}},\"characters\":[",
            GetConfig().pointCap, earned.total, earned.hardcore);
        for (size_t i = 0; i < characters.size(); ++i)
            json += (i ? "," : "") + CharacterJson(characters[i], earned);
        json += "],\"trees\":" + TreesJson() + ",\"challenges\":" + ChallengesJson(account) + "}";
        Answer(handler, json);
        return true;
    }

    static bool HandleSetCommand(ChatHandler* handler, uint32 account, uint32 guid, std::string ranks)
    {
        if (!GetConfig().enabled)
            return Refuse(handler, "closed", "Legacy is switched off on this realm.");

        Build build;
        std::string error;
        if (!ParseBuild(ranks, build, error))
            return Refuse(handler, "invalid", error);

        std::vector<Character> characters = LoadCharacters(account, guid);
        if (characters.empty())
            return Refuse(handler, "no_character", "That character isn't on your account any more.");
        Character const& character = characters.front();

        if (character.online && character.online->IsInCombat())
            return Refuse(handler, "combat", Acore::StringFormat("{} is in combat. Try again once the fight is over.", character.name));

        // A perk the realm doesn't offer keeps whatever it has; a permanent one only goes up.
        Build saved = GetBuild(guid);
        for (PerkInfo const& perk : Perks())
        {
            if (!Offered(perk) && build[perk.perk] != saved[perk.perk])
                return Refuse(handler, "invalid", Acore::StringFormat("{} isn't on this realm right now.", perk.name));
            if (Permanent(perk.perk) && build[perk.perk] < saved[perk.perk])
                return Refuse(handler, "locked", Acore::StringFormat("{} is permanent: the {} rank{} saved can't be taken back.",
                    perk.name, saved[perk.perk], saved[perk.perk] > 1 ? "s" : ""));
        }

        BackfillAccount(account);
        Earned earned = GetEarned(account);
        auto [code, message] = Validate(build, SpendablePoints(earned, character.hardcore));
        if (!code.empty())
            return Refuse(handler, code, message);

        SetBuild(guid, build);
        // The world thread runs SOAP commands while no map updates, so the player is safe here.
        if (Player* player = character.online)
        {
            RefreshPerks(player);
            UpdateBags(player);
            ChatHandler(player->GetSession()).PSendSysMessage("|cffe6cc80Legacy:|r your perks were changed in the launcher ({} of {} points spent).",
                Spent(build), SpendablePoints(earned, character.hardcore));
        }

        Answer(handler, Acore::StringFormat("{{\"ok\":true,\"character\":{},\"message\":{}}}", CharacterJson(character, earned),
            JsonString(Acore::StringFormat("Legacy perks saved for {}.", character.name))));
        return true;
    }

    static bool HandleShowCommand(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;
        if (!GetConfig().enabled)
        {
            handler->SendSysMessage("Legacy is switched off on this realm.");
            return true;
        }

        uint32 guid = player->GetGUID().GetCounter();
        Earned earned = GetEarned(player->GetSession()->GetAccountId());
        bool hardcore = IsHardcore(guid);
        Build build = GetBuild(guid);
        handler->PSendSysMessage("|cffe6cc80Legacy:|r {} of {} points spent; your account has earned {}{}.", Spent(build),
            SpendablePoints(earned, hardcore), earned.total,
            hardcore ? Acore::StringFormat(" ({} on hardcore characters, which is what a hardcore character spends)", earned.hardcore) : "");
        for (TreeInfo const& tree : Trees())
            for (PerkInfo const& perk : Perks())
                if (perk.tree == tree.tree && build[perk.perk])
                    handler->PSendSysMessage("  {}: {} {}/{}: {}", tree.name, perk.name, build[perk.perk], perk.maxRank, RankText(perk, build[perk.perk]));
        if (!Spent(build))
            handler->SendSysMessage("  No perks yet. Pick them on the LEGACY page of the launcher.");
        return true;
    }

    // Deep Pockets is off until this shows the 3.3.5 client draws the extra slots: it gives the
    // GM's own bags in the four bag slots `extra` more slots (0 puts them back) until a bag is
    // moved or the perk is saved.
    static bool HandleBagProbeCommand(ChatHandler* handler, Optional<uint8> extra)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;
        uint32 slots = std::min<uint32>(extra.value_or(4), MAX_BAG_SIZE);
        UpdateBags(player, slots);
        for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
            if (Bag* bag = player->GetBagByPos(bagSlot))
                handler->PSendSysMessage("  bag {}: {} slots ({} without the probe)", bagSlot - INVENTORY_SLOT_BAG_START + 1,
                    bag->GetBagSize(), bag->GetTemplate()->ContainerSlots);
        handler->SendSysMessage("Open your bags: the extra slots work if the bag windows show them and an item can go in one. "
            "Take the items out of them before you log out, or they come back by mail.");
        return true;
    }

    static bool HandleReloadCommand(ChatHandler* handler)
    {
        LoadChallenges();
        handler->SendSysMessage("mod-legacy: challenges reloaded.");
        return true;
    }
};

void AddLegacyCommandScripts()
{
    new LegacyCommandScript();
}
