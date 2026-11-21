/* Native game-file validation for Catacomb 3DS.
 * Original filename/size/CRC32 requirements originate in ReflectionHLE's
 * be_gamedefs_cat*.h files. This module does not redistribute game data.
 */
#include "cat3ds_game_files.h"
#include "cat3ds_apocalypse.h"
#include "cat3ds_manifest.h"
#include "cat3ds_resource_probe.h"

#include <dirent.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char *const kNames[CAT3DS_GAME_COUNT] = {
    "Catacomb 3-D", "Catacomb Abyss", "Catacomb Armageddon", "Catacomb Apocalypse"
};
static const char *const kFolders[CAT3DS_GAME_COUNT] = {
    "sdmc:/3ds/catacomb-3ds/games/cat3d",
    "sdmc:/3ds/catacomb-3ds/games/catabyss",
    "sdmc:/3ds/catacomb-3ds/games/catarm",
    CAT3DS_APOC_GAME_PATH
};

/* Safe and idempotent: mkdir is called on each fixed path only. Existing
 * directories and game files are deliberately left untouched. */
bool cat3dsEnsureGameFolders(const char **failedPath)
{
    static const char *const folders[] = {
        "sdmc:/3ds",
        "sdmc:/3ds/catacomb-3ds",
        "sdmc:/3ds/catacomb-3ds/games",
        "sdmc:/3ds/catacomb-3ds/games/cat3d",
        "sdmc:/3ds/catacomb-3ds/games/catabyss",
        "sdmc:/3ds/catacomb-3ds/games/catarm",
        CAT3DS_APOC_GAME_PATH
    };
    if (failedPath)
        *failedPath = NULL;
    for (unsigned i = 0; i < sizeof(folders) / sizeof(folders[0]); ++i)
    {
        if (mkdir(folders[i], 0777) == 0)
            continue;
        struct stat st;
        if (errno == EEXIST && stat(folders[i], &st) == 0 && S_ISDIR(st.st_mode))
            continue;
        if (failedPath)
            *failedPath = folders[i];
        return false;
    }
    return true;
}

bool cat3dsCanLaunchArmageddon(const Cat3DSInstallStatus &s)
{
    return s.directoryFound && s.fileCount == int(sizeof(cat3ds_files_catarm102) /
                                                  sizeof(cat3ds_files_catarm102[0])) &&
           s.validCount == s.fileCount;
}

bool cat3dsCanLaunchApocalypse(const Cat3DSInstallStatus &s)
{
    return s.directoryFound && s.coreFileCount == 4 && s.coreValidCount == 4;
}

const char *cat3dsGameName(int game)
{
    return (game >= 0 && game < CAT3DS_GAME_COUNT) ? kNames[game] : "Unknown";
}

const char *cat3dsGameFolder(int game)
{
    return (game >= 0 && game < CAT3DS_GAME_COUNT) ? kFolders[game] : "";
}

static uint32_t calculateCRC32(FILE *file, int *ok)
{
    static uint32_t table[256];
    static int initialized;
    if (!initialized)
    {
        for (unsigned i = 0; i < 256; ++i)
        {
            uint32_t crc = i;
            for (int bit = 0; bit < 8; ++bit)
                crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0);
            table[i] = crc;
        }
        initialized = 1;
    }
    uint32_t crc = 0xffffffffu;
    unsigned char data[4096];
    size_t length;
    while ((length = fread(data, 1, sizeof(data), file)) != 0)
        for (size_t i = 0; i < length; ++i)
            crc = (crc >> 8) ^ table[(crc ^ data[i]) & 0xffu];
    *ok = !ferror(file);
    return crc ^ 0xffffffffu;
}

/*
 * The upstream manifests list the entire original distribution, including
 * documents, hintbooks, and catalogs. Those must not be conflated with the
 * four main files needed to identify the game edition. This is NOT a claim
 * that the eventual engine will run with only four files: auxiliary SHP
 * resources may be required at runtime by later Catacomb games.
 */
