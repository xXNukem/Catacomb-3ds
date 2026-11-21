// Compiled in each original engine namespace so the active game owns its state.
// Original help and slot dialogs run their own event loops. Exclude the
// entire modal interval from the next simulation step.
class Cat3DS_DialogClock {
    const id0_longword_t before = SD_GetTimeCount();
public:
    ~Cat3DS_DialogClock() {
        Cat3DS_WaitForPadRelease();
        IN_ClearKeysDown();
        Paused = false;
        SD_SetTimeCount(before);
        lasttimecount = before;
    }
};

bool Cat3DS_HandleMenu(bool title, int &returnRow)
{
#ifdef REFKEEN_VER_CAT3D
    const int game = 0;
    const bool dead = false;
    const bool settings = false;
#else
#ifdef REFKEEN_VER_CATABYSS
    const int game = 1;
#elif defined(REFKEEN_VER_CATARM)
    const int game = 2;
#else
    const int game = 3;
#endif
    const bool dead = Flags & FL_DEAD;
    const bool settings = Keyboard[sc_F2];
#endif
    const int previousRow = returnRow;
    returnRow = -1;
    if (previousRow < 0 && !title && !dead && !Paused && !settings && !Keyboard[sc_Escape] && LastScan != sc_Escape)
        return false;
    const auto clock = SD_GetTimeCount();
    Cat3DSMenu menu = {dead ? CatRoot : Paused ? CatPause : settings ? CatSettings : CatRoot,
                       previousRow >= 0 ? previousRow : dead ? CatLoad : CatResume, 0, 0, game, SoundMode, MusicMode,
                       !!AdLibPresent, dead, title};
    static BE_ST_ControllerMapping nativeModalMapping = {};
    BE_ST_AltControlScheme_Push();
    BE_ST_AltControlScheme_PrepareControllerMapping(&nativeModalMapping);
#ifdef REFKEEN_VER_CAT3D
    const bool pauseMusic = !title && MusicMode == smm_AdLib;
    if (pauseMusic) SD_MusicOff();
#endif
    const int action = Cat3DS_OpenMenu(menu);
#ifdef REFKEEN_VER_CAT3D
    if (pauseMusic) SD_MusicOn();
#endif
    BE_ST_AltControlScheme_Pop();
    Paused = false;
    IN_ClearKeysDown();
    if (MousePresent) BE_ST_GetEmuAccuMouseMotion(NULL, NULL);
    SD_SetTimeCount(clock);
    lasttimecount = clock;
    if (menu.sound != SoundMode) {
        SD_SetSoundMode((SDMode)menu.sound);
        CA_LoadAllSounds();
    }
    if (action == CatSwitch || action == CatExit) {
#ifdef REFKEEN_VER_CAT3D
        if (!title) abortgame = true;
#endif
        cat3dsSessionAction = action;
        playstate = ex_abort;
        return true;
    }
#ifdef REFKEEN_VER_CAT3D
    if (menu.music != MusicMode) {
        StopMusic();
        SD_SetMusicMode((SMMode)menu.music);
        if (!title && MusicMode == smm_AdLib) StartMusic();
    }
    if (action == CatOriginalOptions) {
        returnRow = CatOptions;
        LastScan = sc_Escape;
        return false;
    }
    if (action == CatSave || action == CatLoad) {
        extern void Cat3DS_GameSlots(bool);
        Cat3DS_DialogClock modalClock;
        if (!title) StopMusic();
        NormalScreen();
        if (!title) FreeUpMemory();
        Cat3DS_GameSlots(action == CatSave);
        if (loadedgame) playstate = ex_loadedgame;
        else returnRow = action;
        if (!title) {
            StartMusic();
            DrawPlayScreen();
            CacheScaleds();
        }
        lasttimecount = SD_GetTimeCount();
    }
    if (action == CatNew) { restartgame = gd_Normal; NewGame(); playstate = ex_resetgame; }
#else
    if (action == CatNew) { Flags &= ~FL_DEAD; playstate = ex_resetgame; }
    if (action == CatHelp || action == CatSave || action == CatLoad) {
        returnRow = action;
        Keyboard[action == CatHelp ? sc_F1 : action == CatSave ? sc_F3 : sc_F4] = true;
        return false;
    }
#endif
    return true;
}
