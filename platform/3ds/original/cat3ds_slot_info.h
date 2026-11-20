// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "cat3ds_slot_files.h"
#include "crc32/crc32.h"
#include <cstddef>
#include <initializer_list>

// V1 had no link to its SAV. For V1, missing or stale metadata the caller
// derives the level from the validated payload and the date from the SAV.
struct Cat3DS_SlotInfo {
    char magic[8], title[33], date[20];
    unsigned char payloadCrc[4], modified[8], infoCrc[4];
};

static bool Cat3DS_WriteSlotInfo(const char *profile, int slot, const Cat3DS_SlotFile &data,
                                 const char *title, const char *date)
{
    Cat3DS_SlotInfo info = {};
    FILE *f = data.open();
    if (!f) return false;
    uint32_t crc;
    struct stat st;
    bool ok = fstat(fileno(f), &st) == 0 && Crc32_ComputeFile(f, &crc) == 0;
    if (ok) {
        int64_t modified = st.st_mtime;
        memcpy(info.modified, &modified, sizeof(modified));
    }
    if (fclose(f) || !ok) return false;
    memcpy(info.magic, "C3DSABS2", 8);
    snprintf(info.title, sizeof(info.title), "%s", title);
    snprintf(info.date, sizeof(info.date), "%s", date);
    memcpy(info.payloadCrc, &crc, 4);
    crc = Crc32_ComputeBuf(0, &info, offsetof(Cat3DS_SlotInfo, infoCrc));
    memcpy(info.infoCrc, &crc, 4);
    return Cat3DS_SlotFile(profile, slot, "inf").replace([&](FILE *out) {
        return fwrite(&info, sizeof(info), 1, out) == 1;
    });
}

static void Cat3DS_ReadSlotInfo(const char *profile, int slot, FILE *data,
                                char (&title)[33], char (&date)[20])
{
    FILE *f = Cat3DS_SlotFile(profile, slot, "inf").open();
    if (!f) return;
    Cat3DS_SlotInfo info;
    bool ok = fread(&info, sizeof(info), 1, f) == 1 && fgetc(f) == EOF && !ferror(f);
    fclose(f);
    if (!ok || memcmp(info.magic, "C3DSABS2", 8)) return;
    uint32_t crc = Crc32_ComputeBuf(0, &info, offsetof(Cat3DS_SlotInfo, infoCrc));
    if (memcmp(&crc, info.infoCrc, 4) || fseek(data, 0, SEEK_SET) ||
        Crc32_ComputeFile(data, &crc) || memcmp(&crc, info.payloadCrc, 4))
        return;
    struct stat st;
    if (fstat(fileno(data), &st) != 0) return;
    const int64_t modified = st.st_mtime;
    if (memcmp(info.modified, &modified, sizeof(modified))) return;
    for (const auto *field : {info.title, info.date}) {
        size_t n = field == info.title ? sizeof(info.title) : sizeof(info.date);
        if (field[n-1]) return;
        for (const char *p = field; *p; ++p)
            if ((unsigned char)*p < 32 || (unsigned char)*p > 126) return;
    }
    memcpy(title, info.title, sizeof(title));
    memcpy(date, info.date, sizeof(date));
}
