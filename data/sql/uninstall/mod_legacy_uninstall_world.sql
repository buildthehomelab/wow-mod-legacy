-- mod-legacy: undo the module's world database changes. Run it by hand on the world database
-- after removing the module, with the server stopped. Idempotent: safe to run again.

DROP TABLE IF EXISTS `mod_legacy_challenge`;
DELETE FROM `spell_dbc` WHERE `ID` BETWEEN 90480 AND 90487 OR `ID` BETWEEN 90490 AND 90499;
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_legacy_second_home';
-- Item 40582 goes back to TrinityCore's unused placeholder row (the Second Home stone before it).
DELETE FROM `item_template` WHERE `entry` = 40582;
INSERT INTO `item_template` (`entry`, `class`, `subclass`, `name`, `displayid`, `BuyCount`, `stackable`, `delay`, `Material`, `VerifiedBuild`) VALUES
(40582, 15, 0, '[TDB PH] - unsused Scourgestone', 6418, 1, 1, 1000, 4, 1);

DELETE FROM `updates` WHERE `name` LIKE 'mod\_legacy\_%';
