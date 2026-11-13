/* Catacomb 3DS - Catacomb 3-D title preview.
 *
 * Reads the user's original CAT3D.EXE and EGAGRAPH.C3D. ReflectionHLE's
 * existing LZEXE unpacker supplies the embedded Huffman dictionary and
 * chunk table. The graphics Huffman/planar layout follows the original
 * id91_11/id_ca.c and id91_11/id_vw_ae.c in ReflectionHLE.
 *
 * No original game assets are redistributed. This is a preview only,
 * not a replacement for the eventual original resource-cache engine.
 */
#include "cat3ds_title_preview.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "../../../src/unlzexe/unlzexe.h"
#include "../../../src/backend/video/be_video.h"
#include "../../../src/backend/video/be_video_emu.h"
void BE_ST_SetScreenMode(int mode);
void BE_ST_SetScreenStartAddress(uint16_t address);
void BE_ST_EGASetLineWidth(uint8_t width);
void BE_ST_EGASetPelPanning(uint8_t panning);
void BE_ST_EGASetSplitScreen(int16_t line);
void BE_ST_EGAUpdateGFXBufferFrom4bitsPixel(uint16_t off, uint8_t color, uint16_t count);
void BE_ST_EGAUpdateGFXBufferInPlane(uint16_t off, const uint8_t *src, uint16_t num, uint16_t plane);
}

