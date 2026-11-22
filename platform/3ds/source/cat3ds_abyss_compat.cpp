/* Catacomb 3DS: structural edition adapter, Abyss first.
 * The selected DOS files are NEVER patched. We derive a bounded install
 * descriptor after validating the LZEXE image and embedded EGA/audio/map
 * tables.
 */
#include "cat3ds_resource_probe.h"
#include "cat3ds_game_files.h"
#include "backend/gamedefs/be_gamedefs_structs.h"
#include <dirent.h>
#include <ctype.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace {
BE_GameVerDetails_T variant;
BE_GameFileDetails_T required[32];
BE_EXEFileDetails_T exes[6];
BE_EmbeddedGameFileDetails_T embedded[6];
char names[32][128];
unsigned numRequired;
char gameExe[128];
char variantProfile[64];

bool equalNoCase(const char *a, const char *b)
{
    while (*a && *b)
    {
        if (toupper((unsigned char)*a++) != toupper((unsigned char)*b++)) return false;
    }
    return !*a && !*b;
}
bool findFilename(const char *name, char *out)
{
    DIR *dir = opendir(".");
    if (!dir) return false;
    out[0] = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)))
        if (strlen(entry->d_name) < 128 && equalNoCase(name, entry->d_name))
        {
            snprintf(out, 128, "%s", entry->d_name);
            break;
        }
    closedir(dir);
    return out[0] != 0;
}
uint32_t crcFile(FILE *fp, bool &ok)
{
    unsigned char buf[4096];
    uint32_t crc = 0xffffffffu;
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)))
        for (size_t i = 0; i < n; ++i)
        {
            crc ^= buf[i];
            for (int bit = 0; bit < 8; ++bit)
                crc = (crc >> 1) ^ ((crc & 1u) ? 0xedb88320u : 0);
        }
    ok = !ferror(fp);
    return ~crc;
}
bool addFile(const char *filename, int minSize, int maxSize)
{
    if (numRequired >= 31) return false;
    char actual[128];
    if (!findFilename(filename, actual)) return false;
    FILE *fp = fopen(actual, "rb");
    if (!fp) return false;
    const long size = fseek(fp, 0, SEEK_END) == 0 ? ftell(fp) : -1;
    if (size < minSize || size > maxSize || fseek(fp, 0, SEEK_SET))
    { fclose(fp); return false; }
    bool ok;
    const uint32_t crc = crcFile(fp, ok);
    fclose(fp);
    if (!ok) return false;
    snprintf(names[numRequired], sizeof(names[numRequired]), "%s", actual);
    required[numRequired] = {names[numRequired], int(size), crc};
    ++numRequired;
    return true;
}
}

/* Must be called AFTER chdir to the selected game's data folder and BEFORE
 * BE_Cross_PrepareGameInstallations(), so the backend validates this descriptor
 * again when it opens files. Returns 0 on unsupported data, 113 or 124 on
 * a structurally compatible install. Does not start an EXE or claim the DOS
 * program's code is byte-identical with supported ReflectionHLE revisions.
 */
extern "C" int cat3dsPrepareAbyssCompatibility(void)
{
    Cat3DSResourceProbe probe = {};
    cat3dsProbeResources(1, ".", &probe);
    if (!probe.graphicsValid || !probe.audioAndMapsPresent ||
        !probe.embeddedLayoutValid || probe.imageSize < 140000 ||
        probe.imageSize > 300000 || !probe.exeName[0])
    {
        std::fprintf(stderr, "Abyss: unsupported embedded layout: %s\n", probe.problem);
        return 0;
    }
    const int edition = cat3dsAbyssEdition(probe);
    if (!edition || strlen(probe.exeName) > 12) return 0;
    const bool registered = edition == 124;
    const int id = registered ? BE_GAMEVER_CATABYSS124 : BE_GAMEVER_CATABYSS113;
    const BE_GameVerDetails_T *base = g_be_gamever_ptrs[id];
    if (!base || !base->exeFiles) return 0;

    // Build a descriptor from files the gameplay engine actually opens.
    // SHP01..SHP12 are loaded by the separate original intro program,
    // which the 3DS selector does not launch. Do not block gameplay on them.
    numRequired = 0;
    if (!addFile(probe.exeName, 45000, 600000) ||
        !addFile("AUDIO.ABS", 128, 2000000) ||
        !addFile("EGAGRAPH.ABS", 32000, 2000000) ||
        !addFile("GAMEMAPS.ABS", 1024, 2000000)) return 0;
    // Optional original game resources are whitelisted, not all .EXEs,
    // and not files created by users or the game while running.
    const char *extras[] = {"LAST.ABS", "ARMAPC.ABS", "HELP.TXT"};
    for (const char *extra : extras)
    {
        char actual[128];
        if (findFilename(extra, actual) && !addFile(extra, 1, 300000)) return 0;
    }
    required[numRequired] = {};

    unsigned exeCount = 0, gameIndex = 0;
    for (const BE_EXEFileDetails_T *exe = base->exeFiles; exe->mainFuncPtr; ++exe)
    {
        if (exeCount >= 5) return 0;
        exes[exeCount] = *exe;
        if (exe->subGameVerStr && !strcmp(exe->subGameVerStr, "game"))
            gameIndex = exeCount;
        ++exeCount;
    }
    if (!exeCount || !exes[gameIndex].embeddedFiles ||
        !exes[gameIndex].embeddedFilesLoaderFuncPtr) return 0;
    exes[exeCount] = {};
    snprintf(gameExe, sizeof(gameExe), "%s", probe.exeName);
    exes[gameIndex].exeNames = gameExe;
    exes[gameIndex].decompExeImageSize = int(probe.imageSize);
    exes[gameIndex].decompExeExtraMem = 0;
    if (probe.plainMZ) exes[gameIndex].compressionType = BE_EXECOMPRESSION_NONE;

    const BE_EmbeddedGameFileDetails_T *reference = exes[gameIndex].embeddedFiles;
    const unsigned offsets[] = {probe.audioDictOffset, probe.audioHeadOffset,
             probe.dictOffset, probe.headOffset, probe.mapHeadOffset};
    const unsigned crcs[] = {probe.audioDictCrc, probe.audioHeadCrc,
             probe.egaDictCrc, probe.egaHeadCrc, probe.mapHeadCrc};
    for (int i = 0; i < 5; ++i)
    {
        if (!reference[i].fileDetails.filenames ||
            offsets[i] + unsigned(reference[i].fileDetails.filesize) > probe.imageSize)
            return 0;
        embedded[i] = reference[i];
        embedded[i].offset = int(offsets[i]);
        embedded[i].fileDetails.crc32 = crcs[i];
    }
    embedded[5] = {};
    exes[gameIndex].embeddedFiles = embedded;
    variant = *base;
    variant.reqFiles = required;
    variant.exeFiles = exes;
    variant.description = "Catacomb Abyss - structurally compatible DOS data";
    g_be_gamever_ptrs[id] = &variant;
    std::fprintf(stderr, "Abyss: compatible resource tables; executable %s; image %u bytes\n",
                 probe.exeName, probe.imageSize);
    return registered ? 124 : 113;
}

