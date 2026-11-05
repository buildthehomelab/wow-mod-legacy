-- mod-legacy: undo the module's characters database changes. Run it by hand on the characters
-- database after removing the module, with the server stopped.
--
-- Drops the challenges every account has done (their Legacy points) and every character's
-- perks. Idempotent: safe to run again.

DROP TABLE IF EXISTS `mod_legacy_challenge_done`;
DROP TABLE IF EXISTS `mod_legacy_rank`;
DROP TABLE IF EXISTS `mod_legacy_character`;
DROP TABLE IF EXISTS `mod_legacy_home`;
-- Second Home stones and Personal Banking's saved cooldown.
DELETE FROM `character_inventory` WHERE `item` IN (SELECT `guid` FROM `item_instance` WHERE `itemEntry` = 40582);
DELETE FROM `item_instance` WHERE `itemEntry` = 40582;
DELETE FROM `character_spell_cooldown` WHERE `spell` = 90487;

DELETE FROM `updates` WHERE `name` LIKE 'mod\_legacy\_%';
