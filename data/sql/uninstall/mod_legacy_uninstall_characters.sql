-- mod-legacy: undo the module's characters database changes. Run it by hand on the characters
-- database after removing the module, with the server stopped.
--
-- Drops the challenges every account has done (their Legacy points) and every character's
-- perks. Idempotent: safe to run again.

DROP TABLE IF EXISTS `mod_legacy_challenge_done`;
DROP TABLE IF EXISTS `mod_legacy_rank`;
DROP TABLE IF EXISTS `mod_legacy_character`;

DELETE FROM `updates` WHERE `name` LIKE 'mod\_legacy\_%';
