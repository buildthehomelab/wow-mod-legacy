/*
 * mod-legacy loader.
 *
 * AzerothCore looks up a loader symbol derived from the module's folder name: for folder
 * "mod-legacy" that symbol is exactly "Addmod_legacyScripts". If you clone the repo under a
 * different folder name, rename this function to match.
 *
 * Released under the MIT License.
 */

void AddLegacyDataScripts();
void AddLegacyPerkScripts();
void AddLegacyCommandScripts();

void Addmod_legacyScripts()
{
    AddLegacyDataScripts();
    AddLegacyPerkScripts();
    AddLegacyCommandScripts();
}
