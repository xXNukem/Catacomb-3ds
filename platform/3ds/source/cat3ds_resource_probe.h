#ifndef CAT3DS_RESOURCE_PROBE_H
#define CAT3DS_RESOURCE_PROBE_H

#include <stddef.h>

/* Read-only structural check, independent of edition-specific CRC32.
 * 1..3: Abyss, Armageddon, Apocalypse. The already verified Catacomb 3-D
 * title path is unchanged. This is not yet an engine compatibility guarantee.
 */
struct Cat3DSResourceProbe {
    bool graphicsValid;
    bool audioAndMapsPresent;
    char exeName[128];
    char graphicsName[128];
    char problem[128];
    unsigned imageSize;
    unsigned headOffset;
    unsigned dictOffset;
    // A validated layout is required before ReflectionHLE can load an unknown
    // game executable. Graphics-only validation is not sufficient.
    bool embeddedLayoutValid;
    unsigned audioDictOffset;
    unsigned audioHeadOffset;
    unsigned mapHeadOffset;
    unsigned audioDictCrc, audioHeadCrc, egaDictCrc, egaHeadCrc, mapHeadCrc;
};

void cat3dsProbeResources(int game, const char *folder, Cat3DSResourceProbe *out);
/* Shows the original in-game STATUSPIC bitmap (not the intro or title).
 * Uses existing native EGA backend; never writes any game files. */
bool cat3dsShowOriginalStatusGraphic(int game, const char *folder,
                                    char *error, size_t errorLength);

#endif
