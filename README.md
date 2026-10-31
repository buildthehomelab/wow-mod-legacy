# mod-legacy

An [AzerothCore](https://www.azerothcore.org/) module (WotLK 3.3.5a) that brings over WoW
Forever's **Legacy** system.

Challenges done anywhere on an account earn **Legacy points**, one each. A class reaching a level,
a profession reaching a skill and a raid's achievement are all challenges. Every character spends
those points on perks in three trees, **Adventure**, **Resourcefulness** and **Professions**, and
a **General** row above them, up to a cap that grows with the character's era. They make
levelling, travelling and professions easier; only Talented and Prodigy add combat power, as
talent points.

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
- **Era caps**: each challenge belongs to an era (the `era` column: 0 Classic, 1 The Burning
  Crusade, 2 Wrath of the Lich King). A character can spend at most as many points as there are
  challenges up to its own era, which comes from mod-individual-progression's state (quest
  66000 + state; 8 and later is The Burning Crusade, 13 and later Wrath). With the 130 challenges
  that ship with the module that's **70** in Classic, **100** in The Burning Crusade and **130** in
  Wrath. So a Classic character can't spend points its account earned in Outland or Northrend
  until it gets there. `Legacy.EraCaps.Enable = 0` drops the era caps; `Legacy.PointCap` adds a
  flat limit on top (0, none, by default).
- **Hardcore**: with mod-challenges (its `mod_challenges_character` table), a character still on
  its Hardcore challenge spends only points that hardcore characters earned, and only on **Deep
  Pockets**, **Talented** and **Prodigy**: 24 points in all (`Legacy.Hardcore.LimitedPerks`). It
  skips what Talented and Prodigy normally need from the rest of Adventure (the tree points and
  Well Rested 5/5); Prodigy still needs Talented 5/5. A normal character spends everything the
  account earned, hardcore points included (WoW Forever's rule), and its trees work as usual. A
  hardcore character that dies plays on as a normal one and gets the full pool and every perk; its
  Talented and Prodigy then need their usual requirements, so a build without them switches off at
  its next login until it is changed in the launcher.
- Progress made before the module was installed counts. The launcher's listing and each login
  check every character of the account from the database. Bot accounts are left out.
- If a saved build no longer fits (the cap was lowered, challenges were switched off, or the
  character took Hardcore after picking perks), its perks switch off at login until it is changed
  in the launcher (Deep Pockets stays). Nothing is deleted, so undoing the change brings them back.

## The trees

Ranks cost one point each. A perk in the second row needs 5 points in the rest of its tree first,
and a perk in the third row needs 10. Some perks also need every rank of the perk above them.

### General

One row above the three trees, with nothing needed in any tree. Deep Pockets is off until
`Legacy.DeepPockets.Enable = 1`.

| Perk | Ranks | Per rank |
|---|---|---|
| Deep Pockets | 10 | Each bag in your four bag slots has 1 more slot (36 at most). **Permanent**: a saved rank can't be taken back, and it stays when the rest of the build is switched off. |
| Hasty Hearth | 5 | Your Hearthstone cools down 5 min sooner: 30 min down to 5 min at rank 5. Astral Recall, the Scrolls of Recall, the Runes of Recall, the Ruby Slippers, the Gauntlets of the Cheerful Hearth and the Ethereal Portal cool down 5 min sooner too, never below 5 min. |

Hasty Hearth expects the realm's own cooldowns for those (30 min for the Hearthstone). After
every cast of one of them, perk or not, the module sends the client the cooldown the server has,
since the client otherwise starts the one its Spell.dbc names.

The backpack itself stays at 16: unlike a bag, it has no size the server can change.
How it works: a bag's size is a value on the bag that the server sends to the client. A stock
3.3.5 client reads it only when it creates the bag and keeps that copy, even when the bag is sent
again, so a bag resized in play (the perk bought, a bag moved) shows its new size only after a
relog. Evermore's launcher (Portalkeeper 0.5.46+) edits Wow.exe to read the value itself (wow-server
`tools/client-patches/live_bag_slots.py`): then close and reopen the bag windows to see it. The core
equips a character's bags at login before it loads what's in them, so the extra slots are there
in time for the items saved in them. A bag gets its own size back once it leaves the four bag
slots. A bag with items in its extra slots can't be moved until they're taken out: in a bank bag
slot it would lose those slots at the next login. With the perk switched off, a bag keeps its
extra slots only while items are in them, and at the next login the core mails those items
back ("There were problems with equipping item(s).").

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
| Talented | 5 | Talent points start one level earlier (level 5 at 5/5), up to the same total at the top level (71 at 80). Needs Well Rested 5/5. |
| Prodigy | 9 | +1 talent point once talent points have started (80 at 80 with 9/9). Fourth row (15 points in Adventure); needs Talented 5/5. |

