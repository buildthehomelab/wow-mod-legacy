# mod-legacy

An [AzerothCore](https://www.azerothcore.org/) module (WotLK 3.3.5a) that brings over WoW
Forever's **Legacy** system.

Challenges done anywhere on an account earn **Legacy points**, one each. A class reaching a level,
a profession reaching a skill and a raid's achievement are all challenges. Every character spends
up to 16 of those points on perks in three trees: **Adventure**, **Resourcefulness** and
**Professions**. None of the perks adds combat power; they make levelling, travelling and
professions easier.

Perks are picked on the LEGACY page of the
[Portalkeeper launcher fork](https://github.com/buildthehomelab/wow-Portalkeeper). The launcher
reaches the module through the
[portal](https://github.com/buildthehomelab/wow-mod-azerothcore-portal) (`api/launcher/legacy.php`),
which runs the module's console commands over SOAP. Any other front end can use the same two
commands (see [Commands](#commands)). Changes are
free, can be made at any time, and apply at once to a character that is online, except while it
is in combat.

## Points

- Each challenge in the world table `mod_legacy_challenge` is worth one point, once per account.
  The first character to do it gets the credit. Deleting that character doesn't take the point
  back.
- A character can spend `Legacy.PointCap` points (16, like WoW Forever's first season). The
  account can earn far more: 125 challenges ship with the module.
- **Hardcore**: with mod-challenges (its `mod_challenges_character` table), a character still on its Hardcore challenge spends only points that hardcore characters earned.
  A normal character spends everything the account earned, hardcore points included (WoW
  Forever's rule). A hardcore character that dies plays on as a normal one and gets the full
  pool.
- Progress made before the module was installed counts. The launcher's listing and each login
  check every character of the account from the database. Bot accounts are left out.
- If a saved build no longer fits (the cap was lowered, challenges were switched off, or the
  character took Hardcore after picking perks), its perks switch off at login until it is changed
  in the launcher. Nothing is deleted, so undoing the change brings them back.

## The trees

Ranks cost one point each. A perk in the second row needs 5 points in the rest of its tree first,
and a perk in the third row needs 10. Some perks also need every rank of the perk above them.

### Adventure

| Perk | Ranks | Per rank |
|---|---|---|
| High Alert | 2 | See stealthed enemies as if one level higher. Not in battlegrounds or arenas. |
| Well Rested | 5 | Rested experience builds up 4% faster, and you can store 4% more. |
| Thrill of Adventure | 5 | Killing an enemy that gives experience (you or your pet) restores 1% health and mana over 10 sec. Not in dungeons, raids or battlegrounds. |
| Field Medicine | 2 | Recently Bandaged lasts 5 sec less. Not in dungeons, raids or battlegrounds. |
| Explorer's Stride | 5 | +2% run speed on foot out of combat. Not in dungeons, raids or battlegrounds. Needs Well Rested 5/5. |
| Second Wind | 3 | Resurrection Sickness 33% shorter; at rank 3, none at all. |
| Frequent Flier | 1 | Flight paths cost 50% less, and you fly 20% faster. |
| Summoning Stone | 1 | Raid Finder's Summon to Stone takes you to the raid's meeting stone from anywhere (mod-raid-finder reads this perk). Without it you travel there yourself; Gather Bots at the stone still brings your raid's bots. |

### Resourcefulness

| Perk | Ranks | Per rank |
|---|---|---|
| For Great Honor | 5 | +2% honor from honorable kills. |
| Gourmand | 3 | Well Fed lasts 33% longer. |
| Diplomat | 5 | +2% reputation from monsters and quests. |
| Reinforce | 5 | Items lose 8% less durability when you die. |
| Permanence | 2 | Your class buffs of 10 min or more that can go on others last 25% longer. Needs Gourmand 3/3. |
| The Quick and the Dead | 2 | +5% speed as a ghost; at rank 2, helpful spells cost nothing for 1 min after you come back to life, until combat. Not in battlegrounds or arenas. |
| Reagent Economy | 1 | Class abilities no longer need reagents that vendors sell. |

### Professions

| Perk | Ranks | Per rank |
|---|---|---|
| Working Overtime | 5 | An extra 4% chance of a skill-up when crafting or gathering, while the recipe or node isn't gray. |
| Master Chef | 5 | 10% chance to cook one more. |
| Luremaster | 2 | With a lure on your fishing pole, 25% chance to catch one more fish. |
| Bountiful Harvest | 5 | 4% chance to gather twice as much from Mining, Herbalism and Skinning. Needs Working Overtime 5/5. |
| Bartering | 2 | Vendor prices and repairs 5% lower. |
| Salvager | 3 | 5% chance of one more material when disenchanting, prospecting or milling. |
| Dedicated Study | 1 | Once a day, your lowest profession that isn't at its maximum gains 1 skill. |

### Changes from WoW Forever

Five of WoW Forever's perks rely on things 3.3.5 doesn't have, or that already exist on this
server in another form:

- **Talented** → **Explorer's Stride**: mod-talent-per-level already gives a talent point every
  level.
- **Field Guide** → **Second Wind**: there are no camps.
- **Bountiful Harvest** doubles a gather instead of adding "Scarce" materials.
- **Performance Bonus** → **Salvager**: there are no commerce crates.
- **Dedicated Study** does nothing once every profession is at its maximum: there is no Elemental
  Essence.

Some perks also work a little differently, because of what the core lets a module change:

- For Great Honor counts honorable kills only; battleground bonus honor has no hook.
- Reinforce gives back part of the durability lost to the death and the spirit healer, just
  after resurrection.
- The Quick and the Dead refunds what a helpful spell took just after the cast, so the spell
  still needs the mana to start.
- Working Overtime is a second skill-up roll, so an orange recipe can sometimes give two points.
- Bartering also lowers training costs at the few NPCs that are both vendor and trainer.

## Challenges

| Category | Challenges | Points |
|---|---|---|
| Classes | Each class to level 20, 40, 60, 70 and 80 (Death Knights 60, 70 and 80) | 48 |
| Professions | Alchemy, Blacksmithing, Enchanting, Engineering, Jewelcrafting, Leatherworking, Tailoring and Inscription to 150, 225, 300, 375 and 450 | 40 |
| Dungeons | Novice, Experienced, Master, Outland and Northrend Spelunker (dungeon achievements) | 5 |
| Raids | Each raid's achievement: Onyxia (60), Molten Core, Blackwing Lair, Zul'Gurub, both Ahn'Qiraj, Naxxramas (60), the nine Burning Crusade raids and the nine Wrath raids (10 or 25 player) | 25 |
| Exploration | Explore Eastern Kingdoms, Kalimdor, Outland and Northrend | 4 |
| PvP | Exalted with Warsong Gulch, Arathi Basin and Alterac Valley (either side) | 3 |

The rows are data: add, edit or switch them off (`enabled = 0`) and run `.legacy reload`. A
point earned stays earned, but a challenge that is switched off no longer counts toward the
total. Rows 100-699 ship with the module and are rewritten whenever its SQL changes, so put your
own rows at 700 or above. Challenge types:

| `type` | Done when a character | `value1` | `value2` | `list` |
|---|---|---|---|---|
| 1 | of the class reaches the level | class | level | |
| 2 | has the base skill | skill | value | |
| 3 | has every achievement | | | achievement ids |
| 4 | has any of the achievements | | | achievement ids |
| 5 | is Exalted with any of the factions | | | faction ids |

## Commands

| Command | Who | |
|---|---|---|
| `.legacy show` | players | Your points and perks. |
| `.legacy list <account>` | console / SOAP | The account's characters, points, trees and challenges as one line of JSON. |
| `.legacy set <account> <guid> <perk=rank,...>` | console / SOAP | Saves a character's whole build (`-` for none). |
| `.legacy reload` | administrators | Reloads the challenges. |

## Requirements

- AzerothCore (WotLK 3.3.5a). Two perks use optional core hooks; see the end of Installation.
- SOAP enabled on the worldserver, for the launcher's page (the portal runs `.legacy list` and
  `.legacy set` as console commands).
- Optional: mod-challenges, for the separate hardcore points.

## Installation

```
cd modules
git clone https://github.com/buildthehomelab/wow-mod-legacy.git mod-legacy
```

Re-run CMake, rebuild, and copy `conf/mod_legacy.conf.dist` to `mod_legacy.conf`. The SQL applies
itself at the next start. On the portal, set `LEGACY_ENABLED=true` (it needs the SOAP setup the
coin shop uses).

Two perks use core hooks that a stock AzerothCore doesn't have yet. The module builds without
them:

- **Well Rested** needs `OnPlayerGainRestBonus` and `OnPlayerGetRestBonusMax`; without them it
  does nothing.
- **Frequent Flier** needs `OnPlayerTaxiCostDiscount` for every leg of a flight to be cheaper,
  and `OnPlayerTaxiFlightSpeed` for faster flights. Without them only the first leg is cheaper.

## IDs

- spell_dbc 90480-90482 and 90490-90499: hidden, server-only auras that the client never sees.
  90480 is High Alert, 90481 Explorer's Stride and 90482 The Quick and the Dead; 90490-90499 are
  Reagent Economy, one per class.
- Tables: `mod_legacy_challenge` (world); `mod_legacy_challenge_done`, `mod_legacy_rank` and
  `mod_legacy_character` (characters).

## Troubleshooting

- **No challenges load** (`mod-legacy: 0 challenges loaded` at startup): the world SQL hasn't
  applied. Without challenges nothing is judged, and saved builds stay as they are.
- **A challenge is skipped at startup**: its class, skill, achievement or faction isn't in this
  server's data. Naxxramas (Level 60) needs mod-individual-progression's achievement (533), and
  only mod-individual-progression awards Onyxia's Lair (Level 60) (684).
- **A player's perks "switched off" at login**: the build needs more points than the character
  has now (a lower `Legacy.PointCap`, challenges switched off, or Hardcore taken after picking).
  Changing the build in the launcher, or undoing the change, brings them back.
- **Reagent Economy does nothing for a class**: the startup log lists, per class, how many spell
  ranks need only vendor reagents.

## Credits

- The Legacy system's design, perks and point rules are from Blizzard's WoW Forever.
- Several perks are modelled on mod-guild-levels' guild perks.

## Uninstall

Run `data/sql/uninstall/mod_legacy_uninstall_world.sql` on the world database and
`mod_legacy_uninstall_characters.sql` on the characters database, with the server stopped.

## License

Released under the [MIT License](LICENSE).

### Hardcore profession challenge credit

Account-wide profession synchronization can give a new Hardcore character high
skill values immediately. Hardcore credit for Legacy's profession challenges
waits until the character reaches the trainer level for the rank needed by that
challenge's skill threshold. Ordinary Legacy credit remains based on the existing
skill requirement. Standard WoW achievements, other Legacy challenges, shared
profession skills and recipes are unchanged.

The same check applies to online progression and offline account backfill. Each
profession's rank requirements come from `trainer_spell.ReqLevel`, including
learn-spell wrappers, when challenges load. Reload the Legacy data or restart
after changing trainer requirements. A missing trainer rank withholds its Hardcore
credit. Previously recorded credit is retained; no SQL migration is included.

Run `python3 tests/test_hardcore_professions.py` for focused production-code
regressions covering rank boundaries, normal versus Hardcore credit, delayed
credit, different trainer levels and missing trainer data.
