// Optional integration test. Pass the three earlier DOS installation directories
// in episode order. Only temporary links and derived test images are written.
#include "../platform/3ds/source/cat3ds_resource_probe.cpp"
#include "../platform/3ds/source/cat3ds_game_files.cpp"
#include <cassert>
#include <string>
#include <unistd.h>

static void writeBytes(const std::string &path, const std::vector<uint8_t> &bytes)
{
    FILE *f = fopen(path.c_str(), "wb");
    assert(f && fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size() && !fclose(f));
}

int main(int argc, char **argv)
{
    assert(argc == 5 && !chdir(argv[1]) && !mkdir("sdmc:", 0700));
    assert(cat3dsEnsureGameFolders(nullptr));
    const char *const gameplay[] = {"CATABYSS.EXE", "CATARM.EXE", "CATAPOC.EXE"};
    for (int game = 1; game <= 3; ++game) {
        const std::string original = argv[game+1], folder = cat3dsGameFolder(game);
        const auto alternate = folder + "/variant.exe";
        for (const char *stem : {"AUDIO", "EGAGRAPH", "GAMEMAPS"}) {
            const std::string name = std::string(stem) + "." + kExtensions[game];
            assert(!symlink((original + "/" + name).c_str(), (folder + "/" + name).c_str()));
        }
        const auto packed = original + "/" + gameplay[game-1];
        assert(!symlink(packed.c_str(), alternate.c_str()));
        Cat3DSInstallStatus states[4] = {};
        cat3dsScanInstallations(states);
        const auto &s = states[game];
        assert(s.detectedEdition && s.resourceLayoutValid && !strcmp(s.resourceExeName, "variant.exe"));
        assert(game == 1 || cat3dsCanLaunchArmageddon(s));
        Cat3DSResourceProbe probe = {};
        cat3dsProbeResources(game, folder.c_str(), &probe);
        assert(!probe.plainMZ && cat3dsEarlierRelease(game, probe));
        assert(game != 1 || cat3dsAbyssEdition(probe) == 113);

        // Same measured load image in a synthetic MZ wrapper.
        std::vector<uint8_t> file, image;
        bool plain;
        assert(readFile(packed.c_str(), file, 600000) && unpackGameExe(file, image, plain));
        const unsigned musicIndex[] = {0,102,105,111};
        const auto *offsets = image.data()+probe.audioHeadOffset+4*musicIndex[game];
        std::vector<uint8_t> archive, track;
        const auto audioPath = original + "/AUDIO." + kExtensions[game];
        assert(readFile(audioPath.c_str(),archive,300000));
        const unsigned first=u32(offsets), end=u32(offsets+4);
        assert(first<end && end<=archive.size());
        assert(decodeAudioChunk(archive.data()+first,end-first,
                                image.data()+probe.audioDictOffset,track));
        assert(track.size()>2 && u16(track.data())==track.size()-2 && !(u16(track.data())%4));
        printf("TOOHOT_MUS: %zu bytes, CRC %08x\n",track.size(),crcBuffer(track.data(),track.size()));
        std::vector<uint8_t> mz(32);
        mz.insert(mz.end(), image.begin(), image.end());
        const auto word = [&](size_t at, size_t v) { mz[at] = uint8_t(v); mz[at+1] = uint8_t(v >> 8); };
        word(0, 0x5a4d); word(2, mz.size()%512); word(4, (mz.size()+511)/512);
        word(8, 2); word(24, 28);
        assert(!unlink(alternate.c_str())); // Remove only the temporary link.
        writeBytes(alternate, mz);
        cat3dsProbeResources(game, folder.c_str(), &probe);
        assert(probe.plainMZ && cat3dsEarlierRelease(game, probe));

        mz[32 + probe.mapHeadOffset + 833] ^= 1;
        writeBytes(alternate, mz);
        cat3dsProbeResources(game, folder.c_str(), &probe);
        assert(!cat3dsEarlierRelease(game, probe));
        assert(game != 1 || !cat3dsAbyssEdition(probe));
        mz.pop_back();
        writeBytes(alternate, mz);
        cat3dsProbeResources(game, folder.c_str(), &probe);
        assert(!probe.graphicsValid && strstr(probe.problem, "truncated MZ"));
        file.pop_back();
        writeBytes(alternate, file);
        cat3dsProbeResources(game, folder.c_str(), &probe);
        assert(!probe.graphicsValid && strstr(probe.problem, "truncated MZ"));
        printf("%s: genuine LZEXE/archives, alias, synthetic MZ, altered MTEMP and truncation PASS\n", gameplay[game-1]);
    }
}
