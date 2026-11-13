/* Catacomb 3DS - read-only structural graphics-resource probe.
 *
 * Formats based on the publicly available ReflectionHLE source:
 * src/unlzexe/unlzexe.c, src/id91_11/id_ca.c, src/id91_11/id_vw_ae.c.
 * Unlike edition checksums, the data-format validation can recognize other
 * DOS distributions with the same EGA archive/chunk layout.
 *
 * No game files are changed, extracted to the SD, or included in the port.
 * This validates EGA assets, NOT gameplay differences between executable
 * versions. Error conditions must not be treated as proof of corruption.
 */
#include "cat3ds_resource_probe.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <vector>

#ifdef __3DS__
#ifndef REFKEEN_UNIFIED_BUILD
extern "C" {
#endif
void BE_ST_SetScreenMode(int mode);
void BE_ST_SetScreenStartAddress(uint16_t crtc);
void BE_ST_EGASetLineWidth(uint8_t width);
void BE_ST_EGASetPelPanning(uint8_t panning);
void BE_ST_EGASetSplitScreen(int16_t line);
void BE_ST_EGAUpdateGFXBufferFrom4bitsPixel(uint16_t off, uint8_t color, uint16_t count);
void BE_ST_EGAUpdateGFXBufferInPlane(uint16_t off, const uint8_t *src, uint16_t num, uint16_t plane);
#ifndef REFKEEN_UNIFIED_BUILD
}
#endif
#endif

namespace {

const char *const kExtensions[] = {"", "ABS", "ARM", "APC"};
const unsigned kChunks[] = {0, 626, 658, 682};
const unsigned kPics[]   = {0, 309, 344, 367};

uint16_t u16(const uint8_t *p) { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }
uint32_t u24(const uint8_t *p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16); }
uint32_t u32(const uint8_t *p) { return u24(p) | (uint32_t(p[3]) << 24); }

bool equalNoCase(const char *a, const char *b)
{
    for (; *a && *b; ++a, ++b)
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b))
            return false;
    return *a == *b;
}

bool hasExtension(const char *s, const char *ext)
{
    const char *dot = strrchr(s, '.');
    return dot && equalNoCase(dot + 1, ext);
}

void setMessage(char *out, size_t cap, const char *msg)
{
    if (cap)
        snprintf(out, cap, "%s", msg);
}

bool pathFor(char *out, size_t cap, const char *folder, const char *name)
{
    const int n = snprintf(out, cap, "%s/%s", folder, name);
    return n >= 0 && size_t(n) < cap;
}

bool findFile(const char *folder, const char *required, char outName[128])
{
    outName[0] = 0;
    DIR *dir = opendir(folder);
    if (!dir)
        return false;
    struct dirent *entry;
    while ((entry = readdir(dir)))
        if (equalNoCase(entry->d_name, required) && strlen(entry->d_name) < 128)
        {
            snprintf(outName, 128, "%s", entry->d_name);
            break;
        }
    closedir(dir);
    return outName[0] != 0;
}

bool readFile(const char *path, std::vector<uint8_t> &out, size_t limit)
{
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return false;
    bool ok = false;
    if (fseek(fp, 0, SEEK_END) == 0)
    {
        const long len = ftell(fp);
        if (len >= 0 && size_t(len) <= limit &&
            fseek(fp, 0, SEEK_SET) == 0)
        {
            out.resize(size_t(len));
            ok = out.empty() || fread(out.data(), 1, out.size(), fp) == out.size();
        }
    }
    fclose(fp);
    return ok;
}

struct LzBits {
    const std::vector<uint8_t> &data;
    size_t pos;
    uint16_t bits;
    int remain;
    bool good;
    LzBits(const std::vector<uint8_t> &input, size_t start)
        : data(input), pos(start), bits(0), remain(16), good(start + 2 <= input.size())
    {
        if (good) { bits = u16(&data[pos]); pos += 2; }
    }
    int byte()
    {
        if (pos == data.size()) { good = false; return 0; }
        return data[pos++];
    }
    int bit()
    {
        if (!good) return 0;
        const int val = bits & 1;
        if (--remain == 0)
        {
            if (pos + 2 > data.size()) { good = false; return 0; }
            bits = u16(&data[pos]); pos += 2; remain = 16;
        }
        else bits >>= 1;
        return val;
    }
};

/* LZEXE 0.90/0.91 image decoding. We only need the DOS load image,
 * not its relocations or a new executable. Limits protect against invalid
 * input and distinguish large game EXEs from small intro/helper programs. */