static bool isPrincipalFile(int game, const char *versionName, const char *name)
{
    static const char *const extensions[CAT3DS_GAME_COUNT] = {
        ".C3D", ".ABS", ".ARM", ".APC"
    };
    char filename[32];
    const char *const ext = extensions[game];
    const char *const prefixes[] = { "AUDIO", "EGAGRAPH", "GAMEMAPS" };
    for (unsigned i = 0; i < 3; ++i)
    {
        snprintf(filename, sizeof(filename), "%s%s", prefixes[i], ext);
        if (strcmp(name, filename) == 0)
            return true;
    }
    switch (game)
    {
    case 0: return strcmp(name, "CAT3D.EXE") == 0;
    case 1: return strcmp(name, (versionName && strstr(versionName, "1.13")) ?
                           "CATABYSS.EXE" : "ABYSGAME.EXE") == 0;
    case 2: return strcmp(name, "ARMGAME.EXE") == 0;
    case 3: return cat3dsApocalypseGameplayFile(name);
    default: return false;
    }
}

/* Names on the SD may have different case or be renamed by a distributor.
 * We only allow a different executable name if size AND CRC32 match one of
 * ReflectionHLE's known game executables. A different intro/launcher EXE
 * must not be mistaken for the executable containing game resources.
 */
enum FileResult { FILE_MATCH, FILE_ABSENT, FILE_WRONG_SIZE, FILE_WRONG_CRC };

static bool sameNameIgnoreCase(const char *a, const char *b)
{
    while (*a && *b)
    {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return false;
        ++a;
        ++b;
    }
    return *a == *b;
}

static bool isExeFilename(const char *name)
{
    const char *extension = strrchr(name, '.');
    return extension && sameNameIgnoreCase(extension, ".exe");
}

static void copyFilename(char dest[128], const char *source)
{
    snprintf(dest, 128, "%s", source);
}

static FileResult verifyCandidate(int game, const char *name,
                                  const Cat3DSRequiredFile *expected)
{
    char path[256];
    const int length = snprintf(path, sizeof(path), "%s/%s", kFolders[game], name);
    if (length < 0 || (size_t)length >= sizeof(path))
        return FILE_ABSENT;
    FILE *file = fopen(path, "rb");
    if (!file)
        return FILE_ABSENT;
    long size = -1;
    if (fseek(file, 0, SEEK_END) == 0)
        size = ftell(file);
    if (size < 0 || (uint32_t)size != expected->size ||
        fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return FILE_WRONG_SIZE;
    }
    int ok = 0;
    const uint32_t crc = calculateCRC32(file, &ok);
    fclose(file);
    return (ok && crc == expected->crc32) ? FILE_MATCH : FILE_WRONG_CRC;
}

static FileResult locateExpectedFile(int game, const Cat3DSRequiredFile *expected,
                                     bool exe, char actualName[128],
                                     char foundExeName[128])
{
    actualName[0] = '\0';
    if (exe)
        foundExeName[0] = '\0';
    FileResult result = verifyCandidate(game, expected->name, expected);
    if (result == FILE_MATCH)
    {
        copyFilename(actualName, expected->name);
        return FILE_MATCH;
    }
    DIR *dir = opendir(kFolders[game]);
    if (!dir)
        return result;
    struct dirent *entry;
    while ((entry = readdir(dir)))
    {
        const char *name = entry->d_name;
        if (strlen(name) >= 128)
            continue;
        const bool sameName = sameNameIgnoreCase(name, expected->name);
        const bool candidateExe = exe && game < 2 && isExeFilename(name);
        if (candidateExe && !foundExeName[0])
            copyFilename(foundExeName, name);
        if (!sameName && !candidateExe)
            continue;
        // Already tried this exact path.
        if (!strcmp(name, expected->name))
            continue;
        FileResult alternative = verifyCandidate(game, name, expected);
        if (alternative == FILE_MATCH)
        {
            copyFilename(actualName, name);
            closedir(dir);
            return FILE_MATCH;
        }
        // A differently cased copy of the expected file is present but wrong.
        // Keep that diagnostic if the original expected path wasn't found.
        if (sameName && result == FILE_ABSENT)
            result = alternative;
    }
    closedir(dir);
    return result;
}

