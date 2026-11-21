// Native selector and ReflectionHLE installation check for the direct game entry.
#pragma once
#include <string.h>

#define CAT3DS_APOC_GAMEVER "catapoc101:game"
#define CAT3DS_APOC_DATA_PATH "sdmc:/3ds/catacomb-3ds/saves-apoc"
#define CAT3DS_APOC_GAME_PATH "sdmc:/3ds/catacomb-3ds/games/catapoc"

static inline bool cat3dsApocalypseGameplayFile(const char *manifestName)
{
    return !strcmp(manifestName, "APOCGAME.EXE") || !strcmp(manifestName, "AUDIO.APC") ||
           !strcmp(manifestName, "EGAGRAPH.APC") || !strcmp(manifestName, "GAMEMAPS.APC");
}