bool unpackGameExe(const std::vector<uint8_t> &file, std::vector<uint8_t> &image)
{
    if (file.size() < 64 || (u16(file.data()) != 0x5a4d && u16(file.data()) != 0x4d5a))
        return false;
    const uint16_t headerParagraphs = u16(&file[8]);
    const uint16_t packedCS = u16(&file[22]);
    const uint16_t packedIP = u16(&file[20]);
    const size_t entry = (size_t(headerParagraphs) + packedCS) * 16u + packedIP;
    const size_t metadata = (size_t(headerParagraphs) + packedCS) * 16u;
    static const uint8_t signature[] = {0x06, 0x0e, 0x1f, 0x8b, 0x0e, 0x0c, 0x00, 0x8b};
    if (entry + sizeof(signature) > file.size() || metadata + 16 > file.size() ||
        memcmp(file.data() + entry, signature, sizeof(signature)))
        return false;
    const uint16_t compressedParagraphs = u16(&file[metadata + 8]);
    const int64_t start = (int64_t(packedCS) - compressedParagraphs + headerParagraphs) * 16;
    if (start < 0 || size_t(start) + 2 > file.size())
        return false;
    LzBits stream(file, size_t(start));
    image.clear();
    image.reserve(210000);
    const size_t limit = 300000;
    bool finished = false;
    while (stream.good && image.size() <= limit)
    {
        if (stream.bit())
        {
            const int val = stream.byte();
            if (!stream.good) break;
            image.push_back(uint8_t(val));
            continue;
        }
        int length, displacement;
        if (!stream.bit())
        {
            length = (stream.bit() << 1) | stream.bit();
            length += 2;
            displacement = stream.byte() - 256;
        }
        else
        {
            const int low = stream.byte();
            const int lenbyte = stream.byte();
            const uint16_t packed = uint16_t(low | ((lenbyte & ~7) << 5) | 0xe000);
            displacement = int16_t(packed);
            length = (lenbyte & 7) + 2;
            if (length == 2)
            {
                length = stream.byte();
                if (length == 0) { finished = stream.good; break; }
                if (length == 1) continue;
                ++length;
            }
        }
        if (!stream.good || length < 0 || image.size() + size_t(length) > limit ||
            displacement >= 0 || size_t(-displacement) > image.size())
            break;
        for (int i = 0; i < length; ++i)
            image.push_back(image[image.size() + displacement]);
    }
    return finished && image.size() >= 120000 && image.size() <= limit;
}

/* Structural EGAHEAD check: one 24-bit start offset per chunk plus
 * an end entry, including 0xFFFFFF sparse entries. */
bool findHead(const std::vector<uint8_t> &image, uint32_t graphicsLength,
              unsigned chunks, size_t &headOffset, std::vector<uint32_t> &offsets)
{
    const size_t bytes = (size_t(chunks) + 1) * 3;
    if (image.size() < bytes)
        return false;
    for (size_t last = chunks * 3; last + 3 <= image.size(); ++last)
    {
        if (u24(&image[last]) != graphicsLength)
            continue;
        const size_t start = last - chunks * 3;
        if (u24(&image[start]) != 0 || (u24(&image[start + 3]) != 0xffffff &&
                                     u24(&image[start + 3]) > graphicsLength))
            continue;
        std::vector<uint32_t> found;
        found.reserve(chunks + 1);
        uint32_t prev = 0;
        bool valid = true;
        unsigned usable = 0;
        for (unsigned i = 0; i <= chunks; ++i)
        {
            const uint32_t v = u24(&image[start + 3u * i]);
            if (v != 0xffffff)
            {
                if (v < prev || v > graphicsLength) { valid = false; break; }
                prev = v;
                ++usable;
            }
            found.push_back(v);
        }
        if (valid && usable > chunks / 2)
        {
            headOffset = start;
            offsets.swap(found);
            return true;
        }
    }
    return false;
}

/* A canonical 255-node Huffman tree contains exactly 510 distinct links
 * (literal symbols 0..255 plus references 256..509). Tree children
 * reference earlier nodes; the root is node 254. */
bool isHuffmanTree(const uint8_t *data)
{
    if (u32(data + 1020) != 0) return false;
    bool seen[510] = {};
    for (unsigned edge = 0; edge < 510; ++edge)
    {
        const unsigned v = u16(data + 2 * edge);
        if (v >= 510 || seen[v] || (v >= 256 && (v - 256) >= edge / 2))
            return false;
        seen[v] = true;
    }
    return true;
}

bool expandHuffman(const uint8_t *source, size_t sourceLength,
                   const uint8_t *dictionary, size_t expanded,
                   std::vector<uint8_t> &output, size_t *usedBits = nullptr)
{
    if (!expanded || expanded > 128000 || sourceLength > 300000)
        return false;
    output.resize(expanded);
    unsigned node = 254;
    size_t bit = 0, dst = 0;
    while (dst < expanded)
    {
        if (bit >= sourceLength * 8) return false;
        const unsigned direction = (source[bit / 8] >> (bit & 7)) & 1u;
        ++bit;
        const unsigned next = u16(dictionary + 4 * node + 2 * direction);
        if (next >= 256)
        {
            node = next - 256;
            if (node >= 255) return false;
        }
        else
        {
            output[dst++] = uint8_t(next);
            node = 254;
        }
    }
    if (usedBits) *usedBits = bit;
    return true;
}

