// SPDX-License-Identifier: GPL-2.0-or-later
// Built once per engine by run_3ds_slots_host.sh. Use actual DOS codecs,
// with a synthetic map archive and a minimal live actor list.
#include <cassert>
#include <cstdlib>
#include <string>
#include <vector>
#include "cat3ds_slot_info.h"
#ifdef REFKEEN_VER_CATABYSS
#include "def.h"
#include "gelib.h"
#else
#include "c3_def.h"
#endif

REFKEEN_NS_B
#ifdef REFKEEN_VER_CATABYSS
const char *refkeen_compat_gelib_gamename;
size_t refkeen_compat_gelib_gamename_strbufflen;
#else
id0_word_t refkeen_compat_id_us_printx_offset = 0xa50c;
#endif
#include "cat3ds_slot_header.inc"
extern BE_FILE_T maphandle;
extern bool Cat3DS_ReadSavedGame(FILE *, bool, int &);
statetype s_player, s_wallskel;
static statetype monster;
static statetype *lookup(uint_fast32_t n)
{
    if (n == 0x1234) return &s_player;
    return n == 0x4321 ? &monster : NULL;
}
statetype *(*RefKeen_GetObjStatePtrFromDOSPointer)(uint_fast32_t) = lookup;
id0_unsigned_t mapwidth = 4, mapheight = 4;
id0_byte_t tilemap[MAPSIZE][MAPSIZE];
id0_unsigned_t actorat[MAPSIZE][MAPSIZE];
objtype objlist[MAXACTORS], *newobj, *player;
#ifdef REFKEEN_VER_CATABYSS
id0_word_t refkeen_compat_c4_play_objoffset = 0x2000;
id0_short_t FreezeTime;
id0_int_t screenpage;
void FreeUpMemory() {}
#else
id0_word_t refkeen_compat_c3_play_objoffset = 0x2000;
#endif
static int setups;
void SetupGameLevel() { ++setups; }
void InitObjList() { player = newobj = objlist; newobj->prev = NULL; }
void GetNewObj(id0_boolean_t)
{
    objtype *prev = newobj++;
    prev->next = newobj;
    newobj->prev = prev;
}
static std::vector<unsigned char> contents(FILE *f)
{
    assert(f && !fseek(f, 0, SEEK_END));
    long size = ftell(f);
    assert(size >= 0);
    rewind(f);
    std::vector<unsigned char> data(size);
    assert(fread(data.data(), 1, size, f) == size_t(size));
    fclose(f);
    return data;
}
static void run(const char *root)
{
    maptype header = {};
    header.width = header.height = 4;
    maphandle = tmpfile();
    assert(maphandle && !fseek(maphandle, 8, SEEK_SET));
    assert(fwrite(&header, sizeof(header), 1, maphandle) == 1);
    static id0_byte_t maphead[402];
    maphead[2] = 8;
    tinf = maphead;
    static id0_word_t walls[16], items[16];
    mapsegs[0] = walls;
    mapsegs[2] = items;
    for (int i = 0; i < 16; ++i)
        walls[i] = (i == 5 || i == 6 || i == 9 || i == 10) ? 0 : 1;
    InitObjList();
    player->state = &s_player;
    s_player.compatdospointer = 0x1234;
    player->obclass = playerobj;
    player->angle = 90;
    player->tilex = player->tiley = 1;
    player->x = player->y = TILEGLOBAL + TILEGLOBAL / 2;
    GetNewObj(false);
    monster.compatdospointer = 0x4321;
    newobj->obclass = orcobj;
    newobj->state = &monster;
    newobj->tilex = 2;
    newobj->tiley = 1;
    newobj->x = TILEGLOBAL*2 + TILEGLOBAL/2;
    newobj->y = TILEGLOBAL + TILEGLOBAL/2;
    gamestate.body = MAXBODY;
    gamestate.difficulty = gd_Normal;
    gamestate.bolts = 3;
    FILE *original = tmpfile();
    assert(original && SaveTheGame(original));
    auto bytes = contents(original);

    // Independently construct a legacy on-disk header, including DOS padding.
    Cat3DS_SlotLabel display = {};
    strcpy(display.date, "2026-01-02 03:04");
#ifdef REFKEEN_VER_CATABYSS
    for (const char *name : {"CATACOMB ABYSS  ", "CATACOMB ABYSS 3-D"}) {
        refkeen_compat_gelib_gamename = name;
        refkeen_compat_gelib_gamename_strbufflen = strlen(name)+1;
        std::vector<unsigned char> legacy(name, name + strlen(name)+1);
        legacy.insert(legacy.end(), SAVEVER_DATA, SAVEVER_DATA + sizeof(SAVEVER_DATA));
#else
    {
        std::vector<unsigned char> legacy = {'C','3','D',0,0x0c,0xa5,1,0};
        char name[33] = "Level 01 2026-01-02 03:04";
        legacy.insert(legacy.end(), name, name+sizeof(name));
        legacy.push_back(0);
#endif
        FILE *header = tmpfile();
        assert(header && Cat3DS_SlotHeader(header, true, display));
        assert(contents(header) == legacy);
        header = tmpfile();
        assert(header && fwrite(legacy.data(), 1, legacy.size(), header) == legacy.size());
        assert(fwrite(bytes.data(), 1, bytes.size(), header) == bytes.size());
        rewind(header);
        int map = -1;
        assert(Cat3DS_SlotHeader(header, false, display) && Cat3DS_ReadSavedGame(header, false, map));
        rewind(header);
        fputc('X', header);
        rewind(header);
        assert(!Cat3DS_SlotHeader(header, false, display));
        fclose(header);
    }
    Cat3DS_SlotFile dest(root, 0, slotExtension);
    assert(dest.replace([](FILE *f) { return SaveTheGame(f); }));
    assert(contents(dest.open()) == bytes);
    gamestate.bolts = 7;
    assert(dest.replace([](FILE *f) { return SaveTheGame(f); }));
    auto second = contents(dest.open());
    assert(second != bytes);
    assert(!dest.replace([](FILE *f) { fputc(9, f); return false; }));
    assert(contents(dest.open()) == second);
    assert(!dest.replace([](FILE *f) {
        // Simulate a lost descriptor after stdio has buffered the bytes.
        assert(fputs("partial", f) >= 0);
        assert(close(fileno(f)) == 0);
        return true;
    }));
    assert(contents(dest.open()) == second);
    assert(rename(dest.name, dest.backup) == 0);
    assert(contents(dest.open()) == second);
    assert(dest.replace([](FILE *f) { return SaveTheGame(f); }));

    std::string secondProfile = std::string(root) + "/other-edition";
    assert(mkdir(secondProfile.c_str(), 0700) == 0);
    Cat3DS_SlotFile other(secondProfile.c_str(), 0, slotExtension);
    assert(!other.open() && errno == ENOENT);
    assert(other.replace([&](FILE *f) { return fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size(); }));
    char title[33] = "fallback", date[20] = "";
    FILE *f = dest.open();
    Cat3DS_ReadSlotInfo(root, 0, f, title, date);
    assert(!strcmp(title, "fallback"));
    fclose(f);
    assert(Cat3DS_WriteSlotInfo(root, 0, dest, "Level One", "2026-01-02 03:04"));
    f = dest.open();
    Cat3DS_ReadSlotInfo(root, 0, f, title, date);
    assert(!strcmp(title, "Level One") && !strcmp(date, "2026-01-02 03:04"));
    fclose(f);
    f = other.open();
    strcpy(title, "fallback");
    Cat3DS_ReadSlotInfo(root, 0, f, title, date);
    assert(!strcmp(title, "fallback"));
    fclose(f);

    Cat3DS_SlotFile blocked(root, 1, "inf");
    assert(mkdir(blocked.name, 0700) == 0);
    assert(!Cat3DS_WriteSlotInfo(root, 1, dest, "Level Two", "2026-01-03 03:04"));
    assert(contents(dest.open()) == second);

    int level = -1;
    f = other.open();
    assert(Cat3DS_ReadSavedGame(f, false, level) && level == 0 && setups == 0);
    rewind(f);
    assert(LoadTheGame(f) && gamestate.bolts == 3 && setups == 1);
    assert(player->next == objlist+1 && player->next->prev == player && !player->next->next);
    fclose(f);
    auto reject = [&](const unsigned char *p, size_t n) {
        FILE *bad = tmpfile();
        assert(bad && fwrite(p, 1, n, bad) == n);
        rewind(bad);
        assert(!LoadTheGame(bad));
        assert(setups == 1 && gamestate.bolts == 3 && player->angle == 90 && walls[0] == 1);
        fclose(bad);
    };
    for (size_t n = 0; n < bytes.size(); ++n) reject(bytes.data(), n);
    auto corrupt = bytes;
#ifdef REFKEEN_VER_CATABYSS
    corrupt[4] = 250; // map number after FreezeTime and difficulty
#else
    corrupt[2] = 250;
#endif
    reject(corrupt.data(), corrupt.size());
    corrupt = bytes;
    corrupt[corrupt.size()-4] = 1; // claims a subsequent actor
    reject(corrupt.data(), corrupt.size());
    // Corrupt the first plane length, object type, state, and tile coordinate.
#ifdef REFKEEN_VER_CATABYSS
    const size_t stateBytes = 56, objectBytes = 66;
#else
    const size_t stateBytes = 42, objectBytes = 68;
#endif
    for (size_t pos : {stateBytes, bytes.size()-objectBytes+4,
                       bytes.size()-objectBytes+6, bytes.size()-objectBytes+26}) {
        corrupt = bytes;
        corrupt[pos] = corrupt[pos+1] = 0xff;
        reject(corrupt.data(), corrupt.size());
    }
    corrupt = bytes;
    corrupt.push_back(0xff);
    reject(corrupt.data(), corrupt.size());
    fclose(maphandle);
}
REFKEEN_NS_E
int main(int argc, char **argv)
{
    assert(argc == 2);
#ifdef REFKEEN_VER_CATABYSS
    catabyss::run(argv[1]);
#else
    cat3d::run(argv[1]);
#endif
}
