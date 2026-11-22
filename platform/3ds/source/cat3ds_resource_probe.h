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
    unsigned graphicsSize, graphicsCrc, audioSize, audioArchiveCrc, mapsSize, mapsArchiveCrc;
    unsigned imageSize;
    unsigned headOffset;
    unsigned dictOffset;
    // A validated layout is required before ReflectionHLE can load an unknown
    // game executable. Graphics-only validation is not sufficient.
    bool embeddedLayoutValid;
    bool plainMZ;
    unsigned audioDictOffset;
    unsigned audioHeadOffset;
    unsigned mapHeadOffset;
    unsigned audioDictCrc, audioHeadCrc, egaDictCrc, egaHeadCrc, mapHeadCrc;
};

// Fingerprints from read-only inspection of earlier DOS releases. Offsets
// always come from the validated load image, including for renamed wrappers.
struct Cat3DSEarlierRelease {
    const char *edition;
    unsigned imageSize, graphicsSize, graphicsCrc, audioSize, audioCrc, mapsSize, mapsCrc;
    unsigned audioDict, audioHead, egaDict, egaHead, mapHead;
};

static inline const Cat3DSEarlierRelease *cat3dsEarlierRelease(int game, const Cat3DSResourceProbe &p,
                                                            const char **difference = nullptr)
{
    static const Cat3DSEarlierRelease releases[] = {
        {"Abyss registered 1.23", 194592, 335969, 0x8c8d9023, 6904, 0x462d2eb2, 20433, 0x97ddb354,
         0xe9088011, 0xfbfff495, 0xe130eb8a, 0x0486de41, 0x5d9ccfb3},
        {"Armageddon 1.01", 190528, 355803, 0x5c2ef367, 7104, 0xb6c28bc7, 17653, 0x787794bf,
         0x8f1d4dd2, 0x5f863ad2, 0xbde1147a, 0xdc26e8b7, 0x546f00d1},
        {"Apocalypse 1.00", 191760, 382076, 0x3c0575ea, 7366, 0x23934cec, 18855, 0xd317b2c9,
         0x26658498, 0x76adb051, 0x006b7a41, 0x4efe4b5d, 0x90742162}
    };
    if (game < 1 || game > 3 || !p.embeddedLayoutValid) return nullptr;
    const auto &r = releases[game-1];
    struct Field { const char *name; unsigned actual, expected; };
    const Field fields[] = {
        {"load image size", p.imageSize, r.imageSize},
        {"EGAGRAPH size", p.graphicsSize, r.graphicsSize},
        {"EGAGRAPH CRC", p.graphicsCrc, r.graphicsCrc},
        {"AUDIO size", p.audioSize, r.audioSize},
        {"AUDIO CRC", p.audioArchiveCrc, r.audioCrc},
        {"GAMEMAPS size", p.mapsSize, r.mapsSize},
        {"GAMEMAPS CRC", p.mapsArchiveCrc, r.mapsCrc},
        {"AUDIODCT CRC", p.audioDictCrc, r.audioDict},
        {"AUDIOHHD CRC", p.audioHeadCrc, r.audioHead},
        {"EGADICT CRC", p.egaDictCrc, r.egaDict},
        {"EGAHEAD CRC", p.egaHeadCrc, r.egaHead},
        {"MTEMP CRC", p.mapHeadCrc, r.mapHead}
    };
    for (const auto &field : fields) {
        if (field.actual == field.expected) continue;
        if (difference && (p.graphicsCrc == r.graphicsCrc || p.imageSize == r.imageSize))
            *difference = field.name;
        return nullptr;
    }
    return &r;
}

/* The two published Abyss gameplay images use different DOS state pointer
 * tables. Identify their graphics pair independently of the wrapper name. */
static inline int cat3dsAbyssEdition(const Cat3DSResourceProbe &probe)
{
    if (!probe.embeddedLayoutValid) return 0;
    // Preserve the pre-L1 engine selection for the measured CATABYSS release.
    if (cat3dsEarlierRelease(1, probe)) return 113;
    const bool shareware = probe.egaDictCrc == 0xbb760f1du || probe.egaHeadCrc == 0xe31e1c3bu ||
        (probe.imageSize == 201120-0x1a00 && probe.audioDictOffset == 0x2554c &&
         probe.audioHeadOffset == 0x1a210 && probe.dictOffset == 0x2594c &&
         probe.headOffset == 0x1a3b0 && probe.mapHeadOffset == 0x1ab10);
    const bool registered = probe.egaDictCrc == 0x63eb06d3u || probe.egaHeadCrc == 0x94967205u ||
        (probe.imageSize == 200848-0x1a00 && probe.audioDictOffset == 0x2543a &&
         probe.audioHeadOffset == 0x1a140 && probe.dictOffset == 0x2583a &&
         probe.headOffset == 0x1a2e0 && probe.mapHeadOffset == 0x1aa40);
    if (shareware == registered) return 0;
    return shareware ? 113 : 124;
}

void cat3dsProbeResources(int game, const char *folder, Cat3DSResourceProbe *out);
/* Shows the original in-game STATUSPIC bitmap (not the intro or title).
 * Uses existing native EGA backend; never writes any game files. */
bool cat3dsShowOriginalStatusGraphic(int game, const char *folder,
                                    char *error, size_t errorLength);

#endif