bool loadChunk(FILE *graphics, const std::vector<uint32_t> &offsets,
               unsigned chunk, const uint8_t *dictionary,
               std::vector<uint8_t> &decoded)
{
    if (chunk + 1 >= offsets.size() || offsets[chunk] == 0xffffff)
        return false;
    unsigned next = chunk + 1;
    while (next < offsets.size() && offsets[next] == 0xffffff) ++next;
    if (next >= offsets.size() || offsets[next] <= offsets[chunk])
        return false;
    const size_t bytes = offsets[next] - offsets[chunk];
    if (bytes <= 4 || bytes > 300000)
        return false;
    std::vector<uint8_t> source(bytes);
    if (fseek(graphics, long(offsets[chunk]), SEEK_SET) != 0 ||
        fread(source.data(), 1, bytes, graphics) != bytes)
        return false;
    return expandHuffman(source.data() + 4, bytes - 4, dictionary,
                         u32(source.data()), decoded);
}

struct DecodedResources {
    std::vector<uint8_t> image;
    std::vector<uint32_t> offsets;
    std::vector<uint8_t> pictable;
    std::vector<uint8_t> statusGraphic;
    unsigned dictOff;
    unsigned headOff;
    unsigned imageSize;
    bool embeddedLayoutValid;
    unsigned audioDictOffset, audioHeadOffset, mapHeadOffset;
    unsigned audioDictCrc, audioHeadCrc, egaDictCrc, egaHeadCrc, mapHeadCrc;
    char exeName[128];
    char graphicsName[128];
};

uint32_t crcBuffer(const uint8_t *data, size_t n)
{
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < n; ++i)
    {
        crc ^= data[i];
        for (int k = 0; k < 8; ++k)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xedb88320u : 0);
    }
    return ~crc;
}

/* Locate embedded tables by their CONTENTS, not by the fixed DOS-build
 * deltas. The supplied editions can move AUDIOHHD/MTEMP without changing
 * EGA graphics. Every inferred location is checked against AUDIO/GAMEMAPS.
 * The original known-layout offsets remain the fast path. */

bool openArchive(const char *folder, const char *extension, const char *stem,
                 std::vector<uint8_t> &bytes)
{
    char required[40], actual[128], path[320];
    snprintf(required, sizeof(required), "%s.%s", stem, extension);
    return findFile(folder, required, actual) &&
           pathFor(path, sizeof(path), folder, actual) &&
           readFile(path, bytes, 2000000);
}

bool audioHeadShape(const std::vector<uint8_t> &image, size_t at,
                    size_t count, size_t audioSize)
{
    if (!audioSize || at > image.size() || count > (image.size() - at) / 4)
        return false;
    uint32_t prev = 0;
    unsigned entries = 0, differences = 0;
    for (size_t i = 0; i < count; ++i)
    {
        const uint32_t value = u32(image.data() + at + i * 4);
        if (value == 0xffffffffu) continue;
        if (value < prev || value > audioSize) return false;
        differences += value > prev;
        prev = value;
        ++entries;
    }
    // The final AUDIOHHD entry is the end offset of the archive. V6
    // accepted windows shifted into preceding zero padding because it only
    // required the final offset to exceed half the archive length.
    const uint32_t last = u32(image.data() + at + 4 * (count - 1));
    return u32(image.data() + at) == 0 && entries > 8 &&
           differences > 5 && last != 0xffffffffu &&
           last <= audioSize && audioSize - last <= 4;
}

bool mapEntryShape(const std::vector<uint8_t> &data, uint32_t at)
{
    if (at < 8 || at > data.size() || data.size() - at < 38) return false;
    const auto *h = data.data() + at;
    const unsigned w = u16(h + 18), hgt = u16(h + 20);
    if (!w || !hgt || w > 64 || hgt > 64) return false;
    // CA_CacheMap() in ReflectionHLE skips ANY plane whose compressed
    // length is zero. Requiring plane 2 (or plane 1) rejects valid maps.
    unsigned activePlanes = 0;
    for (unsigned plane = 0; plane < 3; ++plane)
    {
        const uint32_t start = u32(h + 4 * plane);
        const uint16_t length = u16(h + 12 + 2 * plane);
        if (!length) continue;
        if (length < 2 || start < 8 || start >= data.size() ||
            length > data.size() - start)
            return false;
        ++activePlanes;
    }
    return activePlanes != 0;
}