// Launch only the gameplay EXE. The backend rechecks the required archives
// and verifies the embedded table CRCs when loading the selected executable.
extern "C" bool cat3dsPrepareVerifiedGame(int game, int id, const Cat3DSInstallStatus &install)
{
    const auto *base = g_be_gamever_ptrs[id];
    const auto *exe = base->exeFiles;
    if (game) {
        while (exe->mainFuncPtr && (!exe->subGameVerStr || strcmp(exe->subGameVerStr, "game"))) ++exe;
    }
    if (!exe->mainFuncPtr) return false;
    const char *actual = install.compatibleGameTables ? install.resourceExeName : install.matchedExeName;
    if (!*actual || strlen(actual) > 12) return false;
    snprintf(gameExe, sizeof(gameExe), "%s", actual);
    exes[0] = *exe;
    exes[0].exeNames = gameExe;
    exes[1] = {};
    Cat3DSResourceProbe probe = {};
    const Cat3DSEarlierRelease *release = nullptr;
    if (install.compatibleGameTables) {
        cat3dsProbeResources(game, ".", &probe);
        release = cat3dsEarlierRelease(game, probe);
        if (!probe.embeddedLayoutValid || strcmp(probe.exeName, actual) ||
            (!release && probe.imageSize != unsigned(exe->decompExeImageSize))) return false;
        exes[0].decompExeImageSize = int(probe.imageSize);
    }
    unsigned count = 0;
    uint32_t wrapperCrc = 0;
    for (auto f = base->reqFiles; f->filenames; ++f) {
        if (!strcmp(f->filenames, exe->exeNames)) {
            required[count] = *f;
            required[count].filenames = gameExe;
            if (install.compatibleGameTables) {
                FILE *fp = fopen(actual, "rb");
                if (!fp) return false;
                bool ok;
                required[count].crc32 = crcFile(fp, ok);
                required[count].filesize = ftell(fp);
                wrapperCrc = required[count].crc32;
                fclose(fp);
                if (!ok) return false;
            }
        } else if (!strncmp(f->filenames, "AUDIO.", 6) || !strncmp(f->filenames, "EGAGRAPH.", 9) ||
                   !strncmp(f->filenames, "GAMEMAPS.", 9)) required[count] = *f;
        else continue;
        if (release && !strncmp(f->filenames, "EGAGRAPH.", 9)) {
            required[count].filesize = release->graphicsSize;
            required[count].crc32 = release->graphicsCrc;
        }
        ++count;
    }
    if (count != 4) return false;
    required[count] = {};
    if (install.compatibleGameTables) {
        const unsigned offsets[] = {probe.audioDictOffset, probe.audioHeadOffset, probe.dictOffset,
                                    probe.headOffset, probe.mapHeadOffset};
        const unsigned crcs[] = {probe.audioDictCrc, probe.audioHeadCrc, probe.egaDictCrc,
                                 probe.egaHeadCrc, probe.mapHeadCrc};
        for (unsigned i = 0; i < 5; ++i) {
            embedded[i] = exe->embeddedFiles[i];
            if ((!release && crcs[i] != embedded[i].fileDetails.crc32) ||
                offsets[i] + unsigned(embedded[i].fileDetails.filesize) > probe.imageSize) return false;
            embedded[i].fileDetails.crc32 = crcs[i];
            embedded[i].offset = offsets[i];
        }
        embedded[5] = {};
        exes[0].embeddedFiles = embedded;
        if (probe.plainMZ) exes[0].compressionType = BE_EXECOMPRESSION_NONE;
    }
    variant = *base;
    variant.reqFiles = required;
    variant.exeFiles = exes;
    if (install.compatibleGameTables) {
        snprintf(variantProfile, sizeof variantProfile, "%s-%08x", base->writableFilesDir, (unsigned)wrapperCrc);
        variant.writableFilesDir = variantProfile;
    }
    g_be_gamever_ptrs[id] = &variant;
    return true;
}
