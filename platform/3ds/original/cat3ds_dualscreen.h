/* Catacomb 3DS - dual-screen presentation of the original Catacomb 3-D engine.
 * Copyright (C) 2026 xXNukem. SPDX-License-Identifier: GPL-2.0-or-later
 * No game asset data is embedded in this module.
 */
#ifndef CAT3DS_DUALSCREEN_H
#define CAT3DS_DUALSCREEN_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Called by the ORIGINAL game loop, never by the former preview raycaster. */
void Cat3DS_DualScreenEnterMenu(void);
void Cat3DS_DualScreenLeaveMenu(void);
void Cat3DS_DualScreenShutdown(void);
void Cat3DS_DualScreenStartAdventure(void);
void Cat3DS_DualScreenSetGameplay(int enabled);
void Cat3DS_DualScreenBeginLevel(int level, int mapWidth, int mapHeight);
void Cat3DS_DualScreenObserve(int level, int mapWidth, int mapHeight,
                               const uint8_t *tilemapXMajor,
                               const uint8_t *visibleXMajor,
                               int32_t playerX, int32_t playerY, int angle);
int Cat3DS_DualScreenHasGameplayFrame(void);
int Cat3DS_DualScreenHandlesMode(int mode);
/* Time spent in the real ReflectionHLE 3D renderer (timer ticks). */
void Cat3DS_DualScreenReportRenderTicks(uint64_t ticks);
/* Real left/right 3D views using two passes of original ThreeDRefresh. */
int Cat3DS_DualScreenStereoOffset(void);
void Cat3DS_DualScreenCaptureLeftEGA(uint16_t egaPage);
void Cat3DS_DualScreenForgetStereo(void);
/* 2D overlays (scrolls, modal text) must not use stale stereo eyes. */
void Cat3DS_DualScreenSetModalOverlay(int active);
/* Mono level plaque persists until first complete new world frame. */
void Cat3DS_DualScreenBeginLevelTransition(void);

/* Called by ReflectionHLE's software-video presentation, after EGA conversion. */
void Cat3DS_DualScreenPresent(const uint8_t *egaPixels, int pitch,
                              const uint32_t *palette);
#ifdef __cplusplus
}
#endif
#endif
