/* Catacomb 3DS - original Catacomb 3-D map data loader.
 *
 * Uses the game's own MTEMP.TMP embedded in its LZEXE executable and its
 * GAMEMAPS.C3D. Compression (Carmack then RLEW) follows ReflectionHLE's
 * src/id91_11/id_ca.c, CA_CacheMap. No assets are distributed/extracted.
 */
#include "cat3ds_level_map.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

extern "C" {
#include "../../../src/unlzexe/unlzexe.h"
}

namespace {
uint16_t u16(const uint8_t *p) { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }
uint32_t u32(const uint8_t *p) {
    return uint32_t(u16(p)) | (uint32_t(u16(p + 2)) << 16);
}
void errorMsg(char *out, size_t cap, const char *message) {
    if (out && cap) snprintf(out, cap, "%s", message);
}

bool expandCarmack(const uint8_t *src, size_t len, size_t expandedBytes,
                   std::vector<uint16_t> &out)
{
    if (expandedBytes < 2 || (expandedBytes & 1) || expandedBytes > 32768)
        return false;
    const size_t words = expandedBytes / 2;
    out.clear(); out.reserve(words);
    size_t at = 0;
    while (out.size() < words) {
        if (at + 2 > len) return false;
        const uint16_t word = u16(src + at); at += 2;
        const unsigned count = word & 0xff;
        const unsigned high = word >> 8;
        if (high != 0xa7 && high != 0xa8) {
            out.push_back(word);
            continue;
        }
        if (!count) { // Escaped tag: the following BYTE is the low byte.
            if (at >= len) return false;
            out.push_back(uint16_t((word & 0xff00) | src[at++]));
            continue;
        }
        if (out.size() + count > words) return false;
        size_t source;
        if (high == 0xa7) { // Near reference: backward distance in words.
            if (at >= len || !src[at] || src[at] > out.size()) return false;
            source = out.size() - src[at++];
        } else { // Far reference: absolute word offset from start of output.
            if (at + 2 > len) return false;
            source = u16(src + at); at += 2;
            if (source >= out.size()) return false;
        }
        // Allow overlapping LZ copies: each word can refer to one just copied.
        for (unsigned i = 0; i < count; ++i) {
            if (source + i >= out.size()) return false;
            out.push_back(out[source + i]);
        }
    }
    return out.size() == words;
}

bool expandRlew(const std::vector<uint16_t> &src, size_t dstWords,
                uint16_t tag, uint16_t *dst)
{
    if (src.empty() || src[0] != dstWords * 2 || dstWords > 64 * 64)
        return false;
    size_t in = 1, out = 0;
    while (out < dstWords) {
        if (in >= src.size()) return false;
        const uint16_t word = src[in++];
        if (word != tag) {
            dst[out++] = word;
        } else {
            if (in + 2 > src.size()) return false;
            const size_t count = src[in++];
            const uint16_t value = src[in++];
            if (!count || count > dstWords - out) return false;
            for (size_t i = 0; i < count; ++i) dst[out++] = value;
        }
    }
    return out == dstWords;
}

bool readPlane(FILE *fp, uint32_t fileSize, uint32_t start, uint16_t length,
               size_t count, uint16_t tag, uint16_t *dst)
{
    if (length <= 2 || start >= fileSize || uint64_t(start) + length > fileSize)
        return false;
    std::vector<uint8_t> compressed(length);
    if (fseek(fp, long(start), SEEK_SET) ||
        fread(compressed.data(), 1, length, fp) != length)
        return false;
    std::vector<uint16_t> intermediate;
    return expandCarmack(compressed.data() + 2, compressed.size() - 2,
                         u16(compressed.data()), intermediate) &&
           expandRlew(intermediate, count, tag, dst);
}
} // namespace