bool mapHeadShape(const std::vector<uint8_t> &image, size_t at,
                  const std::vector<uint8_t> &maps, int game)
{
    if (at > image.size() || image.size() - at < 834 || maps.size() < 8)
        return false;
    const uint16_t tag = u16(image.data() + at);
    if (!tag || tag == 0xffff) return false;
    // mapfiletype reserves 100 slots; the actual engines load only
    // 19 maps in Abyss and 39 in Armageddon/Apocalypse (id_ca.h).
    // Unused slots outside that range need not represent level headers.
    const unsigned gameMaps = (game == 1) ? 19u : 39u;
    unsigned validHeaders = 0;
    for (unsigned i = 0; i < gameMaps; ++i)
    {
        const uint32_t pos = u32(image.data() + at + 2 + i * 4);
        if (pos == 0xffffffffu || pos == 0) continue;
        if (!mapEntryShape(maps, pos)) return false;
        ++validHeaders;
    }
    return validHeaders >= 2;
}

/* Unlike the original isHuffmanTree(), this accepts unused/padded nodes.
 * It is not sufficient alone: real AUDIO chunks must also decompress. */
bool audioDictShape(const uint8_t *data)
{
    unsigned leaves = 0, branches = 0;
    for (unsigned n = 0; n < 255; ++n)
    {
        for (unsigned side = 0; side < 2; ++side)
        {
            const unsigned v = u16(data + n * 4 + side * 2);
            if (v >= 510 || (v >= 256 && v - 256 >= n)) return false;
            leaves += v < 256;
            branches += v >= 256;
        }
    }
    return leaves > 64 && branches > 32;
}

// The original loader expects a complete Huffman-compressed chunk. V6 could
// accept a wrong dictionary after it produced the requested number of bytes
// from only a small prefix of the compressed data.
bool decodeAudioChunk(const uint8_t *data, size_t size,
                      const uint8_t *dictionary, std::vector<uint8_t> &decoded)
{
    if (size <= 4 || size > 300000) return false;
    const size_t expanded = u32(data);
    if (!expanded || expanded > 128000) return false;
    size_t consumed = 0;
    if (!expandHuffman(data + 4, size - 4, dictionary, expanded,
                       decoded, &consumed)) return false;
    // A partial final byte and up to one alignment byte may remain.
    return consumed <= (size - 4) * 8 && consumed + 16 >= (size - 4) * 8;
}

unsigned audioChunksDecoded(const std::vector<uint8_t> &image,
                            size_t head, size_t count,
                            const std::vector<uint8_t> &audio,
                            const uint8_t *dict)
{
    unsigned passed = 0;
    std::vector<uint8_t> result;
    for (size_t i = 0; i + 1 < count && passed < 3; ++i)
    {
        const uint32_t start = u32(image.data() + head + 4 * i);
        const uint32_t end = u32(image.data() + head + 4 * (i + 1));
        if (start == 0xffffffffu || end == 0xffffffffu ||
            end <= start + 4 || end > audio.size()) continue;
        if (decodeAudioChunk(audio.data() + start, end - start, dict, result))
            ++passed;
    }
    return passed;
}

// Validate compressed sounds according to the actual Catacomb format.
// The first NUMSOUNDS chunks are PCSound (6-byte header + data), and the
// next NUMSOUNDS are AdLibSound (23-byte header + data). Both store a
// 32-bit data length at byte 0. This rejects bogus Huffman dictionaries
// which merely produce the requested output byte count.
bool audioSoundStructureValid(const std::vector<uint8_t> &image,
                              size_t head, size_t count,
                              const std::vector<uint8_t> &audio,
                              const uint8_t *dict, int game)
{
    static const unsigned sounds[] = {0, 34, 35, 37};
    const unsigned n = sounds[game];
    if (2u * n >= count) return false;
    // Some valid editions have an empty first sound in a bank. Do not
    // require fixed sound indices to be populated: verify at least two
    // actual PC and AdLib entries below, including their decoded headers.
    std::vector<uint8_t> decoded;
    for (unsigned group = 0; group < 2; ++group)
    {
        const unsigned first = group * n;
        const size_t overhead = group ? 23u : 6u;
        unsigned verified = 0;
        for (unsigned i = 0; i < n; ++i)
        {
            const unsigned chunk = first + i;
            const uint32_t start = u32(image.data() + head + 4u * chunk);
            const uint32_t end = u32(image.data() + head + 4u * (chunk + 1));
            if (start == 0xffffffffu || end == 0xffffffffu ||
                end <= start + 4 || end > audio.size()) continue;
            if (!decodeAudioChunk(audio.data() + start, end - start,
                                  dict, decoded)) return false;
            if (decoded.size() <= overhead) return false;
            if (u32(decoded.data()) != decoded.size() - overhead) return false;
            ++verified;
        }
        if (verified < 2) return false;
    }
    return true;
}

