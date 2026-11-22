// Exercise the original embedded loader against a synthetic decompressed image.
// Use test CRCs only on a private copy of the upstream descriptors.
#include "backend/gamedefs/be_gamedefs_structs.h"
#include "be_gamestr.h"
#include "be_sound_device_flags.h"
#define CSTR_TO_TCSTR(x) _T(x)
#ifdef REFKEEN_VER_CATAPOC
#include "backend/gamedefs/be_gamedefs_catapoc.h"
#include "../platform/3ds/source/cat3ds_apocalypse.h"
#else
#include "backend/gamedefs/be_gamedefs_catarm.h"
#endif
#include "../platform/3ds/source/cat3ds_manifest.h"
#include "../platform/3ds/source/cat3ds_game_files.h"
#include "crc32/crc32.h"
#include <cassert>
#include <cstdlib>
#include <stdexcept>
#include <vector>

unsigned char *g_be_current_exeImage;
const BE_EXEFileDetails_T *g_be_current_exeFileDetails;
void BE_ST_ExitWithErrorMsg(const char *msg) { throw std::runtime_error(msg); }

REFKEEN_NS_B
extern uint8_t *audiodict, *audiohead, *EGAdict, *maphead;
extern int32_t *EGAhead;
REFKEEN_NS_E
#ifdef REFKEEN_VER_CATAPOC
namespace engine = catapoc;
static const auto &required = g_be_reqgameverfiles_catapoc101;
static const auto &embedded = g_be_embeddedgamefiles_catapoc101_apocgame;
static const auto &manifest = cat3ds_files_catapoc101;
#else
namespace engine = catarm;
static const auto &required = g_be_reqgameverfiles_catarm102;
static const auto &embedded = g_be_embeddedgamefiles_catarm102_armgame;
static const auto &manifest = cat3ds_files_catarm102;
#endif

void checkArmResources()
{
    const size_t count = sizeof(manifest) / sizeof(manifest[0]);
    for (size_t i = 0; i < count; ++i) {
        const auto &req = required[i];
        assert(!strcmp(req.filenames, manifest[i].name));
        assert(req.filesize == manifest[i].size && req.crc32 == manifest[i].crc32);
    }
    assert(!required[count].filenames);
    Cat3DSInstallStatus s = {};
    s.directoryFound = true;
    s.fileCount = count;
    s.validCount = count;
#ifdef REFKEEN_VER_CATAPOC
    s.validCount = 0;
    for (size_t i = 0; i < count; ++i)
        if (cat3dsApocalypseGameplayFile(required[i].filenames)) ++s.coreFileCount;
    assert(s.coreFileCount == 4);
    s.coreValidCount = s.validCount = 4;
    assert(cat3dsCanLaunchApocalypse(s));
    --s.coreValidCount;
    s.resourceLayoutValid = true;
    assert(!cat3dsCanLaunchApocalypse(s));
    assert(!strcmp(cat3dsGameFolder(3), CAT3DS_APOC_GAME_PATH));
    assert(!strcmp(CAT3DS_APOC_GAMEVER, "catapoc101:game"));
    assert(!strcmp(CAT3DS_APOC_DATA_PATH "/" BE_STR_GAMEVER_CATAPOC101,
                   "sdmc:/3ds/catacomb-3ds/saves-apoc/catapoc101"));
#else
    s.coreValidCount = s.coreFileCount = 4;
    assert(cat3dsCanLaunchArmageddon(s));
    --s.coreValidCount;
    s.resourceGraphicsValid = s.resourceAudioMapsPresent = s.resourceLayoutValid = true;
    assert(!cat3dsCanLaunchArmageddon(s));
    assert(!strcmp(cat3dsGameFolder(2), "sdmc:/3ds/catacomb-3ds/games/catarm"));
#endif

    BE_EmbeddedGameFileDetails_T tables[6];
    static_assert(sizeof(tables) == sizeof(embedded), "five tables");
    memcpy(tables, embedded, sizeof(tables));
    BE_EXEFileDetails_T exe = {};
    exe.embeddedFiles = tables;
#ifdef REFKEEN_VER_CATAPOC
    std::vector<unsigned char> image(200064 - 0x2200);
#else
    std::vector<unsigned char> image(198304 - 0x2000);
#endif
    for (int i = 0; i < 5; ++i) {
        assert(tables[i].offset >= 0 && tables[i].offset + tables[i].fileDetails.filesize <= image.size());
        tables[i].fileDetails.crc32 = Crc32_ComputeBuf(0, image.data() + tables[i].offset,
                                                       tables[i].fileDetails.filesize);
    }
    g_be_current_exeImage = image.data();
    g_be_current_exeFileDetails = &exe;
    engine::RefKeen_Load_Embedded_Resources_From_catacombs_exe();
    void *loaded[] = {engine::audiodict, engine::audiohead, engine::EGAdict,
                      engine::EGAhead, engine::maphead};
    for (int i = 0; i < 5; ++i) {
        assert(loaded[i] == image.data() + tables[i].offset);
        image[tables[i].offset] ^= 1;
        bool rejected = false;
        try { engine::RefKeen_Load_Embedded_Resources_From_catacombs_exe(); }
        catch (const std::runtime_error &) { rejected = true; }
        assert(rejected);
        image[tables[i].offset] ^= 1;
    }
    g_be_current_exeImage = nullptr;
    g_be_current_exeFileDetails = nullptr;
}