static Cat3DSInstallStatus scanVersion(int game, const Cat3DSGameVersion *version,
                                      int directoryFound)
{
    Cat3DSInstallStatus status = {};
    status.versionName = version->versionName;
    status.directoryFound = directoryFound;
    status.fileCount = (int)version->fileCount;

    for (unsigned i = 0; i < version->fileCount; ++i)
    {
        const Cat3DSRequiredFile *expected = &version->files[i];
        const bool core = isPrincipalFile(game, version->versionName, expected->name);
        const bool exe = core && isExeFilename(expected->name);
        if (core)
            ++status.coreFileCount;
        if (exe)
            status.expectedExeName = expected->name;
        char actualName[128];
        char candidateExe[128] = {};
        const FileResult result = locateExpectedFile(game, expected, exe,
                                                      actualName, candidateExe);
        if (exe)
        {
            copyFilename(status.foundExeName, candidateExe);
            if (result == FILE_MATCH)
                copyFilename(status.matchedExeName, actualName);
        }
        if (result != FILE_ABSENT)
        {
            ++status.presentCount;
            if (core)
                ++status.corePresentCount;
        }
        if (result == FILE_MATCH)
        {
            ++status.validCount;
            if (core)
                ++status.coreValidCount;
            continue;
        }
        const char *reason = (result == FILE_ABSENT) ?
                             (exe && candidateExe[0] ? "No matching EXE CRC/size" : "Missing") :
                             (result == FILE_WRONG_SIZE) ? "Wrong size" : "Wrong CRC32/read error";
        if (result == FILE_ABSENT)
        {
            ++status.missingCount;
            if (core)
                ++status.coreMissingCount;
        }
        else if (result == FILE_WRONG_SIZE)
        {
            ++status.wrongSizeCount;
            if (core)
                ++status.coreWrongSizeCount;
        }
        else
        {
            ++status.wrongCrcCount;
            if (core)
                ++status.coreWrongCrcCount;
        }
        if (core && !status.coreProblemFile)
        {
            status.coreProblemFile = expected->name;
            status.coreProblemReason = reason;
        }
        if (!status.firstProblemFile)
        {
            status.firstProblemFile = expected->name;
            status.firstProblemReason = reason;
        }
    }
    return status;
}

/* Diagnostic report: writes ONLY outside the original game directories.
 * Never modifies a game file. The report is refreshed after each scan and
 * contains the exact sizes/CRCs to identify variants not in the upstream list.
 */
static bool prefixIgnoreCase(const char *s, const char *prefix)
{
    while (*prefix)
    {
        if (!*s || tolower((unsigned char)*s) != tolower((unsigned char)*prefix))
            return false;
        ++s;
        ++prefix;
    }
    return true;
}

static bool hasAbyssShapes(const char *folder)
{
    // SHP01..SHP12 are accessed by gameplay, regardless of DOS edition.
    // They must be present and nonempty before making Abyss selectable.
    for (unsigned i = 1; i <= 12; ++i)
    {
        char expected[24];
        snprintf(expected, sizeof(expected), "SHP%02u.ABS", i);
        DIR *dir = opendir(folder);
        if (!dir) return false;
        char found[128] = {};
        struct dirent *entry;
        while ((entry = readdir(dir)))
            if (sameNameIgnoreCase(expected, entry->d_name) &&
                strlen(entry->d_name) < sizeof(found))
            {
                snprintf(found, sizeof(found), "%s", entry->d_name);
                break;
            }
        closedir(dir);
        if (!found[0]) return false;
        char path[320];
        const int n = snprintf(path, sizeof(path), "%s/%s", folder, found);
        if (n < 0 || (size_t)n >= sizeof(path)) return false;
        struct stat info;
        if (stat(path, &info) || !S_ISREG(info.st_mode) ||
            info.st_size < 32 || info.st_size > 250000) return false;
    }
    return true;
}