Talented and Prodigy change the core's talent point count, so run them without
mod-talent-per-level (or with `TalentPerLevel.Enable = 0`): it would add them on top of its own
point a level. Death Knights in Ebon Hold keep the core's quest-based count. Taking ranks back
recounts the character's talents at once; one that has spent more than it has left gets a free
talent reset, like after a level-down. Bots have no perks and keep the core's count.

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

Four of WoW Forever's perks rely on things 3.3.5 doesn't have, or that already exist on this
server in another form. Explorer's Stride and Prodigy are this realm's own additions to Adventure.

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
| Challenges | mod-challenges' achievements: Immortal, Deathless and Eternal (Hardcore to 60, 70 and 80 without dying), Hambringer (Boaring Adventure to 60) and That Which Has No Life (Boaring Adventure with Exhaustion and Slow XP to 60) | 5 |

By era: **Classic 70** (levels 20 to 60, professions to 300, Novice to Master Spelunker, the seven
Classic raids, Eastern Kingdoms and Kalimdor, the three PvP factions, Immortal, Hambringer and That
Which Has No Life), **The Burning Crusade 30** (level 70, professions to 375, Outland Spelunker,
the nine Burning Crusade raids, Outland, Deathless) and **Wrath of the Lich King 30** (level 80,
professions to 450, Northrend Spelunker, the nine Wrath raids, Northrend, Eternal).

The Challenges rows need mod-challenges, which grants those achievements. Without it they are
skipped at startup (a log line each) and don't count toward the caps. A Hardcore achievement is
earned on the hardcore character, so it also counts toward the points hardcore characters spend.

The launcher lists them in `sort` order, or easiest first by the `difficulty` column: about the
level a character does the challenge at (a class level as is; a profession's 150, 225, 300, 375
and 450 as 15, 25, 45, 65 and 75), with each raid tier a step above the last (Zul'Gurub and Ruins
of Ahn'Qiraj 61 up to Naxxramas 66; Karazhan 71 up to Sunwell 76; Naxxramas 81 up to Icecrown 86).
Smaller is easier, and rows with the same value keep their `sort` order.

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
| `.legacy bagprobe [slots]` | game masters | Gives your bags in the four bag slots `slots` (4) more slots until a bag is moved; 0 puts them back. Checks that the client draws Deep Pockets' slots before `Legacy.DeepPockets.Enable` goes on. |

## Requirements

- AzerothCore (WotLK 3.3.5a). Two perks use optional core hooks; see the end of Installation.
- SOAP enabled on the worldserver, for the launcher's page (the portal runs `.legacy list` and
  `.legacy set` as console commands).
- Optional: mod-challenges, for the separate hardcore points.
- Optional: mod-individual-progression, for the era caps (without it, turn
  `Legacy.EraCaps.Enable` off, or every character stays at the Classic cap).

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
  has now (a lower `Legacy.PointCap`, challenges switched off, or Hardcore taken after picking),
  or a hardcore character has a perk other than Deep Pockets, Talented and Prodigy, or a
  character that left Hardcore has Talented or Prodigy without the rest of Adventure.
  Changing the build in the launcher, or undoing the change, brings them back.
- **Deep Pockets is missing from the launcher**: `Legacy.DeepPockets.Enable` is 0 (the default).
  Try `.legacy bagprobe` first: if the bag windows don't show the extra slots, leave it off.
- **`.legacy bagprobe` changes nothing on screen**: the client hasn't got the live bag size edit
  (Portalkeeper 0.5.46+, or `live_bag_slots.py` on Wow.exe). Relog: the bags are created again
  with their size, but the probe's slots are gone by then (the login puts the perk's back).
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
