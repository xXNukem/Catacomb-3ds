// SPDX-License-Identifier: GPL-2.0-or-later
// Included inside each linked engine's namespace, next to its DOS readers.
extern bool Cat3DS_MapDimensions(int level, unsigned &width, unsigned &height);
extern statetype s_player;
#if defined(REFKEEN_VER_CATABYSS) || defined(REFKEEN_VER_CATARM)
extern statetype s_wallskel;
#elif defined(REFKEEN_VER_CATAPOC)
extern statetype s_invis_wall_control;
#endif

bool Cat3DS_ReadSavedGame(FILE *file, bool apply, int &level)
{
    gametype state = {};
#if defined(REFKEEN_VER_CATARM) || defined(REFKEEN_VER_CATAPOC)
    id0_word_t sky, ground;
    if (BE_Cross_readInt16LE(file, &sky) != 2 || !GetSkyGndColorPtrFromDOSPointer(sky) ||
        BE_Cross_readInt16LE(file, &ground) != 2 || !GetSkyGndColorPtrFromDOSPointer(ground))
        return false;
#endif
#ifdef REFKEEN_VER_CATADVENTURES
    id0_short_t freeze;
    id0_boolean_t easy;
    if (BE_Cross_readInt16LE(file, &freeze) != 2) return false;
#endif
    if (!LoadGameState(file, &state)) return false;
#ifdef REFKEEN_VER_CATADVENTURES
    if (BE_Cross_read_boolean_From16LE(file, &easy) != 2) return false;
#endif
    unsigned width, height;
    if (state.mapon < 0 || state.mapon >= NUMLEVELS ||
        !Cat3DS_MapDimensions(state.mapon, width, height) ||
        !width || width > MAPSIZE || !height || height > MAPSIZE ||
        state.difficulty < gd_Continue || state.difficulty > gd_Hard ||
        state.body < 0 || state.body > MAXBODY ||
        state.shotpower < 0 || state.shotpower > MAXSHOTPOWER)
        return false;

    if (state.bolts < 0 || state.nukes < 0 || state.potions < 0) return false;
    for (auto keyCount : state.keys)
        if (keyCount < 0) return false;
    for (auto scroll : state.scrolls)
        if (scroll != 0 && scroll != 1) return false;

    // Only the game thread accesses slots. Keep the bounded staging area off
    // the 3DS stack. Nothing below reads from the SD after committing state.
    static id0_word_t planes[2][MAPSIZE * MAPSIZE];
    static objtype actors[MAXACTORS];
    for (auto &plane : planes)
        if (!Cat3DS_ReadSavedPlane(file, plane, width * height)) return false;

    int count = 0;
    do {
        if (count == MAXACTORS) return false;
        objtype &actor = actors[count++];
        if (!LoadObject(file, &actor) || !actor.state ||
            actor.tilex >= width || actor.tiley >= height ||
            actor.x < 0 || actor.y < 0 ||
            unsigned(actor.x >> TILESHIFT) >= width ||
            unsigned(actor.y >> TILESHIFT) >= height ||
            actor.angle < 0 || actor.angle > ANGLES ||
            actor.xl > actor.xh || actor.yl > actor.yh ||
            actor.xl < 0 || actor.yl < 0 ||
            actor.xh > (long(width) << TILESHIFT) ||
            actor.yh > (long(height) << TILESHIFT) ||
            (actor.obclass == playerobj) != (count == 1) ||
            (actor.state == &s_player) != (count == 1))
            return false;
#if defined(REFKEEN_VER_CATABYSS) || defined(REFKEEN_VER_CATARM)
        // T_WallSkeleton uses the DOS hitpoints and speed fields as tile indices.
        if (actor.state == &s_wallskel &&
            (actor.hitpoints < 0 || unsigned(actor.hitpoints) >= width ||
             actor.speed < 0 || unsigned(actor.speed) >= height))
            return false;
#elif defined(REFKEEN_VER_CATAPOC)
        // T_InvisWall restores this field into the renderer’s wall tile map.
        if (actor.state == &s_invis_wall_control &&
            (actor.temp1 < 0 || actor.temp1 >= NUMFLOORS))
            return false;
#endif
    } while (actors[count - 1].next);
    if (fgetc(file) != EOF || ferror(file)) return false;

    level = state.mapon;
    if (!apply) return true;
#ifdef REFKEEN_VER_CATADVENTURES
    screenpage = 0;
    FreeUpMemory();
    FreezeTime = freeze;
    EASYMODEON = easy;
    playstate = ex_loadedgame;
#endif
#if defined(REFKEEN_VER_CATARM) || defined(REFKEEN_VER_CATAPOC)
    skycolor = GetSkyGndColorPtrFromDOSPointer(sky);
    groundcolor = GetSkyGndColorPtrFromDOSPointer(ground);
#endif
    gamestate = state;
    SetupGameLevel();
    memcpy(mapsegs[0], planes[0], width * height * sizeof(id0_word_t));
    memcpy(mapsegs[2], planes[1], width * height * sizeof(id0_word_t));
    memset(tilemap, 0, sizeof(tilemap));
    memset(actorat, 0, sizeof(actorat));
    for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width; ++x) {
            unsigned tile = planes[0][y * width + x];
            if (tile < NUMFLOORS) {
#if defined(REFKEEN_VER_CATARM) || defined(REFKEEN_VER_CATAPOC)
                if (tile != INVISIBLEWALL)
#endif
                    tilemap[x][y] = tile;
                actorat[x][y] = tile;
            }
        }
    InitObjList();
    for (int i = 0; i < count; ++i) {
        if (i) GetNewObj(false);
        objtype *prev = newobj->prev;
        *newobj = actors[i];
        newobj->prev = prev;
        newobj->next = NULL;
        actorat[newobj->tilex][newobj->tiley] = COMPAT_OBJ_CONVERT_OBJ_PTR_TO_DOS_PTR(newobj);
    }
    return true;
}