static bool isDiagnosticFile(const char *name)
{
    return isExeFilename(name) || prefixIgnoreCase(name, "EGAGRAPH.") ||
           prefixIgnoreCase(name, "AUDIO.") ||
           prefixIgnoreCase(name, "GAMEMAPS.");
}

static void writeInstallReport(const Cat3DSInstallStatus states[CAT3DS_GAME_COUNT])
{
    /* Only this generated text report is overwritten, never any game asset. */
    FILE *report = fopen("sdmc:/3ds/catacomb-3ds/install-report.txt", "wb");
    if (!report)
        return;
    fprintf(report, "CATACOMB 3DS - INSTALLATION DIAGNOSTICS\n");
    fprintf(report, "Read-only inspection of game folders. No game files modified.\n");
    fprintf(report, "CRC32 is hexadecimal, sizes are bytes.\n");
    fprintf(report, "An unknown CRC is not proof of a corrupt or pirated game.\n\n");

    for (int game = 0; game < CAT3DS_GAME_COUNT; ++game)
    {
        const Cat3DSInstallStatus *state = &states[game];
        fprintf(report, "=== %s ===\n", cat3dsGameName(game));
        fprintf(report, "Folder: %s\n", cat3dsGameFolder(game));
        fprintf(report, "Closest known edition: %s (main files %d/%d)\n",
                state->versionName ? state->versionName : "none",
                state->coreValidCount, state->coreFileCount);
        fprintf(report, "Found EXE (unverified): %s\n",
                state->foundExeName[0] ? state->foundExeName : "none");
        fprintf(report, "Matched game EXE: %s\n",
                state->matchedExeName[0] ? state->matchedExeName : "none");
        if (game > 0)
        {
            fprintf(report, "STRUCTURAL EGA CHECK: %s\n",
                    state->resourceGraphicsValid ? "PASS" : "NOT VERIFIED");
            fprintf(report, "EGA resource EXE: %s\n",
                    state->resourceExeName[0] ? state->resourceExeName : "none");
            if (state->resourceGraphicsValid)
                fprintf(report, "EXE image bytes: %u, EGAHEAD offset: %u, EGADICT offset: %u\n",
                        state->resourceImageSize, state->resourceHeadOffset,
                        state->resourceDictOffset);
            fprintf(report, "Embedded tables: %s\n",
                    state->resourceLayoutValid ? "STRUCTURALLY COMPATIBLE" : "NOT VALIDATED");
            if (game == 1)
                fprintf(report, "SHP01..SHP12: %s\n",
                        state->resourceShapesPresent ? "PRESENT" : "MISSING OR INVALID");
            fprintf(report, "EGA check detail: %s\n", state->resourceIssue);
        }
        fprintf(report, "ACTUAL PRIMARY FILES (filename | size | CRC32):\n");
        DIR *dir = opendir(kFolders[game]);
        if (!dir)
        {
            fprintf(report, "  [Folder unavailable]\n\n");
            continue;
        }
        struct dirent *entry;
        int listed = 0;
        while ((entry = readdir(dir)))
        {
            const char *name = entry->d_name;
            if (!isDiagnosticFile(name) || strlen(name) >= 128)
                continue;
            char path[320];
            const int length = snprintf(path, sizeof(path), "%s/%s", kFolders[game], name);
            if (length < 0 || (size_t)length >= sizeof(path))
                continue;
            FILE *file = fopen(path, "rb");
            if (!file)
            {
                fprintf(report, "  %s | cannot open\n", name);
                continue;
            }
            long size = -1;
            if (fseek(file, 0, SEEK_END) == 0)
                size = ftell(file);
            int ok = 0;
            uint32_t crc = 0;
            if (size >= 0 && fseek(file, 0, SEEK_SET) == 0)
                crc = calculateCRC32(file, &ok);
            fclose(file);
            if (!ok)
            {
                fprintf(report, "  %s | read error\n", name);
                continue;
            }
            fprintf(report, "  %-25s | %ld | %08lX", name, size,
                    (unsigned long)crc);
            int matches = 0;
            for (unsigned v = 0; v < cat3ds_gameVersionCount; ++v)
            {
                const Cat3DSGameVersion *version = &cat3ds_gameVersions[v];
                if (version->game != game)
                    continue;
                for (unsigned f = 0; f < version->fileCount; ++f)
                {
                    const Cat3DSRequiredFile *expected = &version->files[f];
                    if (!isPrincipalFile(game, version->versionName, expected->name))
                        continue;
                    if (size == (long)expected->size && crc == expected->crc32)
                    {
                        fprintf(report, " | MATCHES %s : %s",
                                version->versionName, expected->name);
                        ++matches;
                    }
                }
            }
            if (!matches)
                fprintf(report, " | no exact known-core match");
            fprintf(report, "\n");
            ++listed;
        }
        closedir(dir);
        if (!listed)
            fprintf(report, "  [No executable or main-data files found]\n");
        fprintf(report, "KNOWN CORE FILES FOR THIS GAME:\n");
        for (unsigned v = 0; v < cat3ds_gameVersionCount; ++v)
        {
            const Cat3DSGameVersion *version = &cat3ds_gameVersions[v];
            if (version->game != game)
                continue;
            fprintf(report, "  %s\n", version->versionName);
            for (unsigned f = 0; f < version->fileCount; ++f)
            {
                const Cat3DSRequiredFile *expected = &version->files[f];
                if (!isPrincipalFile(game, version->versionName, expected->name))
                    continue;
                fprintf(report, "    %-20s | %lu | %08lX\n",
                        expected->name, (unsigned long)expected->size,
                        (unsigned long)expected->crc32);
            }
        }
        fprintf(report, "\n");
    }
    fclose(report);
}

