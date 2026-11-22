// SPDX-License-Identifier: GPL-2.0-or-later
// Compiled in the game's namespace to use its DOS save hooks.
extern bool Cat3DS_ReadSavedGame(FILE *, bool apply, int &level);

#include "cat3ds_slot_header.inc"

extern "C" void Cat3DS_DualScreenEnterMenu(void);
extern "C" void Cat3DS_DualScreenLeaveMenu(void);
extern "C" void Cat3DS_DrawSlotPage(const char *, const char *const *, const char *const *, int, const char *);

// Read physical Nintendo buttons directly only inside this modal. Require a
// fresh press, and consume its release before returning to the engine menu.
static u32 Cat3DS_SlotKey()
{
    const u32 mask = KEY_A | KEY_B | KEY_START | KEY_DUP | KEY_DDOWN;
    bool released = false;
    u32 choice = 0;
    while (aptMainLoop()) {
        BE_ST_ShortSleep();
        // BE_ST_ShortSleep has already pumped SDL and scanned HID.
        const u32 held = hidKeysHeld() & mask;
        if (!held) {
            if (choice) break;
            released = true;
        } else if (released && !choice) {
            choice = (held & KEY_START) ? KEY_B : held;
        }
    }
    IN_ClearKeysDown();
    return choice ? choice : KEY_B;
}

static bool Cat3DS_SlotMessage(const char *text, bool confirm)
{
    Cat3DS_DrawSlotPage(text,nullptr,nullptr,-1,confirm ? "A Replace     B Cancel" : "A / B Back");
    for (;;) {
        u32 key = Cat3DS_SlotKey();
        if (key & KEY_B) return false;
        if (key & KEY_A) return true;
    }
}

enum Cat3DS_SlotStatus { SlotEmpty, SlotReady, SlotInvalid };

static Cat3DS_SlotStatus Cat3DS_InspectSlot(const char *profile, int slot, Cat3DS_SlotLabel &label)
{
    Cat3DS_SlotFile data(profile, slot, slotExtension);
    FILE *f = data.open();
    if (!f) return errno == ENOENT ? SlotEmpty : SlotInvalid;
    int level;
    bool valid = Cat3DS_SlotHeader(f, false, label) && Cat3DS_ReadSavedGame(f, false, level);
#if defined(REFKEEN_VER_CATABYSS) || defined(REFKEEN_VER_CATARM) || defined(REFKEEN_VER_CATAPOC)
    if (valid) {
        snprintf(label.title, sizeof(label.title), "Level %02d", level + 1);
        struct stat st;
        Cat3DS_Date(fstat(fileno(f), &st) == 0 ? st.st_mtime : (time_t)-1, label.date);
#ifdef REFKEEN_VER_CATABYSS
        Cat3DS_ReadSlotInfo(profile, slot, f, label.title, label.date);
#endif
    }
#endif
    fclose(f);
    return valid ? SlotReady : SlotInvalid;
}

static bool Cat3DS_RunSlots(bool saving)
{
    const char *profile = g_be_selectedGameInstallation->writableFilesPath;
    Cat3DS_SlotLabel labels[6] = {};
    Cat3DS_SlotStatus status[6];
    for (int i = 0; i < 6; ++i)
        status[i] = Cat3DS_InspectSlot(profile, i, labels[i]);
    struct MenuDisplay {
        MenuDisplay() { Cat3DS_DualScreenEnterMenu(); }
        ~MenuDisplay() { Cat3DS_DualScreenLeaveMenu(); }
    } display;
    int slot = 0;
    for (;;) {
        const char *titles[6], *dates[6];
        for (int i=0;i<6;++i) {
            titles[i] = status[i]==SlotReady ? labels[i].title :
                        status[i]==SlotEmpty ? "Empty" : "Unreadable game";
            dates[i] = status[i]==SlotReady ? labels[i].date : "";
        }
        Cat3DS_DrawSlotPage(saving ? "Save game" : "Load game",titles,dates,slot,"Up/Down   A Select   B Back");
        u32 key = Cat3DS_SlotKey();
        if (key & KEY_B) return false;
        if (key & KEY_DUP) slot = (slot + 5) % 6;
        else if (key & KEY_DDOWN) slot = (slot + 1) % 6;
        else if (key & KEY_A) {
            Cat3DS_SlotFile data(profile, slot, slotExtension);
            Cat3DS_SlotLabel label = {};
            if (saving) {
                if (status[slot] != SlotEmpty && !Cat3DS_SlotMessage("Replace this saved game?", true))
                    continue;
                Cat3DS_Date(time(NULL), label.date);
                int level;
                bool saved = data.replace([&](FILE *f) {
                    if (!Cat3DS_SlotHeader(f, true, label) || !USL_SaveGame || !USL_SaveGame(f) ||
                        fflush(f) || fseek(f, 0, SEEK_SET)) return false;
                    return Cat3DS_SlotHeader(f, false, label) && Cat3DS_ReadSavedGame(f, false, level);
                });
                if (!saved) Cat3DS_SlotMessage("Could not save this game", false);
#ifdef REFKEEN_VER_CATABYSS
                else if (!Cat3DS_WriteSlotInfo(profile, slot, data, Cat3DS_AbyssLevelName(), label.date))
                    Cat3DS_SlotMessage("Game saved; label write failed", false);
#endif
                return saved;
            }
            if (status[slot] == SlotEmpty) {
                Cat3DS_SlotMessage("This slot is empty", false);
                continue;
            }
            FILE *f = data.open();
            bool loaded = f && Cat3DS_SlotHeader(f, false, label) && USL_LoadGame && USL_LoadGame(f);
            if (f) fclose(f);
            if (loaded) return true;
            Cat3DS_SlotMessage("Cannot read this saved game", false);
        }
    }
}
