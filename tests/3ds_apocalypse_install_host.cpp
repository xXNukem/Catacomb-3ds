// Exercise backend installation acceptance with synthetic checksums on a private
// manifest copy. Filesystem access uses the host's temporary test directory.
#include "backend/filesystem/be_filesystem_gameinst.h"
#include "backend/filesystem/be_filesystem_app_paths.h"
#include "backend/filesystem/be_filesystem_file_ops.h"
#include "cat3ds_apocalypse.h"
#include "cat3ds_manifest.h"
#include "crc32/crc32.h"
#include "be_gamestr.h"
#include <cassert>
#include <cerrno>
#include <string>
#include <sys/stat.h>
#include <vector>

TCHAR g_be_appDataPath[BE_CROSS_PATH_LEN_BOUND];
void BEL_Cross_mkdir(const TCHAR *path) { assert(!mkdir(path, 0700) || errno == EEXIST); }
BE_FILE_T BEL_Cross_apply_file_action_in_dir(const char *name, BE_FileRequest_T action,
                                             const TCHAR *dir, TCHAR (*)[BE_CROSS_PATH_LEN_BOUND])
{
    assert(action == BE_FILE_REQUEST_READ);
    return fopen((std::string(dir) + "/" + name).c_str(), "rb");
}

void checkApocalypseInstall(const char *root)
{
    std::vector<BE_GameFileDetails_T> files;
    const char marker = 'A';
    const std::string bad = std::string(root) + "/AUDIO.APC";
    for (auto &entry : cat3ds_files_catapoc101) {
        BE_GameFileDetails_T file = {entry.name, int(entry.size), entry.crc32};
        if (cat3dsApocalypseGameplayFile(entry.name)) {
            file.filesize = 1;
            file.crc32 = Crc32_ComputeBuf(0, &marker, 1);
            FILE *f = fopen((std::string(root) + "/" + entry.name).c_str(), "wb");
            assert(f && fputc(!strcmp(entry.name, "AUDIO.APC") ? 'X' : marker, f) != EOF);
            assert(fclose(f) == 0);
        }
        files.push_back(file);
    }
    files.push_back({});
    BE_GameVerDetails_T version = {};
    version.reqFiles = files.data();
    version.description = "Synthetic Apocalypse";
    version.verId = BE_GAMEVER_CATAPOC101;
    version.writableFilesDir = BE_STR_GAMEVER_CATAPOC101;
    snprintf(g_be_appDataPath, sizeof(g_be_appDataPath), "%s/saves-apoc", root);
    BE_TryAddGameInstallation_ErrorMsg_T error = {};
    BEL_Cross_ConditionallyAddGameInstallation_WithReturnedErrMsg(&version, root, nullptr, &error);
    assert(!g_be_gameinstallationsbyver[version.verId] && strstr(error, "AUDIO.APC"));
    FILE *f = fopen(bad.c_str(), "wb");
    assert(f && fputc(marker, f) != EOF && !fclose(f));
    BEL_Cross_ConditionallyAddGameInstallation_WithReturnedErrMsg(&version, root, nullptr, &error);
    const auto *installation = g_be_gameinstallationsbyver[version.verId];
    const auto profile = std::string(root) + "/saves-apoc/catapoc101";
    assert(installation && profile == installation->writableFilesPath);
    struct stat info;
    assert(!stat(profile.c_str(), &info) && S_ISDIR(info.st_mode));
}
