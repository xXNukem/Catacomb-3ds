// SPDX-License-Identifier: GPL-2.0-or-later
struct Cat3DS_SlotLabel {
    char title[33], date[20];
};

static void Cat3DS_Date(time_t when, char (&out)[20])
{
    const tm *local = when == (time_t)-1 ? NULL : localtime(&when);
    if (!local || !strftime(out, sizeof(out), "%Y-%m-%d %H:%M", local))
        snprintf(out, sizeof(out), "Date unavailable");
}

#ifdef REFKEEN_VER_CAT3D
static const char *const slotExtension = "c3d";
extern id0_int_t Cat3DS_GetSaveLevel(void);

static bool Cat3DS_SlotHeader(FILE *f, bool writing, Cat3DS_SlotLabel &label)
{
    SaveGame h = {};
#if REFKEEN_USE_BYTE_ALIGNMENT
    const bool packed = true;
#else
    const bool packed = false;
#endif
    unsigned char pad = 0;
    if (writing) {
        memcpy(h.signature, EXTENSION, sizeof(h.signature));
        h.oldtestoffset = refkeen_compat_id_us_printx_offset;
        h.present = true;
        snprintf(h.name, sizeof(h.name), "Level %02d %.19s", Cat3DS_GetSaveLevel(), label.date);
        return fwrite(h.signature, 1, sizeof(h.signature), f) == sizeof(h.signature) &&
               BE_Cross_writeInt16LE(f, &h.oldtestoffset) == 2 &&
               BE_Cross_write_boolean_To16LE(f, &h.present) == 2 &&
               fwrite(h.name, 1, sizeof(h.name), f) == sizeof(h.name) &&
               (packed || fwrite(&pad, 1, 1, f) == 1);
    }
    if (fread(h.signature, 1, sizeof(h.signature), f) != sizeof(h.signature) ||
        memcmp(h.signature, EXTENSION, sizeof(h.signature)) ||
        BE_Cross_readInt16LE(f, &h.oldtestoffset) != 2 ||
        h.oldtestoffset != refkeen_compat_id_us_printx_offset ||
        BE_Cross_read_boolean_From16LE(f, &h.present) != 2 ||
        fread(h.name, 1, sizeof(h.name), f) != sizeof(h.name) ||
        (!packed && fread(&pad, 1, 1, f) != 1))
        return false;
    memcpy(label.title, h.name, sizeof(label.title));
    label.title[sizeof(label.title)-1] = 0;
    for (char *p = label.title; *p; ++p)
        if ((unsigned char)*p < 32 || (unsigned char)*p > 126) *p = '?';
    return true;
}
#elif defined(REFKEEN_VER_CATABYSS)
static const char *const slotExtension = "sav";
extern const id0_char_t *Cat3DS_AbyssLevelName(void);

static bool Cat3DS_SlotHeader(FILE *f, bool writing, Cat3DS_SlotLabel &)
{
    char name[19], version[sizeof(SAVEVER_DATA)];
    const size_t len = refkeen_compat_gelib_gamename_strbufflen;
    if (!len || len > sizeof(name)) return false;
    if (writing)
        return fwrite(refkeen_compat_gelib_gamename, 1, len, f) == len &&
               fwrite(SAVEVER_DATA, 1, sizeof(version), f) == sizeof(version);
    return fread(name, 1, len, f) == len &&
           !memcmp(name, refkeen_compat_gelib_gamename, len) &&
           fread(version, 1, sizeof(version), f) == sizeof(version) &&
           !memcmp(version, SAVEVER_DATA, sizeof(version));
}
#else
#error Missing DOS slot header for this game
#endif
