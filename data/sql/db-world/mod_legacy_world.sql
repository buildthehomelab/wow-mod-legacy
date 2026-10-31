-- mod-legacy: the challenges that earn Legacy points, and the server-only perk auras.

-- One Legacy point each, once per account. `type`:
--   1  a character of class `value1` reaches level `value2`
--   2  a character reaches `value2` (base, without bonuses) in skill `value1`
--   3  a character has every achievement in `list`
--   4  a character has any achievement in `list`
--   5  a character is Exalted with any faction in `list`
--   6  a character reached mod-challenges' milestone in every challenge in the bits of `value1`
--      (1 Hardcore, 2 Iron Man, 4 Self-Found, 8 Self-Crafted, 16 Slow XP, 32 Quest XP Only,
--      64 Item Quality Level, 128 Boaring Adventure, 256 Exhaustion): Challenges.MilestoneLevel
--      while still running it
-- `era` is the expansion the challenge belongs to (0 Classic, 1 The Burning Crusade, 2 Wrath of
-- the Lich King). A character spends at most as many points as there are challenges up to its
-- own era (mod-individual-progression's state), so a Classic character can't spend points its
-- account earned in Northrend. `difficulty` orders the launcher's "Easiest first" sort: about
-- the level a character does the challenge at, with each raid tier a step above the last (smaller
-- is easier; rows with the same value keep `sort` order). Adding, removing or editing rows is safe at any time
-- (`.legacy reload`): points already earned stay with the account, and a row that is switched
-- off stops counting. Rows 100-699 ship with the module and are rewritten whenever this file
-- changes: put your own rows at 700 or above. The Challenges rows (651-662) need mod-challenges:
-- without it their achievements and milestones don't exist, so they are skipped at startup.
CREATE TABLE IF NOT EXISTS `mod_legacy_challenge` (
    `id` SMALLINT UNSIGNED NOT NULL,
    `category` VARCHAR(32) NOT NULL,
    `name` VARCHAR(64) NOT NULL,
    `description` VARCHAR(255) NOT NULL,
    `type` TINYINT UNSIGNED NOT NULL,
    `value1` INT UNSIGNED NOT NULL DEFAULT 0,
    `value2` INT UNSIGNED NOT NULL DEFAULT 0,
    `list` VARCHAR(255) NOT NULL DEFAULT '',
    `sort` SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    `era` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `difficulty` SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    `enabled` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='mod-legacy: challenges that earn Legacy points';

-- Tables made before `era` existed get it here (MySQL has no ADD COLUMN IF NOT EXISTS).
SET @legacy_era := (SELECT COUNT(*) FROM information_schema.columns
    WHERE table_schema = DATABASE() AND table_name = 'mod_legacy_challenge' AND column_name = 'era');
SET @legacy_sql := IF(@legacy_era = 0,
    'ALTER TABLE `mod_legacy_challenge` ADD COLUMN `era` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `sort`',
    'DO 0');
PREPARE legacy_stmt FROM @legacy_sql;
EXECUTE legacy_stmt;
DEALLOCATE PREPARE legacy_stmt;

-- And before `difficulty` existed.
SET @legacy_difficulty := (SELECT COUNT(*) FROM information_schema.columns
    WHERE table_schema = DATABASE() AND table_name = 'mod_legacy_challenge' AND column_name = 'difficulty');
SET @legacy_sql := IF(@legacy_difficulty = 0,
    'ALTER TABLE `mod_legacy_challenge` ADD COLUMN `difficulty` SMALLINT UNSIGNED NOT NULL DEFAULT 0 AFTER `era`',
    'DO 0');
PREPARE legacy_stmt FROM @legacy_sql;
EXECUTE legacy_stmt;
DEALLOCATE PREPARE legacy_stmt;

DELETE FROM `mod_legacy_challenge` WHERE `id` BETWEEN 100 AND 699;
INSERT INTO `mod_legacy_challenge` (`id`, `category`, `name`, `description`, `type`, `value1`, `value2`, `list`, `sort`, `era`, `difficulty`) VALUES
(101, 'Classes', 'Warrior: Level 20', 'Reach level 20 with a Warrior.', 1, 1, 20, '', 0, 0, 20),
(102, 'Classes', 'Warrior: Level 40', 'Reach level 40 with a Warrior.', 1, 1, 40, '', 1, 0, 40),
(103, 'Classes', 'Warrior: Level 60', 'Reach level 60 with a Warrior.', 1, 1, 60, '', 2, 0, 60),
(104, 'Classes', 'Warrior: Level 70', 'Reach level 70 with a Warrior.', 1, 1, 70, '', 3, 1, 70),
(105, 'Classes', 'Warrior: Level 80', 'Reach level 80 with a Warrior.', 1, 1, 80, '', 4, 2, 80),
(106, 'Classes', 'Paladin: Level 20', 'Reach level 20 with a Paladin.', 1, 2, 20, '', 5, 0, 20),
(107, 'Classes', 'Paladin: Level 40', 'Reach level 40 with a Paladin.', 1, 2, 40, '', 6, 0, 40),
(108, 'Classes', 'Paladin: Level 60', 'Reach level 60 with a Paladin.', 1, 2, 60, '', 7, 0, 60),
(109, 'Classes', 'Paladin: Level 70', 'Reach level 70 with a Paladin.', 1, 2, 70, '', 8, 1, 70),
(110, 'Classes', 'Paladin: Level 80', 'Reach level 80 with a Paladin.', 1, 2, 80, '', 9, 2, 80),
(111, 'Classes', 'Hunter: Level 20', 'Reach level 20 with a Hunter.', 1, 3, 20, '', 10, 0, 20),
(112, 'Classes', 'Hunter: Level 40', 'Reach level 40 with a Hunter.', 1, 3, 40, '', 11, 0, 40),
(113, 'Classes', 'Hunter: Level 60', 'Reach level 60 with a Hunter.', 1, 3, 60, '', 12, 0, 60),
(114, 'Classes', 'Hunter: Level 70', 'Reach level 70 with a Hunter.', 1, 3, 70, '', 13, 1, 70),
(115, 'Classes', 'Hunter: Level 80', 'Reach level 80 with a Hunter.', 1, 3, 80, '', 14, 2, 80),
(116, 'Classes', 'Rogue: Level 20', 'Reach level 20 with a Rogue.', 1, 4, 20, '', 15, 0, 20),
(117, 'Classes', 'Rogue: Level 40', 'Reach level 40 with a Rogue.', 1, 4, 40, '', 16, 0, 40),
(118, 'Classes', 'Rogue: Level 60', 'Reach level 60 with a Rogue.', 1, 4, 60, '', 17, 0, 60),
(119, 'Classes', 'Rogue: Level 70', 'Reach level 70 with a Rogue.', 1, 4, 70, '', 18, 1, 70),
(120, 'Classes', 'Rogue: Level 80', 'Reach level 80 with a Rogue.', 1, 4, 80, '', 19, 2, 80),
(121, 'Classes', 'Priest: Level 20', 'Reach level 20 with a Priest.', 1, 5, 20, '', 20, 0, 20),
(122, 'Classes', 'Priest: Level 40', 'Reach level 40 with a Priest.', 1, 5, 40, '', 21, 0, 40),
(123, 'Classes', 'Priest: Level 60', 'Reach level 60 with a Priest.', 1, 5, 60, '', 22, 0, 60),
(124, 'Classes', 'Priest: Level 70', 'Reach level 70 with a Priest.', 1, 5, 70, '', 23, 1, 70),
(125, 'Classes', 'Priest: Level 80', 'Reach level 80 with a Priest.', 1, 5, 80, '', 24, 2, 80),
(126, 'Classes', 'Death Knight: Level 60', 'Reach level 60 with a Death Knight.', 1, 6, 60, '', 25, 0, 60),
(127, 'Classes', 'Death Knight: Level 70', 'Reach level 70 with a Death Knight.', 1, 6, 70, '', 26, 1, 70),
(128, 'Classes', 'Death Knight: Level 80', 'Reach level 80 with a Death Knight.', 1, 6, 80, '', 27, 2, 80),
(129, 'Classes', 'Shaman: Level 20', 'Reach level 20 with a Shaman.', 1, 7, 20, '', 28, 0, 20),
(130, 'Classes', 'Shaman: Level 40', 'Reach level 40 with a Shaman.', 1, 7, 40, '', 29, 0, 40),
(131, 'Classes', 'Shaman: Level 60', 'Reach level 60 with a Shaman.', 1, 7, 60, '', 30, 0, 60),
(132, 'Classes', 'Shaman: Level 70', 'Reach level 70 with a Shaman.', 1, 7, 70, '', 31, 1, 70),
(133, 'Classes', 'Shaman: Level 80', 'Reach level 80 with a Shaman.', 1, 7, 80, '', 32, 2, 80),
(134, 'Classes', 'Mage: Level 20', 'Reach level 20 with a Mage.', 1, 8, 20, '', 33, 0, 20),
(135, 'Classes', 'Mage: Level 40', 'Reach level 40 with a Mage.', 1, 8, 40, '', 34, 0, 40),
(136, 'Classes', 'Mage: Level 60', 'Reach level 60 with a Mage.', 1, 8, 60, '', 35, 0, 60),
(137, 'Classes', 'Mage: Level 70', 'Reach level 70 with a Mage.', 1, 8, 70, '', 36, 1, 70),
(138, 'Classes', 'Mage: Level 80', 'Reach level 80 with a Mage.', 1, 8, 80, '', 37, 2, 80),
(139, 'Classes', 'Warlock: Level 20', 'Reach level 20 with a Warlock.', 1, 9, 20, '', 38, 0, 20),
(140, 'Classes', 'Warlock: Level 40', 'Reach level 40 with a Warlock.', 1, 9, 40, '', 39, 0, 40),
(141, 'Classes', 'Warlock: Level 60', 'Reach level 60 with a Warlock.', 1, 9, 60, '', 40, 0, 60),
(142, 'Classes', 'Warlock: Level 70', 'Reach level 70 with a Warlock.', 1, 9, 70, '', 41, 1, 70),
(143, 'Classes', 'Warlock: Level 80', 'Reach level 80 with a Warlock.', 1, 9, 80, '', 42, 2, 80),
(144, 'Classes', 'Druid: Level 20', 'Reach level 20 with a Druid.', 1, 11, 20, '', 43, 0, 20),
(145, 'Classes', 'Druid: Level 40', 'Reach level 40 with a Druid.', 1, 11, 40, '', 44, 0, 40),
(146, 'Classes', 'Druid: Level 60', 'Reach level 60 with a Druid.', 1, 11, 60, '', 45, 0, 60),
(147, 'Classes', 'Druid: Level 70', 'Reach level 70 with a Druid.', 1, 11, 70, '', 46, 1, 70),
(148, 'Classes', 'Druid: Level 80', 'Reach level 80 with a Druid.', 1, 11, 80, '', 47, 2, 80),
(201, 'Professions', 'Alchemy: 150', 'Reach 150 skill in Alchemy.', 2, 171, 150, '', 48, 0, 15),
(202, 'Professions', 'Alchemy: 225', 'Reach 225 skill in Alchemy.', 2, 171, 225, '', 49, 0, 25),
(203, 'Professions', 'Alchemy: 300', 'Reach 300 skill in Alchemy.', 2, 171, 300, '', 50, 0, 45),
(204, 'Professions', 'Alchemy: 375', 'Reach 375 skill in Alchemy.', 2, 171, 375, '', 51, 1, 65),
(205, 'Professions', 'Alchemy: 450', 'Reach 450 skill in Alchemy.', 2, 171, 450, '', 52, 2, 75),
(206, 'Professions', 'Blacksmithing: 150', 'Reach 150 skill in Blacksmithing.', 2, 164, 150, '', 53, 0, 15),
(207, 'Professions', 'Blacksmithing: 225', 'Reach 225 skill in Blacksmithing.', 2, 164, 225, '', 54, 0, 25),
(208, 'Professions', 'Blacksmithing: 300', 'Reach 300 skill in Blacksmithing.', 2, 164, 300, '', 55, 0, 45),
(209, 'Professions', 'Blacksmithing: 375', 'Reach 375 skill in Blacksmithing.', 2, 164, 375, '', 56, 1, 65),
(210, 'Professions', 'Blacksmithing: 450', 'Reach 450 skill in Blacksmithing.', 2, 164, 450, '', 57, 2, 75),
(211, 'Professions', 'Enchanting: 150', 'Reach 150 skill in Enchanting.', 2, 333, 150, '', 58, 0, 15),
(212, 'Professions', 'Enchanting: 225', 'Reach 225 skill in Enchanting.', 2, 333, 225, '', 59, 0, 25),
(213, 'Professions', 'Enchanting: 300', 'Reach 300 skill in Enchanting.', 2, 333, 300, '', 60, 0, 45),
(214, 'Professions', 'Enchanting: 375', 'Reach 375 skill in Enchanting.', 2, 333, 375, '', 61, 1, 65),
(215, 'Professions', 'Enchanting: 450', 'Reach 450 skill in Enchanting.', 2, 333, 450, '', 62, 2, 75),
(216, 'Professions', 'Engineering: 150', 'Reach 150 skill in Engineering.', 2, 202, 150, '', 63, 0, 15),
(217, 'Professions', 'Engineering: 225', 'Reach 225 skill in Engineering.', 2, 202, 225, '', 64, 0, 25),
(218, 'Professions', 'Engineering: 300', 'Reach 300 skill in Engineering.', 2, 202, 300, '', 65, 0, 45),
(219, 'Professions', 'Engineering: 375', 'Reach 375 skill in Engineering.', 2, 202, 375, '', 66, 1, 65),
(220, 'Professions', 'Engineering: 450', 'Reach 450 skill in Engineering.', 2, 202, 450, '', 67, 2, 75),
(221, 'Professions', 'Jewelcrafting: 150', 'Reach 150 skill in Jewelcrafting.', 2, 755, 150, '', 68, 0, 15),
(222, 'Professions', 'Jewelcrafting: 225', 'Reach 225 skill in Jewelcrafting.', 2, 755, 225, '', 69, 0, 25),
(223, 'Professions', 'Jewelcrafting: 300', 'Reach 300 skill in Jewelcrafting.', 2, 755, 300, '', 70, 0, 45),
(224, 'Professions', 'Jewelcrafting: 375', 'Reach 375 skill in Jewelcrafting.', 2, 755, 375, '', 71, 1, 65),
(225, 'Professions', 'Jewelcrafting: 450', 'Reach 450 skill in Jewelcrafting.', 2, 755, 450, '', 72, 2, 75),
(226, 'Professions', 'Leatherworking: 150', 'Reach 150 skill in Leatherworking.', 2, 165, 150, '', 73, 0, 15),
(227, 'Professions', 'Leatherworking: 225', 'Reach 225 skill in Leatherworking.', 2, 165, 225, '', 74, 0, 25),
(228, 'Professions', 'Leatherworking: 300', 'Reach 300 skill in Leatherworking.', 2, 165, 300, '', 75, 0, 45),
(229, 'Professions', 'Leatherworking: 375', 'Reach 375 skill in Leatherworking.', 2, 165, 375, '', 76, 1, 65),
(230, 'Professions', 'Leatherworking: 450', 'Reach 450 skill in Leatherworking.', 2, 165, 450, '', 77, 2, 75),
(231, 'Professions', 'Tailoring: 150', 'Reach 150 skill in Tailoring.', 2, 197, 150, '', 78, 0, 15),
(232, 'Professions', 'Tailoring: 225', 'Reach 225 skill in Tailoring.', 2, 197, 225, '', 79, 0, 25),
(233, 'Professions', 'Tailoring: 300', 'Reach 300 skill in Tailoring.', 2, 197, 300, '', 80, 0, 45),
(234, 'Professions', 'Tailoring: 375', 'Reach 375 skill in Tailoring.', 2, 197, 375, '', 81, 1, 65),
(235, 'Professions', 'Tailoring: 450', 'Reach 450 skill in Tailoring.', 2, 197, 450, '', 82, 2, 75),
(236, 'Professions', 'Inscription: 150', 'Reach 150 skill in Inscription.', 2, 773, 150, '', 83, 0, 15),
(237, 'Professions', 'Inscription: 225', 'Reach 225 skill in Inscription.', 2, 773, 225, '', 84, 0, 25),
(238, 'Professions', 'Inscription: 300', 'Reach 300 skill in Inscription.', 2, 773, 300, '', 85, 0, 45),
(239, 'Professions', 'Inscription: 375', 'Reach 375 skill in Inscription.', 2, 773, 375, '', 86, 1, 65),
(240, 'Professions', 'Inscription: 450', 'Reach 450 skill in Inscription.', 2, 773, 450, '', 87, 2, 75),
(301, 'Dungeons', 'Novice Spelunker', 'Complete Ragefire Chasm, The Deadmines, Wailing Caverns, Shadowfang Keep, Blackfathom Deeps and The Stockade.', 3, 0, 0, '629,628,630,631,632,633', 88, 0, 24),
(302, 'Dungeons', 'Experienced Spelunker', 'Complete Gnomeregan, Razorfen Kraul, Scarlet Monastery, Razorfen Downs, Uldaman, Zul''Farrak and Maraudon.', 3, 0, 0, '634,635,637,636,638,639,640', 89, 0, 45),
(303, 'Dungeons', 'Master Spelunker', 'Complete the Sunken Temple, Blackrock Depths, Lower and Upper Blackrock Spire, Dire Maul, Scholomance and Stratholme.', 3, 0, 0, '641,642,643,1307,644,645,646', 90, 0, 58),
(304, 'Dungeons', 'Outland Spelunker', 'Earn the achievement Outland Dungeonmaster.', 3, 0, 0, '1284', 91, 1, 70),
(305, 'Dungeons', 'Northrend Spelunker', 'Earn the achievement Northrend Dungeonmaster.', 3, 0, 0, '1288', 92, 2, 80),
(401, 'Raids', 'Onyxia''s Lair', 'Earn the achievement Onyxia''s Lair.', 3, 0, 0, '684', 93, 0, 62),
(402, 'Raids', 'Molten Core', 'Earn the achievement Molten Core.', 3, 0, 0, '686', 94, 0, 62),
(403, 'Raids', 'Blackwing Lair', 'Earn the achievement Blackwing Lair.', 3, 0, 0, '685', 95, 0, 63),
(404, 'Raids', 'Zul''Gurub', 'Earn the achievement Zul''Gurub.', 3, 0, 0, '688', 96, 0, 61),
(405, 'Raids', 'Ruins of Ahn''Qiraj', 'Earn the achievement Ruins of Ahn''Qiraj.', 3, 0, 0, '689', 97, 0, 61),
(406, 'Raids', 'Temple of Ahn''Qiraj', 'Earn the achievement Temple of Ahn''Qiraj.', 3, 0, 0, '687', 98, 0, 64),
(407, 'Raids', 'Naxxramas (Level 60)', 'Earn the achievement Naxxramas (Level 60).', 3, 0, 0, '533', 99, 0, 66),
(408, 'Raids', 'Karazhan', 'Earn the achievement Karazhan.', 3, 0, 0, '690', 100, 1, 71),
(409, 'Raids', 'Gruul''s Lair', 'Earn the achievement Gruul''s Lair.', 3, 0, 0, '692', 101, 1, 72),
(410, 'Raids', 'Magtheridon''s Lair', 'Earn the achievement Magtheridon''s Lair.', 3, 0, 0, '693', 102, 1, 72),
(411, 'Raids', 'Zul''Aman', 'Earn the achievement Zul''Aman.', 3, 0, 0, '691', 103, 1, 73),
(412, 'Raids', 'Serpentshrine Cavern', 'Earn the achievement Serpentshrine Cavern.', 3, 0, 0, '694', 104, 1, 73),
(413, 'Raids', 'Tempest Keep', 'Earn the achievement Tempest Keep.', 3, 0, 0, '696', 105, 1, 74),
(414, 'Raids', 'The Battle for Mount Hyjal', 'Earn the achievement The Battle for Mount Hyjal.', 3, 0, 0, '695', 106, 1, 75),
(415, 'Raids', 'The Black Temple', 'Earn the achievement The Black Temple.', 3, 0, 0, '697', 107, 1, 75),
(416, 'Raids', 'Sunwell Plateau', 'Earn the achievement Sunwell Plateau.', 3, 0, 0, '698', 108, 1, 76),
(417, 'Raids', 'The Fall of Naxxramas', 'Earn the achievement The Fall of Naxxramas (10 or 25 player).', 4, 0, 0, '576,577', 109, 2, 81),
(418, 'Raids', 'Besting the Black Dragonflight', 'Earn the achievement Besting the Black Dragonflight (10 or 25 player).', 4, 0, 0, '1876,625', 110, 2, 81),
(419, 'Raids', 'The Spellweaver''s Downfall', 'Earn the achievement The Spellweaver''s Downfall (10 or 25 player).', 4, 0, 0, '622,623', 111, 2, 82),
(420, 'Raids', 'Archavon the Stone Watcher', 'Earn the achievement Archavon the Stone Watcher (10 or 25 player).', 4, 0, 0, '1722,1721', 112, 2, 81),
(421, 'Raids', 'The Secrets of Ulduar', 'Earn the achievement The Secrets of Ulduar (10 or 25 player).', 4, 0, 0, '2894,2895', 113, 2, 83),
(422, 'Raids', 'Call of the Crusade', 'Earn the achievement Call of the Crusade (10 or 25 player).', 4, 0, 0, '3917,3916', 114, 2, 84),
(423, 'Raids', 'Onyxia''s Lair (Level 80)', 'Earn the achievement Onyxia''s Lair (10 or 25 player), the level 80 raid.', 4, 0, 0, '4396,4397', 115, 2, 82),
(424, 'Raids', 'Fall of the Lich King', 'Earn the achievement Fall of the Lich King (10 or 25 player).', 4, 0, 0, '4532,4608', 116, 2, 86),
(425, 'Raids', 'The Twilight Destroyer', 'Earn the achievement The Twilight Destroyer (10 or 25 player).', 4, 0, 0, '4817,4815', 117, 2, 85),
(501, 'Exploration', 'Explore Eastern Kingdoms', 'Earn the achievement Explore Eastern Kingdoms.', 3, 0, 0, '42', 118, 0, 58),
(502, 'Exploration', 'Explore Kalimdor', 'Earn the achievement Explore Kalimdor.', 3, 0, 0, '43', 119, 0, 58),
(503, 'Exploration', 'Explore Outland', 'Earn the achievement Explore Outland.', 3, 0, 0, '44', 120, 1, 68),
(504, 'Exploration', 'Explore Northrend', 'Earn the achievement Explore Northrend.', 3, 0, 0, '45', 121, 2, 78),
(601, 'PvP', 'Exalted: Warsong Gulch', 'Reach Exalted with the Silverwing Sentinels or the Warsong Outriders.', 5, 0, 0, '890,889', 122, 0, 65),
(602, 'PvP', 'Exalted: Arathi Basin', 'Reach Exalted with the League of Arathor or the Defilers.', 5, 0, 0, '509,510', 123, 0, 64),
(603, 'PvP', 'Exalted: Alterac Valley', 'Reach Exalted with the Stormpike Guard or the Frostwolf Clan.', 5, 0, 0, '730,729', 124, 0, 61),
(651, 'Challenges', 'Immortal', 'Reach level 60 in the Hardcore challenge without dying: the achievement Immortal.', 3, 0, 0, '9260', 125, 0, 66),
(652, 'Challenges', 'Deathless', 'Reach level 70 in the Hardcore challenge without dying: the achievement Deathless.', 3, 0, 0, '9261', 126, 1, 76),
(653, 'Challenges', 'Eternal', 'Reach level 80 in the Hardcore challenge without dying: the achievement Eternal.', 3, 0, 0, '9262', 127, 2, 86),
(654, 'Challenges', 'Hambringer', 'Reach level 60 in Boaring Adventure, killing nothing but boars: the achievement Hambringer.', 3, 0, 0, '9271', 128, 0, 64),
(655, 'Challenges', 'That Which Has No Life', 'Reach level 60 in Boaring Adventure together with Exhaustion and Slow XP: the achievement That Which Has No Life.', 3, 0, 0, '9272', 129, 0, 66),
(656, 'Challenges', 'Iron Man: Level 60', 'Reach level 60 in the Iron Man challenge without dying.', 6, 2, 0, '', 130, 0, 66),
(657, 'Challenges', 'Self-Found: Level 60', 'Reach level 60 in the Self-Found challenge.', 6, 4, 0, '', 131, 0, 64),
(658, 'Challenges', 'Self-Crafted: Level 60', 'Reach level 60 in the Self-Crafted challenge.', 6, 8, 0, '', 132, 0, 64),
(659, 'Challenges', 'Slow XP: Level 60', 'Reach level 60 in the Slow XP challenge.', 6, 16, 0, '', 133, 0, 62),
(660, 'Challenges', 'Quest XP Only: Level 60', 'Reach level 60 in the Quest XP Only challenge.', 6, 32, 0, '', 134, 0, 63),
(661, 'Challenges', 'Item Quality Level: Level 60', 'Reach level 60 in the Item Quality Level challenge.', 6, 64, 0, '', 135, 0, 62),
(662, 'Challenges', 'Exhaustion: Level 60', 'Reach level 60 in the Exhaustion challenge.', 6, 256, 0, '', 136, 0, 61);

-- Hidden passive auras the client never sees (Attributes 0x40 passive + 0x80 hidden). The
-- module sets their amounts:
--   90480 High Alert: Mod Stealth Detect (17), general stealth.
--   90481 Explorer's Stride: Mod Speed Always (129), unmounted run speed.
--   90482 The Quick and the Dead: Mod Speed Always (129), the ghost's run speed.
--   90490-90499 Reagent Economy, one per class: No Reagent Use (256). The module fills in each
--          one's spell class mask at startup, from the class spells whose reagents a vendor sells.
DELETE FROM `spell_dbc` WHERE `ID` IN (90480, 90481, 90482) OR `ID` BETWEEN 90490 AND 90499;
INSERT INTO `spell_dbc`
    (`ID`, `Attributes`, `AttributesEx`, `CastingTimeIndex`, `DurationIndex`, `RangeIndex`, `EquippedItemClass`,
     `Effect_1`, `EffectDieSides_1`, `EffectBasePoints_1`, `ImplicitTargetA_1`, `EffectAura_1`,
     `Effect_2`, `EffectDieSides_2`, `EffectBasePoints_2`, `ImplicitTargetA_2`, `EffectAura_2`,
     `SpellIconID`, `Name_Lang_enUS`, `Name_Lang_Mask`, `SchoolMask`,
     `EffectMultipleValue_1`, `EffectMultipleValue_2`, `EffectChainAmplitude_1`, `EffectChainAmplitude_2`,
     `EffectBonusMultiplier_1`, `EffectBonusMultiplier_2`)
VALUES
    (90480, 192, 268435456, 1, 21, 1, -1,
     6, 1, 4, 1, 17,
     0, 0, 0, 0, 0,
     1, 'High Alert', 16712190, 1,
     0, 0, 1, 0, 1, 0),
    (90481, 192, 268435456, 1, 21, 1, -1,
     6, 1, 1, 1, 129,
     0, 0, 0, 0, 0,
     1, 'Explorer''s Stride', 16712190, 1,
     0, 0, 1, 0, 1, 0),
    (90482, 192, 268435456, 1, 21, 1, -1,
     6, 1, 4, 1, 129,
     0, 0, 0, 0, 0,
     1, 'The Quick and the Dead', 16712190, 1,
     0, 0, 1, 0, 1, 0),
    (90490, 192, 268435456, 1, 21, 1, -1,
     6, 1, 0, 1, 256,
     0, 0, 0, 0, 0,
     1, 'Reagent Economy (Warrior)', 16712190, 1,
     0, 0, 1, 0, 1, 0),
    (90491, 192, 268435456, 1, 21, 1, -1,
     6, 1, 0, 1, 256,
     0, 0, 0, 0, 0,
     1, 'Reagent Economy (Paladin)', 16712190, 1,
     0, 0, 1, 0, 1, 0),
    (90492, 192, 268435456, 1, 21, 1, -1,
     6, 1, 0, 1, 256,
     0, 0, 0, 0, 0,
     1, 'Reagent Economy (Hunter)', 16712190, 1,
     0, 0, 1, 0, 1, 0),
    (90493, 192, 268435456, 1, 21, 1, -1,
     6, 1, 0, 1, 256,
     0, 0, 0, 0, 0,
     1, 'Reagent Economy (Rogue)', 16712190, 1,
     0, 0, 1, 0, 1, 0),
    (90494, 192, 268435456, 1, 21, 1, -1,
     6, 1, 0, 1, 256,
     0, 0, 0, 0, 0,
     1, 'Reagent Economy (Priest)', 16712190, 1,
     0, 0, 1, 0, 1, 0),
    (90495, 192, 268435456, 1, 21, 1, -1,
     6, 1, 0, 1, 256,
     0, 0, 0, 0, 0,
     1, 'Reagent Economy (Death Knight)', 16712190, 1,
     0, 0, 1, 0, 1, 0),
    (90496, 192, 268435456, 1, 21, 1, -1,
     6, 1, 0, 1, 256,
     0, 0, 0, 0, 0,
     1, 'Reagent Economy (Shaman)', 16712190, 1,
     0, 0, 1, 0, 1, 0),
    (90497, 192, 268435456, 1, 21, 1, -1,
     6, 1, 0, 1, 256,
     0, 0, 0, 0, 0,
     1, 'Reagent Economy (Mage)', 16712190, 1,
     0, 0, 1, 0, 1, 0),
    (90498, 192, 268435456, 1, 21, 1, -1,
     6, 1, 0, 1, 256,
     0, 0, 0, 0, 0,
     1, 'Reagent Economy (Warlock)', 16712190, 1,
     0, 0, 1, 0, 1, 0),
    (90499, 192, 268435456, 1, 21, 1, -1,
     6, 1, 0, 1, 256,
     0, 0, 0, 0, 0,
     1, 'Reagent Economy (Druid)', 16712190, 1,
     0, 0, 1, 0, 1, 0);