void cat3dsScanInstallations(Cat3DSInstallStatus out[CAT3DS_GAME_COUNT])
{
    for (int game = 0; game < CAT3DS_GAME_COUNT; ++game)
    {
        DIR *dir = opendir(kFolders[game]);
        const int found = dir != NULL;
        if (dir)
            closedir(dir);

        Cat3DSInstallStatus best = {};
        int haveVersion = 0;
        for (unsigned i = 0; i < cat3ds_gameVersionCount; ++i)
        {
            const Cat3DSGameVersion *version = &cat3ds_gameVersions[i];
            if (version->game != game)
                continue;
            Cat3DSInstallStatus candidate = scanVersion(game, version, found);
            // Prefer the edition with the most matching principal files;
            // then use other exact file matches as a tie breaker.
            if (!haveVersion || candidate.coreValidCount > best.coreValidCount ||
                (candidate.coreValidCount == best.coreValidCount &&
                 (candidate.validCount > best.validCount ||
                  (candidate.validCount == best.validCount &&
                   candidate.presentCount >= best.presentCount))))
            {
                best = candidate;
                haveVersion = 1;
            }
        }
        out[game] = best;
        if (game > 0 && found)
        {
            Cat3DSResourceProbe probe = {};
            cat3dsProbeResources(game, kFolders[game], &probe);
            out[game].resourceGraphicsValid = probe.graphicsValid;
            out[game].resourceAudioMapsPresent = probe.audioAndMapsPresent;
            out[game].resourceLayoutValid = probe.embeddedLayoutValid;
            out[game].resourceShapesPresent = game != 1 || hasAbyssShapes(kFolders[game]);
            snprintf(out[game].resourceExeName, sizeof(out[game].resourceExeName),
                     "%s", probe.exeName);
            snprintf(out[game].resourceIssue, sizeof(out[game].resourceIssue),
                     "%s", probe.problem);
            out[game].resourceImageSize = probe.imageSize;
            out[game].resourceHeadOffset = probe.headOffset;
            out[game].resourceDictOffset = probe.dictOffset;
        }
    }
    writeInstallReport(out);
}
