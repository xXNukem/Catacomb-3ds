/* Catacomb 3DS - original EGA wall art loader for the first-person test.
 * Resource formats: ReflectionHLE id91_11/id_ca.c and cat3d/gfxe_c3d.h.
 * Lighting and tile mapping: ReflectionHLE cat3d/c3_draw.c.
 * Reads only from the user's DOS installation; no extracted art on SD.
 */
#include "cat3ds_wall_textures.h"

#include <stdio.h>
#include <string.h>
#include <vector>

extern "C" {
#include "../../../src/unlzexe/unlzexe.h"
}

namespace {
uint16_t u16(const uint8_t *p) { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }
uint32_t u24(const uint8_t *p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16); }
uint32_t u32(const uint8_t *p) { return u24(p) | (uint32_t(p[3]) << 24); }

void setMessage(char *out, size_t size, const char *message)
{
    if (out && size) snprintf(out, size, "%s", message);
}

/* Huffman-coded EGA chunks use the low bit first; node 254 is the root.
 * Avoid overreading malformed input or following invalid tree pointers. */
bool huffman(const uint8_t *data, size_t count, const uint8_t *dict,
             std::vector<uint8_t> &out)
{
    size_t decoded = 0;
    unsigned node = 254;
    for (size_t i = 0; i < count && decoded < out.size(); ++i)
        for (unsigned bit = 0; bit < 8 && decoded < out.size(); ++bit)
        {
            const unsigned direction = (data[i] >> bit) & 1u;
            const uint16_t link = u16(dict + 4 * node + 2 * direction);
            if (link >= 256)
            {
                node = unsigned(link) - 256u;
                if (node >= 255) return false;
            }
            else
            {
                out[decoded++] = uint8_t(link);
                node = 254;
            }
        }
    return decoded == out.size();
}

bool readChunk(FILE *graphics, long graphicsSize, const uint8_t *head,
               const uint8_t *dict, unsigned chunk, std::vector<uint8_t> &out)
{
    /* EGAHEAD.C3D contains 479 little-endian 24-bit offsets for 478 chunks. */
    if (chunk >= 478) return false;
    const uint32_t start = u24(head + 3 * chunk);
    if (start == 0xffffffu) return false;
    unsigned next = chunk + 1;
    while (next <= 478 && u24(head + 3 * next) == 0xffffffu) ++next;
    if (next > 478) return false;
    const uint32_t end = u24(head + 3 * next);
    if (end <= start + 4u || end > uint32_t(graphicsSize) || end - start > 300000u)
        return false;
    std::vector<uint8_t> compressed(end - start);
    if (fseek(graphics, long(start), SEEK_SET) ||
        fread(compressed.data(), 1, compressed.size(), graphics) != compressed.size())
        return false;
    const uint32_t expanded = u32(compressed.data());
    if (!expanded || expanded > 128000u) return false;
    out.assign(expanded, 0);
    return huffman(compressed.data() + 4, compressed.size() - 4, dict, out);
}

bool safePath(char *out, size_t len, const char *folder, const char *file)
{
    if (!folder || !file || !*file || strchr(file, '/') || strchr(file, '\\') || strstr(file, ".."))
        return false;
    const int written = snprintf(out, len, "%s/%s", folder, file);
    return written > 0 && size_t(written) < len;
}
} // namespace

int cat3dsWallPicture(unsigned tile, bool ySide)
{
    /* Array values copied from the original walllight1/walldark1 mapping.
     * 1..14: seven types duplicated; 15..17: exploding walls;
     * 18..33: four color-coded door families, two alternating images each.
     * Tile 0 and the remaining special codes have no ordinary picture. */
    static const uint8_t light[7] = {137, 140, 142, 144, 146, 148, 150};
    static const uint8_t dark [7] = {138, 139, 141, 143, 145, 147, 149};
    if (tile >= 1 && tile <= 14)
        return ySide ? dark[(tile - 1) % 7] : light[(tile - 1) % 7];
    if (tile >= 15 && tile <= 17)
        return 134 + int(tile - 15);
    if (tile >= 18 && tile <= 33)
        return 151 + int((tile - 18) / 4 * 2 + ((tile - 18) & 1));
    return -1;
}

