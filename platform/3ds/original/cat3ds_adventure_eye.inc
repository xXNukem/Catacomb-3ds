/* Included in each adventure namespace after its original sprite and hand routines. */
/* CAT3DS_NEAR_WALL_SCALER_V2: linked by each game namespace's c3_asm.c. */
extern id0_word_t cat3dsFullWallScale[VIEWWIDTH];
static void Cat3DS_DrawAdventureViews(int sliderOffset)
{
    static id0_byte_t centerVisible[sizeof spotvis];
    const fixed centerX = viewx, centerY = viewy;
    // The LCD samples the middle 240 of 320 DOS columns. Shorten the
    // focal length in source pixels to preserve the horizontal field and
    // gain vertical coverage without borrowing rows from the status panel.
    const int aperture = 240;
    const int firstColumn = (VIEWWIDTH - aperture) / 2;
    const fixed savedScale = scale;
    scale = (scale * VIEWWIDTH + aperture/2) / aperture;
    const double focalPixels = (double)FOCALLENGTH / scale;
    fixed separation = Cat3DS_Baseline(sliderOffset, aperture, focalPixels);
    const double forwardX = (double)FixedByFrac(GLOBAL1, viewcos) / TILEGLOBAL;
    const double forwardY = -(double)FixedByFrac(GLOBAL1, viewsin) / TILEGLOBAL;

    separation = Cat3DS_LimitCornerOffset(separation, tilemap[0], MAPSIZE, mapwidth, mapheight,
        (double)centerX / TILEGLOBAL, (double)centerY / TILEGLOBAL, forwardX, forwardY,
        aperture, focalPixels);
    separation = Cat3DS_LimitActorOffset(separation, player, forwardX, forwardY, aperture, focalPixels);
    if (!separation) Cat3DS_DualScreenForgetStereo();
    cat3dsStereoOcclusionEnabled = 1;
    // Use the centered DDA for simulation visibility in both display modes.
    for (int pass = 0; pass < (separation ? 3 : 1); ++pass) {
        const int eye = pass == 1 ? -1 : pass == 2 ? 1 : 0;
        centerActorsOnly = separation && !eye;
        privateEyeActors = eye != 0;
        viewx = centerX + eye * FixedByFrac(separation, viewsin);
        viewy = centerY + eye * FixedByFrac(separation, viewcos);
        eyeLensShift = Cat3DS_LensShift((double)(eye * separation) / TILEGLOBAL, focalPixels);
        if (separation) memset(spotvis, 0, sizeof spotvis);
        memset(wallwidth, 0, sizeof wallwidth);
        memset(zbuffer, 0, sizeof zbuffer);
        EGAWRITEMODE(2);
        EGAMAPMASK(15);
        if (!centerActorsOnly) ClearScreen();
        for (int x = firstColumn; x < firstColumn + aperture; ++x) {
            const double side = (x - (CENTERX + 0.5 + eyeLensShift)) / focalPixels;
            const Cat3DSWallHit hit = Cat3DS_TraceColumn(tilemap[0], spotvis[0], MAPSIZE,
                mapwidth, mapheight, (double)viewx / TILEGLOBAL, (double)viewy / TILEGLOBAL,
                forwardX - forwardY * side, forwardY + forwardX * side, 0.0);
            if (centerActorsOnly) continue;
            cat3dsWallDepth[x] = (fixed)(hit.depth * TILEGLOBAL);
            if (!hit.tile || hit.tile >= NUMFLOORS) continue;
#ifdef REFKEEN_VER_CATABYSS
            const int pic = hit.side ? (animframe ? walllight2 : walllight1)[hit.tile] :
                                       (animframe ? walldark2 : walldark1)[hit.tile];
#else
            const int frame = hit.tile + wall_anim_pos[hit.tile];
            if (frame < 0 || frame >= NUMFLOORS) continue;
            const int pic = (hit.side ? walllight1 : walldark1)[frame];
#endif
            const int index = pic - FIRSTWALLPIC;
            if (index < 0 || index >= NUMSCALEWALLS || !walldirectory[index]) continue;
            /* CAT3DS_NEAR_WALL_SCALER_V2: don't squeeze oversized
             * nearby wall textures to the legacy scaler maximum. */
            const int fullHeight = (int)fmin(CAT3DS_NEAR_WALL_SCALE_LIMIT,
                                         fmax(1.0, TILEGLOBAL / (2.0 * hit.depth * scale)));
            const int height = fullHeight > MAXSCALE ? MAXSCALE : fullHeight;
            cat3dsFullWallScale[x] = (id0_word_t)fullHeight;
            zbuffer[x] = wallheight[x] = height;
            wallwidth[x] = 1;
            wallofs[x] = hit.column * 64;
            wallseg[x] = (id0_byte_t *)walldirectory[index];
        }
        if (!centerActorsOnly) ScaleWalls();
        DrawScaleds();
        if (centerActorsOnly) memcpy(centerVisible, spotvis, sizeof spotvis);
        if (eye < 0) {
            EGAWRITEMODE(0);
            EGABITMASK(0xff);
            if (handheight) DrawHand();
            Cat3DS_DualScreenCaptureLeftEGA(bufferofs);
        }
    }
    scale = savedScale;
    privateEyeActors = false;
    cat3dsStereoOcclusionEnabled = 0;
    viewx = centerX;
    viewy = centerY;
    if (separation) memcpy(spotvis, centerVisible, sizeof spotvis);
}