bool cat3dsLoadCat3dMap(const char *version, const char *exeFilename,
                        const char *folder, unsigned mapIndex,
                        Cat3DSLevelMap *map, char *error, size_t errorSize)
{
    if (!map || !folder || !exeFilename || !*exeFilename ||
        strchr(exeFilename, '/') || strchr(exeFilename, '\\') ||
        strstr(exeFilename, "..") || mapIndex >= 20) {
        errorMsg(error, errorSize, "Invalid map request"); return false;
    }
    const bool v100 = version && strstr(version, "1.00");
    const bool v122 = version && strstr(version, "1.22");
    if (!v100 && !v122) {
        errorMsg(error, errorSize, "Unrecognized Catacomb 3-D edition"); return false;
    }
    const size_t exeBytes = v100 ? 191536u - 0x1400u : 191904u - 0x1600u;
    const size_t mapHeadOff = v100 ? 0x1b170u : 0x1ae20u;
    const size_t mapHeadLen = 618;
    if (mapHeadOff + mapHeadLen > exeBytes) {
        errorMsg(error, errorSize, "Invalid embedded map header"); return false;
    }
    char path[320];
    if (snprintf(path, sizeof(path), "%s/%s", folder, exeFilename) >= (int)sizeof(path)) {
        errorMsg(error, errorSize, "EXE path too long"); return false;
    }
    FILE *exe = fopen(path, "rb");
    if (!exe) {
        errorMsg(error, errorSize, "Cannot open Catacomb EXE"); return false;
    }
    std::vector<uint8_t> image(exeBytes);
    uint16_t extraMem = 0;
    const bool unpacked = Unlzexe_unpack(exe, image.data(), (int)image.size(), &extraMem);
    fclose(exe);
    if (!unpacked) {
        errorMsg(error, errorSize, "LZEXE decompression failed"); return false;
    }
    const uint8_t *const header = image.data() + mapHeadOff;
    const uint16_t rlewTag = u16(header);
    const uint32_t mapHeaderPosition = u32(header + 2 + mapIndex * 4);
    if (!mapHeaderPosition || mapHeaderPosition == 0xffffffffu) {
        errorMsg(error, errorSize, "Map slot is empty"); return false;
    }
    if (snprintf(path, sizeof(path), "%s/GAMEMAPS.C3D", folder) >= (int)sizeof(path)) {
        errorMsg(error, errorSize, "Map file path too long"); return false;
    }
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        errorMsg(error, errorSize, "Cannot open GAMEMAPS.C3D"); return false;
    }
    bool ok = false;
    do {
        if (fseek(fp, 0, SEEK_END)) break;
        const long size = ftell(fp);
        if (size <= 0 || size > 4 * 1024 * 1024 ||
            mapHeaderPosition < 8 || uint64_t(mapHeaderPosition) + 38 > uint64_t(size)) break;
        if (fseek(fp, long(mapHeaderPosition), SEEK_SET)) break;
        uint8_t mh[38];
        if (fread(mh, 1, sizeof(mh), fp) != sizeof(mh)) break;
        const unsigned w = u16(mh + 18), h = u16(mh + 20);
        if (!w || !h || w > 64 || h > 64) break;
        const uint32_t plane0 = u32(mh), plane2 = u32(mh + 8);
        const uint16_t len0 = u16(mh + 12), len2 = u16(mh + 16);
        Cat3DSLevelMap result = {};
        result.width = w; result.height = h; result.index = mapIndex;
        memcpy(result.name, mh + 22, 16); result.name[16] = 0;
        for (unsigned n = 0; n < 16; ++n)
            if (result.name[n] && (unsigned char)result.name[n] < 32)
                result.name[n] = ' ';
        const size_t count = w * h;
        if (!readPlane(fp, uint32_t(size), plane0, len0, count, rlewTag, result.walls) ||
            !readPlane(fp, uint32_t(size), plane2, len2, count, rlewTag, result.objects)) break;
        *map = result;
        ok = true;
    } while (false);
    fclose(fp);
    errorMsg(error, errorSize, ok ? "OK" : "Map header or plane decompression failed");
    return ok;
}
