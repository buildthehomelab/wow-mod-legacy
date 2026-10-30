"""Compile and run production eligibility and record logic with an in-memory DB fixture."""
from pathlib import Path
import subprocess
import tempfile

source = (Path(__file__).parents[1] / 'src/Legacy.cpp').read_text()


def extract(signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


harness = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
using uint8 = uint8_t; using uint16 = uint16_t; using uint32 = uint32_t;
enum { SKILL_ALCHEMY=171, SKILL_BLACKSMITHING=164, SKILL_ENCHANTING=333,
SKILL_ENGINEERING=202, SKILL_HERBALISM=182, SKILL_INSCRIPTION=773,
SKILL_JEWELCRAFTING=755, SKILL_LEATHERWORKING=165, SKILL_MINING=186,
SKILL_SKINNING=393, SKILL_TAILORING=197, SKILL_COOKING=185,
SKILL_FIRST_AID=129, SKILL_FISHING=356, CHALLENGE_SKILL=2, CHALLENGE_LEVEL=1, ERA_CLASSIC=0 };
'''
for name in ['struct Challenge\n', 'struct Done\n', 'struct Fresh\n']:
    harness += extract(name) + ';\n'
start = source.index('    struct ProfessionDefinition')
end = source.index('    bool IsProfessionRankSpell', start)
harness += source[start:end]
harness += r'''
std::unordered_map<uint32, uint8> ProfessionTrainerLevels;
std::mutex doneLock;
std::unordered_map<uint32, std::unordered_map<uint32, Done>> doneByAccount;
struct Transaction { template<class... Args> void Append(char const*, Args...) {} };
using CharacterDatabaseTransaction = std::shared_ptr<Transaction>;
struct Database {
    int commits = 0;
    auto BeginTransaction() { return std::make_shared<Transaction>(); }
    void CommitTransaction(CharacterDatabaseTransaction) { ++commits; }
} CharacterDatabase;
namespace GameTime { auto GetGameTime() { return std::chrono::seconds(123); } }
'''
harness += extract('bool MeetsHardcoreProfessionLevel(')
harness += extract('std::vector<Fresh> Record(')
harness += r'''
int main() {
    // Fixture levels can vary by profession; production loads them from trainer_spell.
    std::array<uint8,6> craft{5,10,20,35,50,65};
    auto const* alchemy = GetProfessionDefinition(SKILL_ALCHEMY);
    for (uint32 rank=1; rank<=6; ++rank) {
        ProfessionTrainerLevels[alchemy->RankSpells[rank-1]] = craft[rank-1];
        Challenge c; c.type=CHALLENGE_SKILL; c.value1=SKILL_ALCHEMY; c.value2=rank*75;
        assert(!MeetsHardcoreProfessionLevel(c, craft[rank-1]-1));
        assert(MeetsHardcoreProfessionLevel(c, craft[rank-1]));
        assert(GetBestRankSpellForSkill(c.value1, c.value2)==alchemy->RankSpells[rank-1]);
        if (rank<6) assert(GetBestRankSpellForSkill(c.value1, c.value2+1)==alchemy->RankSpells[rank]);
    }
    Challenge c; c.id=205; c.type=CHALLENGE_SKILL; c.value1=SKILL_ALCHEMY; c.value2=450;
    std::vector<Challenge const*> met{&c};
    // A synced HC character still gets its ordinary account credit at level 1, but no HC credit.
    auto fresh = Record(1,10,true,1,met);
    assert(fresh.size()==1 && fresh[0].newForAccount);
    assert(doneByAccount[1][205].guid==10 && doneByAccount[1][205].hardcoreGuid==0);
    assert(Record(1,10,true,64,met).empty());
    fresh = Record(1,10,true,65,met);
    assert(fresh.size()==1 && !fresh[0].newForAccount);
    assert(doneByAccount[1][205].hardcoreGuid==10);
    assert(Record(1,11,true,80,met).empty());
    // A normal character's synced profession challenge is unchanged, including missing trainer data.
    ProfessionTrainerLevels.clear();
    fresh = Record(2,20,false,1,met);
    assert(fresh.size()==1 && doneByAccount[2][205].guid==20 && doneByAccount[2][205].hardcoreGuid==0);
    assert(Record(2,20,true,80,met).empty());
    // Existing normal account credit can acquire HC credit at the correct per-profession level.
    ProfessionTrainerLevels[alchemy->RankSpells[5]]=55;
    assert(Record(2,21,true,54,met).empty());
    assert(Record(2,21,true,55,met).size()==1);
    assert(doneByAccount[2][205].guid==20 && doneByAccount[2][205].hardcoreGuid==21);
    // Other HC challenges and non-profession skill challenges retain their existing rules.
    c.id=1; c.type=CHALLENGE_LEVEL;
    assert(Record(3,30,true,1,met).size()==1 && doneByAccount[3][1].hardcoreGuid==30);
    c.id=2; c.type=CHALLENGE_SKILL; c.value1=999;
    assert(Record(3,30,true,1,met).size()==1 && doneByAccount[3][2].hardcoreGuid==30);
    // Values beyond the supported profession ranks cannot grant premature HC credit.
    c.value1=SKILL_ALCHEMY; c.value2=451;
    assert(!MeetsHardcoreProfessionLevel(c,80));
}
'''
with tempfile.TemporaryDirectory() as directory:
    cpp = Path(directory) / 'policy.cpp'
    binary = Path(directory) / 'policy'
    cpp.write_text(harness)
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('Hardcore profession eligibility and normal/HC recording regressions passed')
