#ifndef CAT3DS_GAME_FILES_H
#define CAT3DS_GAME_FILES_H

/* Native SD-card installation checker. No proprietary content is bundled. */

enum { CAT3DS_GAME_COUNT = 4 };

typedef struct Cat3DSInstallStatus {
    const char *versionName;
    const char *firstProblemFile;
    const char *firstProblemReason;
    int directoryFound;
    int fileCount;
    int validCount;
    int presentCount;
    int missingCount;
    int wrongSizeCount;
    int wrongCrcCount;
    // Four principal files, required for edition identification.
    // Other files can still be required by the full game once linked.
    int coreFileCount;
    int coreValidCount;
    int corePresentCount;
    int coreMissingCount;
    int coreWrongSizeCount;
    int coreWrongCrcCount;
    const char *coreProblemFile;
    const char *coreProblemReason;
    const char *expectedExeName;
    char matchedExeName[128]; // Name verified by size/CRC, even if renamed
    char foundExeName[128]; // Diagnostic: an unrecognized EXE in the folder
    // Structural EGA probe: accepts resource-compatible DOS distributions
    // regardless of edition CRC. NOT a guarantee that engine code will run.
    bool resourceGraphicsValid;
    bool resourceAudioMapsPresent;
    bool resourceLayoutValid;
    bool resourceShapesPresent;
    char resourceExeName[128];
    char resourceIssue[128];
    unsigned resourceImageSize;
    unsigned resourceHeadOffset;
    unsigned resourceDictOffset;
} Cat3DSInstallStatus;

bool cat3dsCanLaunchApocalypse(const Cat3DSInstallStatus &status);
bool cat3dsCanLaunchArmageddon(const Cat3DSInstallStatus &status);
const char *cat3dsGameName(int game);
const char *cat3dsGameFolder(int game);
void cat3dsScanInstallations(Cat3DSInstallStatus out[CAT3DS_GAME_COUNT]);
/* Creates missing SD folders only; never deletes, clears or overwrites files.
 * On failure, failedPath points to a constant string. */
bool cat3dsEnsureGameFolders(const char **failedPath);

#endif
