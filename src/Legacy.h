/*
 * mod-legacy: WoW Forever's Legacy system.
 *
 * Challenges done anywhere on an account (a class reaching a level, a profession reaching a
 * skill, a raid's achievement, ...) earn it Legacy points, one each. Every character spends up to
 * Legacy.PointCap of them on perks in three trees: Adventure, Resourcefulness and Professions.
 * None of the perks adds combat power. Perks are picked in the launcher, which reaches the module
 * through the portal over SOAP (`.legacy list` / `.legacy set`).
 *
 * Hardcore characters (mod-challenges) spend only the points hardcore characters earned; normal
 * characters spend everything the account earned.
 *
 * Released under the MIT License.
 */

#ifndef MOD_LEGACY_H
#define MOD_LEGACY_H

#include "Define.h"
#include <array>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

class Player;

namespace Legacy
{
    enum Tree : uint8
    {
        TREE_ADVENTURE,
        TREE_RESOURCEFULNESS,
        TREE_PROFESSIONS,
        MAX_TREES
    };

    enum Perk : uint8
    {
        // Adventure
        PERK_HIGH_ALERT,
        PERK_WELL_RESTED,
        PERK_THRILL_OF_ADVENTURE,
        PERK_FIELD_MEDICINE,
        PERK_EXPLORERS_STRIDE,
        PERK_SECOND_WIND,
        PERK_FREQUENT_FLIER,
        PERK_SUMMONING_STONE,
        // Resourcefulness
        PERK_FOR_GREAT_HONOR,
        PERK_GOURMAND,
        PERK_DIPLOMAT,
        PERK_REINFORCE,
        PERK_PERMANENCE,
        PERK_QUICK_AND_DEAD,
        PERK_REAGENT_ECONOMY,
        // Professions
        PERK_WORKING_OVERTIME,
        PERK_MASTER_CHEF,
        PERK_LUREMASTER,
        PERK_BOUNTIFUL_HARVEST,
        PERK_BARTERING,
        PERK_SALVAGER,
        PERK_DEDICATED_STUDY,
        MAX_PERKS
    };

    constexpr uint8 NO_PERK = 0xFF;

    struct PerkInfo
    {
        Perk perk;
        char const* key;            // the id the launcher and the database use
        Tree tree;
        char const* name;
        char const* icon;
        uint8 maxRank;
        uint8 row;
        uint8 column;
        uint8 requiresTreePoints;   // points in the rest of the tree before a first rank
        uint8 requiresPerk;         // a perk that has to be at its max rank first, or NO_PERK
        uint32 perRank;             // the perk's number per rank (percent, seconds, ...)
        char const* description;
    };

    struct TreeInfo
    {
        Tree tree;
        char const* key;
        char const* name;
        char const* icon;
    };

    using Build = std::array<uint8, MAX_PERKS>;

    std::array<TreeInfo, MAX_TREES> const& Trees();
    std::array<PerkInfo, MAX_PERKS> const& Perks();
    PerkInfo const* FindPerk(std::string_view key);
    std::string RankText(PerkInfo const& perk, uint8 rank);

    struct Config
    {
        bool enabled = true;
        uint32 pointCap = 16;
        bool hardcorePool = true;
        bool announce = true;
    };

    Config const& GetConfig();

    // Ranks. The cache holds every character's build (loaded at startup), so hooks on any map
    // thread can read it; only the world thread writes it.
    Build GetBuild(uint32 guid);
    uint8 Rank(Player const* player, Perk perk);
    void SetBuild(uint32 guid, Build const& build);
    // Switches a saved build off until it is changed in the launcher (it no longer fits the
    // character's points); the rows stay, so a config or data mistake loses nothing.
    void SuspendBuild(uint32 guid);
    bool IsSuspended(uint32 guid);
    void ForgetCharacter(uint32 guid);
    uint32 Spent(Build const& build);
    uint32 SpentInTree(Build const& build, Tree tree);

    // Why `build` isn't allowed with `points` to spend: "" when it is, else a refusal code
    // (points, locked, invalid) and the message for the player.
    std::pair<std::string, std::string> Validate(Build const& build, uint32 points);

    // Challenges.
    struct Earned
    {
        uint32 total = 0;
        uint32 hardcore = 0;
    };

    Earned GetEarned(uint32 account);
    uint32 SpendablePoints(Earned const& earned, bool hardcore);
    bool IsHardcore(uint32 guid);
    bool HardcoreTracked();

    // Checks an online player for challenges done, from their own map update. `hardcore` is the
    // player's cached hardcore state.
    void CheckChallenges(Player* player, bool hardcore);
    size_t ChallengeCount();
    // Checks every character of an account from the database (offline progress, and everything
    // done before the module was installed). World thread only.
    void BackfillAccount(uint32 account);

    void LoadChallenges();
    void LoadRanks();
    void LoadDone();

    std::string JsonString(std::string_view text);
    std::string ChallengesJson(uint32 account);

    // Bot sessions: IsHeadless() on current playerbots cores, IsBot() on older ones.
    template <typename Session, typename = void>
    struct HasIsHeadless : std::false_type { };

    template <typename Session>
    struct HasIsHeadless<Session, std::void_t<decltype(std::declval<Session&>().IsHeadless())>> : std::true_type { };

    template <typename Session, typename = void>
    struct HasIsBot : std::false_type { };

    template <typename Session>
    struct HasIsBot<Session, std::void_t<decltype(std::declval<Session&>().IsBot())>> : std::true_type { };

    template <typename Session>
    bool IsBotSession(Session* session)
    {
        if constexpr (HasIsHeadless<Session>::value)
            return session->IsHeadless();
        else if constexpr (HasIsBot<Session>::value)
            return session->IsBot();
        else
            return false;
    }

    // Perks (LegacyPerks.cpp).
    void RefreshPerks(Player* player);
    void LoadReagentMasks();
    void OnLogin(Player* player);
}

#endif
