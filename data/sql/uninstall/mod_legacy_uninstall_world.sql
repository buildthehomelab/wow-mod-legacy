-- mod-legacy: undo the module's world database changes. Run it by hand on the world database
-- after removing the module, with the server stopped. Idempotent: safe to run again.

DROP TABLE IF EXISTS `mod_legacy_challenge`;
DELETE FROM `spell_dbc` WHERE `ID` IN (90480, 90481, 90482) OR `ID` BETWEEN 90490 AND 90499;

DELETE FROM `updates` WHERE `name` LIKE 'mod\_legacy\_%';
