/* Included in each adventure namespace after its original sprite and hand routines. */
static void Cat3DS_DrawAdventureViews(int sliderOffset)
{
    static id0_byte_t centerVisible[sizeof spotvis];
    const fixed centerX = viewx, centerY = viewy;
    const double focalPixels = (double)FOCALLENGTH / scale;
    /* Match the physical LCD disparity budget of the 264-wide Catacomb camera. */
    fixed separation = (fixed)(sliderOffset * (65536.0 / 378.0) / focalPixels *
                                     VIEWWIDTH / 264.0 + 0.5);
    const double forwardX = (double)FixedByFrac(GLOBAL1, viewcos) / TILEGLOBAL;
    const double forwardY = -(double)FixedByFrac(GLOBAL1, viewsin) / TILEGLOBAL;

    separation = Cat3DS_LimitCornerOffset(separation, tilemap[0], MAPSIZE, mapwidth, mapheight,
        (double)centerX / TILEGLOBAL, (double)centerY / TILEGLOBAL, forwardX, forwardY,
        VIEWWIDTH, focalPixels);
    if (separation) {
        memcpy(centerVisible, spotvis, sizeof spotvis);
        centerActorsOnly = true;
        DrawScaleds();
        centerActorsOnly = false;
    } else {
        Cat3DS_DualScreenForgetStereo();
    }
    privateEyeActors = separation != 0;
    cat3dsStereoOcclusionEnabled = 1;
    for (int eye = separation ? -1 : 0; eye <= 1; eye += 2) {
        viewx = centerX + eye * FixedByFrac(separation, viewsin);
        viewy = centerY + eye * FixedByFrac(separation, viewcos);
        eyeLensShift = Cat3DS_LensShift((double)(eye * separation) / TILEGLOBAL, focalPixels);
        if (separation) memset(spotvis, 0, sizeof spotvis);
        memset(wallwidth, 0, sizeof wallwidth);
        memset(zbuffer, 0, sizeof zbuffer);
        EGAWRITEMODE(2);
        EGAMAPMASK(15);
        ClearScreen();
        for (int x = 0; x < VIEWWIDTH; ++x) {
            const double side = (x - (CENTERX + 0.5 + eyeLensShift)) / focalPixels;
            const Cat3DSWallHit hit = Cat3DS_TraceColumn(tilemap[0], separation ? spotvis[0] : NULL, MAPSIZE,
                mapwidth, mapheight, (double)viewx / TILEGLOBAL, (double)viewy / TILEGLOBAL,
                forwardX - forwardY * side, forwardY + forwardX * side, (double)FOCALLENGTH / TILEGLOBAL);
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
            const int height = (int)fmin(MAXSCALE, fmax(1, TILEGLOBAL / (2.0 * hit.depth * scale)));
            zbuffer[x] = wallheight[x] = height;
            wallwidth[x] = 1;
            wallofs[x] = hit.column * 64;
            wallseg[x] = (id0_byte_t *)walldirectory[index];
        }
        ScaleWalls();
        DrawScaleds();
        if (eye < 0) {
            EGAWRITEMODE(0);
            EGABITMASK(0xff);
            if (handheight) DrawHand();
            Cat3DS_DualScreenCaptureLeftEGA(bufferofs);
        }
    }
    privateEyeActors = false;
    cat3dsStereoOcclusionEnabled = 0;
    viewx = centerX;
    viewy = centerY;
    if (separation) memcpy(spotvis, centerVisible, sizeof spotvis);
}
