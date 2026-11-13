#ifndef CAT3DS_TITLE_PREVIEW_H
#define CAT3DS_TITLE_PREVIEW_H

#include <stddef.h>

/* Loads the original Catacomb 3-D TITLEPIC from the user's DOS game files.
 * Writes into ReflectionHLE's EGA emulated video memory.
 * Does not contain or distribute any original game assets.
 */
bool cat3dsLoadCat3dTitle(const char *versionName, const char *exeFilename, char *error, size_t errorSize);

#endif
