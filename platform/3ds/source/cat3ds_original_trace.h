#ifndef CAT3DS_ORIGINAL_TRACE_H
#define CAT3DS_ORIGINAL_TRACE_H

#include <stdint.h>
#include "cat3ds_level_map.h"
#include "cat3ds_walk_preview.h"

/* First isolated integration of Catacomb 3-D's original c3_draw.c camera
 * geometry / TraceRay concept. Used for validation only: FollowWalls(),
 * DrawWallList(), object drawing, and the gameplay loop aren't linked yet. */
struct Cat3DSOriginalTrace {
    int leftX, leftY, rightX, rightY;
    uint16_t leftWall, rightWall;
    int firstAngleDegrees;
    unsigned stepsLeft, stepsRight;
};

/* Returns false on malformed/unenclosed map or invalid camera. Never reads
 * outside map data; unlike DOS TraceRay, bounded to avoid infinite loops. */
bool cat3dsTraceOriginalViewEdges(const Cat3DSLevelMap &map,
                                 const Cat3DSWalkPreview &camera,
                                 Cat3DSOriginalTrace *result);
#endif
