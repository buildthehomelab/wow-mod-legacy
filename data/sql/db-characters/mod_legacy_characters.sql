-- mod-legacy: the challenges each account has done, and each character's perks.

-- A challenge done once on the account, by the first character that did it. A hardcore
-- character (mod-challenges) doing it fills in the hardcore columns too, whether or not a
-- normal character did it first: hardcore characters spend only those points.
CREATE TABLE IF NOT EXISTS `mod_legacy_challenge_done` (
    `account` INT UNSIGNED NOT NULL,
    `challenge` SMALLINT UNSIGNED NOT NULL COMMENT 'mod_legacy_challenge.id',
    `guid` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'first character that did it',
    `done_at` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'unix time',
    `hardcore_guid` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'first hardcore character that did it, 0 = none yet',
    `hardcore_at` INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`account`, `challenge`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='mod-legacy: challenges done per account';

-- A character's Legacy perks: one row per perk with a rank above 0.
CREATE TABLE IF NOT EXISTS `mod_legacy_rank` (
    `guid` INT UNSIGNED NOT NULL COMMENT 'characters.guid',
    `perk` VARCHAR(40) NOT NULL,
    `rank` TINYINT UNSIGNED NOT NULL,
    PRIMARY KEY (`guid`, `perk`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='mod-legacy: perk ranks per character';

-- Dedicated Study: when a character last got its daily skill point.
CREATE TABLE IF NOT EXISTS `mod_legacy_character` (
    `guid` INT UNSIGNED NOT NULL COMMENT 'characters.guid',
    `study_at` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'unix time',
    PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='mod-legacy: per-character state';