// Number of internal Huffman nodes reachable from the actual root.
// False positives can start two bytes inside a real table; their apparent
// root is only a subtree. Prefer the structurally complete tree, not the
// offset closest to a guessed DOS address.
unsigned audioTreeReachableNodes(const uint8_t *dict)
{
    bool seen[255] = {}, queued[255] = {};
    unsigned stack[255], count = 0, pending = 1;
    stack[0] = 254;
    queued[254] = true;
    while (pending)
    {
        const unsigned node = stack[--pending];
        if (seen[node]) continue;
        seen[node] = true;
        ++count;
        for (unsigned side = 0; side < 2; ++side)
        {
            const unsigned v = u16(dict + 4 * node + 2 * side);
            if (v < 256) continue;
            const unsigned next = v - 256;
            if (next >= 255 || next >= node) return 0;
            if (!queued[next])
            {
                queued[next] = true;
                if (pending >= 255) return 0;
                stack[pending++] = next;
            }
        }
    }
    return count;
}

// Fingerprint every non-empty archive chunk by its *decoded* contents.
// Different embedded Huffman tables may be byte-different yet decode exactly
// the same AUDIO archive; treating them as different would be a false alarm.
// A complete decode also rejects candidates that work for only two samples.
bool audioArchiveFingerprint(const std::vector<uint8_t> &image,
                             size_t head, size_t count,
                             const std::vector<uint8_t> &audio,
                             const uint8_t *dict, uint64_t &fingerprint)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    const auto mix = [&hash](uint8_t byte) {
        hash = (hash ^ byte) * UINT64_C(1099511628211);
    };
    std::vector<uint8_t> decoded;
    unsigned chunks = 0;
    for (size_t i = 0; i + 1 < count; ++i)
    {
        const uint32_t start = u32(image.data() + head + 4 * i);
        const uint32_t end = u32(image.data() + head + 4 * (i + 1));
        if (start == 0xffffffffu || end == 0xffffffffu) continue;
        if (start == end) continue;  // Empty slot.
        if (end <= start + 4 || end > audio.size()) return false;
        if (!decodeAudioChunk(audio.data() + start, end - start,
                              dict, decoded)) return false;
        const uint32_t expanded = uint32_t(decoded.size());
        // Include the original chunk index and decoded length in the hash.
        for (unsigned j = 0; j < 4; ++j)
        {
            mix(uint8_t(i >> (j * 8)));
            mix(uint8_t(expanded >> (j * 8)));
        }
        for (const uint8_t byte : decoded) mix(byte);
        ++chunks;
    }
    fingerprint = hash;
    return chunks >= 6;
}

