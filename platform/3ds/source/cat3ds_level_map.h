#ifndef CAT3DS_LEVEL_MAP_H
#define CAT3DS_LEVEL_MAP_H

#include <stddef.h>
#include <stdint.h>

/* Decoded original DOS map planes (id_ca.c: CA_CacheMap). A data-loading
 * milestone, not the Catacomb 3-D game runtime or its collision rules. */
struct Cat3DSLevelMap {
    uint16_t walls[64 * 64];
    uint16_t objects[64 * 64];
    unsigned width, height, index;
    char name[17];
};

/* Reads game data, writes nothing. Requires a recognized Catacomb 3-D
 * v1.00 or v1.22 installation; returns a descriptive failure on error. */
bool cat3dsLoadCat3dMap(const char *version, const char *exeFilename,
                        const char *folder, unsigned mapIndex,
                        Cat3DSLevelMap *map, char *error, size_t errorSize);

#endif