static uint16_t read16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t read24(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

static uint32_t read32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Decoding of the EGA Huffman tree: 255 nodes, each containing two
 * little-endian 16-bit links; node 254 is the root. The low bit of each
 * compressed byte is consumed first, as in CAL_HuffExpand. */
static bool expandHuffman(const uint8_t *encoded, size_t encodedSize,
                          uint8_t *decoded, size_t decodedSize,
                          const uint8_t *dictionary)
{
    unsigned node = 254;
    size_t bit = 0;
    size_t out = 0;
    while (out < decodedSize)
    {
        if (bit >= encodedSize * 8)
            return false;
        const unsigned direction = (encoded[bit / 8] >> (bit & 7)) & 1u;
        ++bit;
        const uint16_t link = read16(dictionary + 4 * node + 2 * direction);
        if (link >= 256)
        {
            node = (unsigned)link - 256u;
            if (node >= 255)
                return false;
        }
        else
        {
            decoded[out++] = (uint8_t)link;
            node = 254;
        }
    }
    return true;
}

static bool readChunk(FILE *graphic, const uint8_t *head,
                      unsigned numChunks, unsigned chunk,
                      const uint8_t *dictionary, uint8_t **out,
                      size_t *outSize)
{
    if (chunk >= numChunks)
        return false;
    const uint32_t begin = read24(head + 3 * chunk);
    if (begin == 0xffffffu)
        return false;
    unsigned next = chunk + 1;
    while (next <= numChunks && read24(head + 3 * next) == 0xffffffu)
        ++next;
    if (next > numChunks)
        return false;
    const uint32_t end = read24(head + 3 * next);
    if (end <= begin || end - begin <= 4 || end - begin > 300000u)
        return false;
    const size_t length = end - begin;
    uint8_t *compressed = (uint8_t *)malloc(length);
    if (!compressed)
        return false;
    const bool got = fseek(graphic, begin, SEEK_SET) == 0 &&
                     fread(compressed, 1, length, graphic) == length;
    if (!got)
    {
        free(compressed);
        return false;
    }
    const size_t expanded = read32(compressed);
    /* Title and picture table chunks are small; reject corrupted headers. */
    if (!expanded || expanded > 128000u)
    {
        free(compressed);
        return false;
    }
    uint8_t *result = (uint8_t *)malloc(expanded);
    if (!result)
    {
        free(compressed);
        return false;
    }
    const bool ok = expandHuffman(compressed + 4, length - 4, result,
                                  expanded, dictionary);
    free(compressed);
    if (!ok)
    {
        free(result);
        return false;
    }
    *out = result;
    *outSize = expanded;
    return true;
}

static void setError(char *error, size_t errorSize, const char *message)
{
    if (error && errorSize)
        snprintf(error, errorSize, "%s", message);
}

bool cat3dsLoadCat3dTitle(const char *versionName, const char *exeFilename, char *error, size_t errorSize)
{
    /* Original positions in the *unpacked image* (not on-disk EXE offsets).
     * Source: ReflectionHLE be_gamedefs_cat3d.h, v1.00 and v1.22. */
    const bool oldVersion = versionName && strstr(versionName, "1.00");
    const bool newVersion = versionName && strstr(versionName, "1.22");
    if (!oldVersion && !newVersion)
    {
        setError(error, errorSize, "Unsupported Catacomb 3-D version");
        return false;
    }
    const size_t imageSize = oldVersion ? (191536u - 0x1400u)
                                        : (191904u - 0x1600u);
    const size_t dictOff = oldVersion ? 0x23064u : 0x22fd8u;
    const size_t headOff = oldVersion ? 0x1abd0u : 0x1a880u;
    const size_t dictSize = 1024u;
    const size_t headSize = 1437u;
    const unsigned numChunks = 478u;
    const unsigned titleChunk = 21u; // TITLEPIC in gfxe_c3d.h
    const unsigned firstPicChunk = 5u; // STARTPICS in gfxe_c3d.h
    const unsigned picCount = 155u; // NUMPICS in gfxe_c3d.h
    if (dictOff + dictSize > imageSize || headOff + headSize > imageSize)
    {
        setError(error, errorSize, "Invalid embedded resource positions");
        return false;
    }

    const char *const folder = "sdmc:/3ds/catacomb-3ds/games/cat3d/";
    char path[192];
    if (!exeFilename || !exeFilename[0] || strchr(exeFilename, '/') || strchr(exeFilename, '\\'))
    {
        setError(error, errorSize, "Missing/invalid executable filename");
        return false;
    }
    snprintf(path, sizeof(path), "%s%s", folder, exeFilename);
    FILE *exe = fopen(path, "rb");
    if (!exe)
    {
        setError(error, errorSize, "Cannot open recognized Catacomb EXE");
        return false;
    }
    uint8_t *image = (uint8_t *)malloc(imageSize);
    uint16_t minExtraMemory = 0;
    const bool unpacked = image && Unlzexe_unpack(exe, image, (int)imageSize,
                                                   &minExtraMemory);
    fclose(exe);
    if (!unpacked)
    {
        free(image);
        setError(error, errorSize, "Unable to unpack recognized Catacomb EXE");
        return false;
    }

    snprintf(path, sizeof(path), "%sEGAGRAPH.C3D", folder);
    FILE *graphics = fopen(path, "rb");
    if (!graphics)
    {
        free(image);
        setError(error, errorSize, "Cannot open EGAGRAPH.C3D");
        return false;
    }
    const uint8_t *dictionary = image + dictOff;
    const uint8_t *head = image + headOff;
    uint8_t *picTable = NULL;
    uint8_t *title = NULL;
    size_t picSize = 0, titleSize = 0;
    bool ok = readChunk(graphics, head, numChunks, 0, dictionary,
                        &picTable, &picSize);
    if (ok && picSize >= picCount * 4u)
        ok = readChunk(graphics, head, numChunks, titleChunk,
                       dictionary, &title, &titleSize);
    fclose(graphics);
    free(image);

    unsigned widthBytes = 0, height = 0;
    if (ok)
    {
        /* Each pictable entry is {width-in-EGA-bytes, height} (u16 LE). */
        const uint8_t *dims = picTable + (titleChunk - firstPicChunk) * 4u;
        widthBytes = read16(dims);
        height = read16(dims + 2);
        ok = widthBytes > 0 && widthBytes <= 40 && height > 0 &&
             height <= 200 && titleSize == 4u * widthBytes * height;
    }
    free(picTable);
    if (!ok)
    {
        free(title);
        setError(error, errorSize, "Unable to decode original TITLEPIC");
        return false;
    }

    BE_ST_SetScreenMode(0xD);
    BE_ST_SetScreenStartAddress(0);
    BE_ST_EGASetLineWidth(40);
    BE_ST_EGASetPelPanning(0);
    BE_ST_EGASetSplitScreen(-2);
    BE_ST_EGAUpdateGFXBufferFrom4bitsPixel(0, 0, 40 * 200);
    const unsigned startX = (40 - widthBytes) / 2u;
    const unsigned startY = (200 - height) / 2u;
    const size_t planeSize = widthBytes * height;
    for (unsigned plane = 0; plane < 4; ++plane)
        for (unsigned y = 0; y < height; ++y)
        {
            const uint16_t dest = (uint16_t)((startY + y) * 40u + startX);
            const uint8_t *row = title + plane * planeSize + y * widthBytes;
            BE_ST_EGAUpdateGFXBufferInPlane(dest, row,
                                             (uint16_t)widthBytes, (uint16_t)plane);
        }
    free(title);
    setError(error, errorSize, "OK");
    return true;
}