bool locateEmbeddedTables(const std::vector<uint8_t> &image,
                          size_t egaDict, size_t egaHead,
                          const char *folder, int game,
                          DecodedResources &result,
                          char *reason, size_t reasonSize)
{
    std::vector<uint8_t> audio, maps;
    if (!openArchive(folder, kExtensions[game], "AUDIO", audio) ||
        !openArchive(folder, kExtensions[game], "GAMEMAPS", maps))
    {
        setMessage(reason, reasonSize, "Cannot read AUDIO/GAMEMAPS");
        return false;
    }
    static const int audioHeadDelta[] = {0, -0x1a0, -0x1b0, -0x1d0};
    // Verified against be_gamedefs_catabyss.h (both v1.13/v1.24):
    // MTEMP - EGAHEAD = 0x1ab10 - 0x1a3b0 = -0x8a0.
    static const int mapHeadDelta[]   = {0, -0x8a0, -0x500, -0x520};
    static const size_t audioHeadSize[] = {0, 416, 428, 452};
    const size_t count = audioHeadSize[game] / 4;
    const int64_t referenceAudio = int64_t(egaHead) + audioHeadDelta[game];
    const int64_t referenceMap = int64_t(egaHead) + mapHeadDelta[game];

    // Strong evidence first: when the AUDIO archive is byte-identical to a
    // supported release, the exact embedded tables are authoritative. Do not
    // reject them using heuristics intended for unknown editions.
    static const uint32_t knownArchiveCrc[] = {
        0u, 0x462d2eb2u, 0xb6c28bc7u, 0x23934cecu
    };
    static const uint32_t knownDictCrc[] = {
        0u, 0xe9088011u, 0x8f1d4dd2u, 0x26658498u
    };
    static const uint32_t knownHeadCrc[] = {
        0u, 0xfbfff495u, 0x5f863ad2u, 0x76adb051u
    };
    const bool knownArchive =
        crcBuffer(audio.data(), audio.size()) == knownArchiveCrc[game];
    const auto knownHead = [&](size_t at) {
        return at <= image.size() &&
            audioHeadSize[game] <= image.size() - at &&
            crcBuffer(image.data() + at, audioHeadSize[game]) == knownHeadCrc[game];
    };
    const auto knownDict = [&](size_t at) {
        return at <= image.size() && 1024 <= image.size() - at &&
            crcBuffer(image.data() + at, 1024) == knownDictCrc[game];
    };
    const auto viableHead = [&](size_t at) {
        return audioHeadShape(image, at, count, audio.size()) ||
               (knownArchive && knownHead(at));
    };
    std::vector<size_t> audioHeads, mapHeads, dicts;
    const auto addUnique = [](std::vector<size_t> &items, size_t at) {
        for (const size_t old : items) if (old == at) return;
        items.push_back(at);
    };
    if (referenceAudio >= 0 && viableHead(size_t(referenceAudio)))
        addUnique(audioHeads, size_t(referenceAudio));

    // The map table is independent of the AUDIO tables. Preserve the
    // previously verified map selection and never relax map integrity.
    static const uint32_t knownMapCrc[] = {
        0u, 0x5d9ccfb3u, 0x546f00d1u, 0x90742162u
    };
    if (referenceMap >= 0 && size_t(referenceMap) <= image.size() &&
        image.size() - size_t(referenceMap) >= 834 &&
        crcBuffer(image.data() + size_t(referenceMap), 834) == knownMapCrc[game])
        mapHeads.push_back(size_t(referenceMap));
    else if (referenceMap >= 0 && mapHeadShape(image, size_t(referenceMap), maps, game))
        mapHeads.push_back(size_t(referenceMap));
    if (mapHeads.empty())
        for (size_t at = 0; at + 834 <= image.size(); at += 2)
            if (mapHeadShape(image, at, maps, game))
                mapHeads.push_back(at);
    if (mapHeads.empty())
    {
        setMessage(reason, reasonSize, "MTEMP: no valid level header table found");
        return false;
    }
    if (mapHeads.size() != 1)
    {
        setMessage(reason, reasonSize, "MTEMP: ambiguous level header tables");
        return false;
    }

    const size_t expectedDict = egaDict >= 1024 ? egaDict - 1024 : 0;
    if (egaDict >= 1024 &&
        (knownDict(expectedDict) ||
         (audioDictShape(image.data() + expectedDict) &&
          audioTreeReachableNodes(image.data() + expectedDict) >= 16)))
        addUnique(dicts, expectedDict);

    // Try known-relative addresses first. Unlike V6, failure of a plausible
    // candidate does not suppress a full search for other valid candidates.
    struct Pair {
        size_t dict, head;
        bool canonical;
        uint64_t fingerprint;
        unsigned treeNodes;
    };
    struct CheckCounts {
        unsigned decoded = 0, sounds = 0, full = 0, known = 0;
    } checks;
    std::vector<Pair> pairs;
    const auto validatePairs = [&]() {
        for (const size_t dict : dicts)
        {
            const bool exactDict = knownDict(dict);
            const unsigned treeNodes = audioTreeReachableNodes(image.data() + dict);
            if (!treeNodes && !exactDict) continue;
            for (const size_t head : audioHeads)
            {
                const bool canonical = knownArchive && exactDict && knownHead(head);
                // Only treat an exact, published dictionary+header+archive
                // triple as authoritative. No heuristic sound test needed.
                if (canonical)
                {
                    ++checks.known;
                    pairs.push_back({dict, head, true, 0, treeNodes});
                    continue;
                }
                if (audioChunksDecoded(image, head, count, audio,
                                       image.data() + dict) < 2) continue;
                ++checks.decoded;
                if (!audioSoundStructureValid(image, head, count, audio,
                                               image.data() + dict, game)) continue;
                ++checks.sounds;
                uint64_t fingerprint = 0;
                if (!audioArchiveFingerprint(image, head, count, audio,
                                             image.data() + dict, fingerprint)) continue;
                ++checks.full;
                pairs.push_back({dict, head, false, fingerprint, treeNodes});
            }
        }
    };
    validatePairs();
    if (pairs.empty())
    {
        // Full search is a fallback, not a first-launch requirement. This
        // fixes V6's early commitment to the first plausible relative offset.
        for (size_t at = 0; at + audioHeadSize[game] <= image.size(); at += 2)
            if (viableHead(at)) addUnique(audioHeads, at);
        for (size_t at = 0; at + 1024 <= image.size(); at += 2)
            if (audioDictShape(image.data() + at) &&
                audioTreeReachableNodes(image.data() + at) >= 16)
                addUnique(dicts, at);
        checks = {};
        validatePairs();
    }
    if (pairs.empty())
    {
        char detail[128];
        snprintf(detail, sizeof(detail),
                 "AUDIO: H=%u D=%u DEC=%u SND=%u FULL=%u",
                 unsigned(audioHeads.size()), unsigned(dicts.size()),
                 checks.decoded, checks.sounds, checks.full);
        setMessage(reason, reasonSize, detail);
        return false;
    }

    // Several shifted copies can be equivalent. Canonical data wins; for
    // other editions require exactly one distinct fully decoded stream.
    const size_t expectedHead = referenceAudio >= 0 ? size_t(referenceAudio) : 0;
    const auto distance = [=](const Pair &p) -> size_t {
        return (p.dict > expectedDict ? p.dict - expectedDict : expectedDict - p.dict) +
               (p.head > expectedHead ? p.head - expectedHead : expectedHead - p.head);
    };
    std::vector<Pair> unique;
    for (const Pair &candidate : pairs)
    {
        if (!candidate.canonical) continue;
        bool duplicate = false;
        for (Pair &other : unique)
        {
            if (!memcmp(image.data() + other.dict, image.data() + candidate.dict, 1024) &&
                !memcmp(image.data() + other.head, image.data() + candidate.head,
                        audioHeadSize[game]))
            {
                if (distance(candidate) < distance(other)) other = candidate;
                duplicate = true;
                break;
            }
        }
        if (!duplicate) unique.push_back(candidate);
    }
    if (unique.empty())
    {
        unsigned mostNodes = 0;
        for (const Pair &candidate : pairs)
            if (candidate.treeNodes > mostNodes) mostNodes = candidate.treeNodes;
        for (const Pair &candidate : pairs)
        {
            if (candidate.treeNodes != mostNodes) continue;
            bool duplicate = false;
            for (Pair &other : unique)
            {
                if (other.fingerprint == candidate.fingerprint)
                {
                    if (distance(candidate) < distance(other)) other = candidate;
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) unique.push_back(candidate);
        }
    }
    if (unique.size() != 1)
    {
        char detail[128];
        snprintf(detail, sizeof(detail),
                 "AUDIO: %u different validated archives", unsigned(unique.size()));
        setMessage(reason, reasonSize, detail);
        return false;
    }
    const size_t selectedAudioHead = unique.front().head;
    const size_t selectedDict = unique.front().dict;
    result.audioDictOffset = unsigned(selectedDict);
    result.audioHeadOffset = unsigned(selectedAudioHead);
    result.mapHeadOffset = unsigned(mapHeads.front());
    result.audioDictCrc = crcBuffer(image.data() + selectedDict, 1024);
    result.audioHeadCrc = crcBuffer(image.data() + selectedAudioHead, audioHeadSize[game]);
    result.egaDictCrc = crcBuffer(image.data() + egaDict, 1024);
    result.egaHeadCrc = crcBuffer(image.data() + egaHead, (kChunks[game] + 1) * 3u);
    result.mapHeadCrc = crcBuffer(image.data() + mapHeads.front(), 834);
    setMessage(reason, reasonSize, "EGA and 5 embedded tables structurally verified");
    return true;
}

bool loadResources(int game, const char *folder, DecodedResources &res,
                   char *problem, size_t cap)
{
    if (game < 1 || game > 3)
    {
        setMessage(problem, cap, "Game handled by Catacomb 3-D loader");
        return false;
    }
    char graphicsRequired[24];
    snprintf(graphicsRequired, sizeof(graphicsRequired), "EGAGRAPH.%s", kExtensions[game]);
    if (!findFile(folder, graphicsRequired, res.graphicsName))
    {
        setMessage(problem, cap, "EGAGRAPH file not found");
        return false;
    }
    char graphicsPath[320];
    if (!pathFor(graphicsPath, sizeof(graphicsPath), folder, res.graphicsName))
    {
        setMessage(problem, cap, "Graphic filename too long");
        return false;
    }
    FILE *graphics = fopen(graphicsPath, "rb");
    if (!graphics)
    {
        setMessage(problem, cap, "Cannot open EGAGRAPH file");
        return false;
    }
    const long graphLength = fseek(graphics, 0, SEEK_END) == 0 ? ftell(graphics) : -1;
    if (graphLength < 64 || graphLength > 2000000)
    {
        fclose(graphics);
        setMessage(problem, cap, "Invalid EGAGRAPH length");
        return false;
    }
    DIR *dir = opendir(folder);
    if (!dir)
    {
        fclose(graphics);
        setMessage(problem, cap, "Cannot list game folder");
        return false;
    }
    bool foundCandidate = false;
    struct dirent *entry;
    while ((entry = readdir(dir)))
    {
        const char *name = entry->d_name;
        if (!hasExtension(name, "EXE") || strlen(name) >= 128 || name[0] == '.')
            continue;
        char path[320];
        if (!pathFor(path, sizeof(path), folder, name)) continue;
        std::vector<uint8_t> exeFile, image;
        if (!readFile(path, exeFile, 600000) || exeFile.size() < 45000 ||
            !unpackGameExe(exeFile, image))
            continue;
        foundCandidate = true;
        std::vector<uint32_t> offsets;
        size_t headOff = 0;
        if (!findHead(image, uint32_t(graphLength), kChunks[game], headOff, offsets))
            continue;
        // Take the tree that can decode the picture table and the status
        // graphic. The other matching 255-node tree is usually for audio.
        for (size_t off = 0; off + 1024 <= image.size(); off += 2)
        {
            const uint8_t *dict = image.data() + off;
            if (!isHuffmanTree(dict)) continue;
            std::vector<uint8_t> pics, status;
            if (!loadChunk(graphics, offsets, 0, dict, pics) ||
                pics.size() != kPics[game] * 4u)
                continue;
            bool dimensionsValid = true;
            for (unsigned i = 0; i < kPics[game]; ++i)
            {
                const unsigned w = u16(pics.data() + i * 4);
                const unsigned h = u16(pics.data() + i * 4 + 2);
                if (!w || w > 40 || !h || h > 200)
                {
                    dimensionsValid = false;
                    break;
                }
            }
            if (!dimensionsValid ||
                !loadChunk(graphics, offsets, 5, dict, status) ||
                status.size() != size_t(u16(pics.data() + 4)) *
                                 u16(pics.data() + 6) * 4u)
                continue;
            // Validate all five runtime tables, including the graphics pair.
            // Different DOS editions need not preserve their relative offsets.
            char tableProblem[128] = {};
            res.embeddedLayoutValid = locateEmbeddedTables(image, off, headOff,
                 folder, game, res, tableProblem, sizeof(tableProblem));
            setMessage(problem, cap, tableProblem);
            res.imageSize = unsigned(image.size());
            res.headOff = unsigned(headOff);
            res.dictOff = unsigned(off);
            snprintf(res.exeName, sizeof(res.exeName), "%s", name);
            res.graphicsName[sizeof(res.graphicsName) - 1] = 0;
            res.offsets.swap(offsets);
            res.pictable.swap(pics);
            res.statusGraphic.swap(status);
            closedir(dir);
            fclose(graphics);
            if (res.embeddedLayoutValid)
                setMessage(problem, cap, "Graphics and EXE tables verified");
            return true;
        }
    }
    closedir(dir);
    fclose(graphics);
    setMessage(problem, cap, foundCandidate ?
               "EGA tables/pictures not compatible" :
               "No compatible LZEXE game EXE found");
    return false;
}

} // namespace

