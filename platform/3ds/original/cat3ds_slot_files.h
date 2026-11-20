// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sys/stat.h>
#include <unistd.h>

// Paths are built only from the selected ReflectionHLE profile, a slot and a
// fixed DOS extension. Keep the backup readable across an interrupted rename.
struct Cat3DS_SlotFile {
    char name[288], temp[296], backup[296];
    Cat3DS_SlotFile(const char *profile, int slot, const char *extension) {
        snprintf(name, sizeof(name), "%s/savegam%d.%s", profile, slot, extension);
        snprintf(temp, sizeof(temp), "%s.new", name);
        snprintf(backup, sizeof(backup), "%s.bak", name);
    }

    FILE *open() const {
        FILE *f = fopen(name, "rb");
        if (!f && errno == ENOENT)
            f = fopen(backup, "rb");
        return f;
    }

    template<class Writer> bool replace(Writer write) const {
        FILE *f = fopen(temp, "w+b");
        if (!f) return false;
        bool ok = write(f) && !ferror(f) && fflush(f) == 0 && fsync(fileno(f)) == 0;
        if (fclose(f) != 0) ok = false;
        if (!ok) {
            unlink(temp);
            return false;
        }

        // FAT implementations need a vacant rename destination.
        struct stat st;
        if (stat(name, &st) == 0) {
            if (!S_ISREG(st.st_mode)) return false;
            if (unlink(backup) != 0 && errno != ENOENT) return false;
            if (rename(name, backup) != 0) return false;
        } else if (errno != ENOENT) {
            return false;
        }
        if (rename(temp, name) != 0) {
            // If rollback itself fails, open() still finds the backup.
            rename(backup, name);
            return false;
        }
        // A leftover backup is harmless; the published file takes precedence.
        unlink(backup);
        return true;
    }
};