bool cat3dsLoadCat3dWallTextures(const char *version, const char *exeFilename,
                                const char *folder, Cat3DSWallTextures *textures,
                                char *error, size_t errorSize)
{
    if (!textures || !folder || !version)
    { setMessage(error, errorSize, "Invalid graphics request"); return false; }
    if (textures->ready)
    { setMessage(error, errorSize, "OK"); return true; }
    const bool v100 = strstr(version, "1.00") != NULL;
    const bool v122 = strstr(version, "1.22") != NULL;
    if (!v100 && !v122)
    { setMessage(error, errorSize, "Unknown Catacomb 3-D edition"); return false; }
    const size_t imageBytes = v100 ? 191536u - 0x1400u : 191904u - 0x1600u;
    const size_t dictOffset = v100 ? 0x23064u : 0x22fd8u;
    const size_t headOffset = v100 ? 0x1abd0u : 0x1a880u;
    if (dictOffset + 1024 > imageBytes || headOffset + 1437 > imageBytes)
    { setMessage(error, errorSize, "Invalid embedded graphics offsets"); return false; }
    char path[320];
    if (!safePath(path, sizeof(path), folder, exeFilename))
    { setMessage(error, errorSize, "Invalid EXE filename"); return false; }
    FILE *exe = fopen(path, "rb");
    if (!exe)
    { setMessage(error, errorSize, "Cannot open Catacomb EXE"); return false; }
    std::vector<uint8_t> image(imageBytes);
    uint16_t extraMemory = 0;
    const bool unpacked = Unlzexe_unpack(exe, image.data(), int(image.size()), &extraMemory);
    fclose(exe);
    if (!unpacked)
    { setMessage(error, errorSize, "Unable to unpack Catacomb EXE"); return false; }

    if (snprintf(path, sizeof(path), "%s/EGAGRAPH.C3D", folder) >= int(sizeof(path)))
    { setMessage(error, errorSize, "Graphics path too long"); return false; }
    FILE *graphics = fopen(path, "rb");
    if (!graphics)
    { setMessage(error, errorSize, "Cannot open EGAGRAPH.C3D"); return false; }
    bool ok = true;
    do {
        if (fseek(graphics, 0, SEEK_END)) { ok = false; break; }
        const long bytes = ftell(graphics);
        if (bytes <= 0 || bytes > 2000000) { ok = false; break; }
        const uint8_t *head = image.data() + headOffset;
        const uint8_t *dictionary = image.data() + dictOffset;
        std::vector<uint8_t> pictable;
        if (!readChunk(graphics, bytes, head, dictionary, 0, pictable) || pictable.size() != 155u * 4u)
        { ok = false; break; }
        for (unsigned pic = 134; pic <= 158; ++pic)
        {
            const uint8_t *dimensions = pictable.data() + (pic - 5u) * 4u;
            if (u16(dimensions) != 8 || u16(dimensions + 2) != 64)
            { ok = false; break; }
            std::vector<uint8_t> planar;
            if (!readChunk(graphics, bytes, head, dictionary, pic, planar) || planar.size() != 2048)
            { ok = false; break; }
            uint8_t *pixels = textures->pixels[pic - 134];
            for (unsigned y = 0; y < 64; ++y)
                for (unsigned x = 0; x < 64; ++x)
                {
                    const unsigned offset = y * 8 + x / 8;
                    const unsigned mask = 0x80u >> (x & 7);
                    uint8_t color = 0;
                    for (unsigned plane = 0; plane < 4; ++plane)
                        if (planar[plane * 512u + offset] & mask) color |= uint8_t(1u << plane);
                    pixels[y * 64 + x] = color;
                }
        }
    } while (false);
    fclose(graphics);
    textures->ready = ok;
    setMessage(error, errorSize, ok ? "OK" : "Invalid EGA wall chunk (134..158)");
    return ok;
}