void cat3dsProbeResources(int game, const char *folder, Cat3DSResourceProbe *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    DecodedResources resources = {};
    out->graphicsValid = loadResources(game, folder, resources,
                                      out->problem, sizeof(out->problem));
    if (!out->graphicsValid) return;
    snprintf(out->exeName, sizeof(out->exeName), "%s", resources.exeName);
    snprintf(out->graphicsName, sizeof(out->graphicsName), "%s", resources.graphicsName);
    out->imageSize = resources.imageSize;
    out->dictOffset = resources.dictOff;
    out->headOffset = resources.headOff;
    out->embeddedLayoutValid = resources.embeddedLayoutValid;
    out->audioDictOffset = resources.audioDictOffset;
    out->audioHeadOffset = resources.audioHeadOffset;
    out->mapHeadOffset = resources.mapHeadOffset;
    out->audioDictCrc = resources.audioDictCrc;
    out->audioHeadCrc = resources.audioHeadCrc;
    out->egaDictCrc = resources.egaDictCrc;
    out->egaHeadCrc = resources.egaHeadCrc;
    out->mapHeadCrc = resources.mapHeadCrc;
    char name[32], found[128];
    snprintf(name, sizeof(name), "AUDIO.%s", kExtensions[game]);
    const bool audio = findFile(folder, name, found);
    snprintf(name, sizeof(name), "GAMEMAPS.%s", kExtensions[game]);
    out->audioAndMapsPresent = audio && findFile(folder, name, found);
    if (!out->audioAndMapsPresent)
        setMessage(out->problem, sizeof(out->problem),
                   "Graphics OK, but AUDIO or GAMEMAPS missing");
}

bool cat3dsShowOriginalStatusGraphic(int game, const char *folder,
                                    char *error, size_t errorLength)
{
    DecodedResources resources = {};
    if (!loadResources(game, folder, resources, error, errorLength))
        return false;
    const unsigned width = u16(resources.pictable.data() + 4);
    const unsigned height = u16(resources.pictable.data() + 6);
    if (!width || width > 40 || !height || height > 200 ||
        resources.statusGraphic.size() != size_t(width) * height * 4u)
    {
        setMessage(error, errorLength, "STATUSPIC has invalid dimensions");
        return false;
    }
#ifdef __3DS__
    BE_ST_SetScreenMode(0xD);
    BE_ST_SetScreenStartAddress(0);
    BE_ST_EGASetLineWidth(40);
    BE_ST_EGASetPelPanning(0);
    BE_ST_EGASetSplitScreen(-2);
    BE_ST_EGAUpdateGFXBufferFrom4bitsPixel(0, 0, 40 * 200);
    const unsigned x = (40 - width) / 2;
    const unsigned y = (200 - height) / 2;
    const size_t planeSize = width * height;
    for (unsigned plane = 0; plane < 4; ++plane)
        for (unsigned row = 0; row < height; ++row)
            BE_ST_EGAUpdateGFXBufferInPlane(uint16_t((y + row) * 40 + x),
                    resources.statusGraphic.data() + plane * planeSize + row * width,
                    uint16_t(width), uint16_t(plane));
#endif
    return true;
}
