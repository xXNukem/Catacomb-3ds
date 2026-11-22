// Run the production SD scanner in a private directory with a small test manifest.
#include "../platform/3ds/source/cat3ds_game_files.cpp"
#include <cassert>
#include <unistd.h>

void cat3dsProbeResources(int, const char*, Cat3DSResourceProbe*) {}

int main(int argc,char **argv)
{
    assert(argc==2 && !chdir(argv[1]) && !mkdir("sdmc:",0700));
    assert(cat3dsEnsureGameFolders(nullptr));
    Cat3DSResourceProbe abyss={};
    abyss.embeddedLayoutValid=true;
    strcpy(abyss.exeName,"ANYNAME.EXE");
    abyss.egaDictCrc=0x63eb06d3u;
    assert(cat3dsAbyssEdition(abyss)==124);
    abyss.egaHeadCrc=0xe31e1c3bu;
    assert(!cat3dsAbyssEdition(abyss));
    abyss.egaDictCrc=0xbb760f1du;
    assert(cat3dsAbyssEdition(abyss)==113);
    // The registered V1.23 files previously used the 113 engine via their
    // filename. Exercise selection with the measured identities and an alias.
    Cat3DSResourceProbe earlier = {};
    earlier.embeddedLayoutValid = true;
    strcpy(earlier.exeName, "RENAMED.EXE");
    earlier.graphicsSize = 335969;
    earlier.imageSize = 194592;
    earlier.graphicsCrc = 0x8c8d9023;
    earlier.audioArchiveCrc = 0x462d2eb2;
    earlier.audioSize = 6904;
    earlier.mapsSize = 20433;
    earlier.mapsArchiveCrc = 0x97ddb354;
    earlier.audioDictCrc = 0xe9088011;
    earlier.audioHeadCrc = 0xfbfff495;
    earlier.egaDictCrc = 0xe130eb8a;
    earlier.egaHeadCrc = 0x0486de41;
    earlier.mapHeadCrc = 0x5d9ccfb3;
    assert(cat3dsAbyssEdition(earlier) == 113);
    earlier.mapsArchiveCrc ^= 1;
    assert(!cat3dsAbyssEdition(earlier));
    const char *difference = nullptr;
    assert(!cat3dsEarlierRelease(1, earlier, &difference) && !strcmp(difference, "GAMEMAPS CRC"));
    for (int game=2; game<=3; ++game) {
        const char *ext=game==2 ? "ARM" : "APC";
        char names[3][16];
        Cat3DSRequiredFile files[4]={};
        const char *archive[]={"AUDIO","EGAGRAPH","GAMEMAPS"};
        for (int i=0;i<3;++i) {
            snprintf(names[i],sizeof names[i],"%s.%s",archive[i],ext);
            files[i]={names[i],1,0xd3d99e8bu}; // byte 'A'
        }
        files[3]={game==2 ? "ARMGAME.EXE" : "APOCGAME.EXE",1,0xd3d99e8bu};
        Cat3DSGameVersion version={game,"test",files,4};
        auto write=[&](const char *name,int byte) {
            char path[256];
            snprintf(path,sizeof path,"%s/%s",cat3dsGameFolder(game),name);
            FILE *f=fopen(path,"wb"); assert(f && fputc(byte,f)==byte && !fclose(f));
        };
        for (const auto &name : names) write(name,'A');
        write("RENAMED.EXE",'A');
        auto s=scanVersion(game,&version,1);
        assert(s.coreValidCount==4 && !strcmp(s.matchedExeName,"RENAMED.EXE"));
        write("RENAMED.EXE",'B');
        s=scanVersion(game,&version,1);
        assert(s.coreValidCount==3 && !s.matchedExeName[0]);
        Cat3DSResourceProbe probe={};
        strcpy(probe.exeName,"RENAMED.EXE"); probe.embeddedLayoutValid=true;
        probe.imageSize=game==2 ? 198304-0x2000 : 200064-0x2200;
        probe.audioDictCrc=game==2 ? 0x8f1d4dd2u : 0x26658498u;
        probe.audioHeadCrc=game==2 ? 0x5f863ad2u : 0x76adb051u;
        probe.egaDictCrc=game==2 ? 0xab662db8u : 0xb2ed57fdu;
        probe.egaHeadCrc=game==2 ? 0x711cbf10u : 0xd7548ed8u;
        probe.mapHeadCrc=game==2 ? 0x546f00d1u : 0x90742162u;
        assert(matchesGameTables(game,s,probe));
        assert(!matchesGameTables(5-game,s,probe));
        probe.audioDictCrc^=1;
        assert(!matchesGameTables(game,s,probe));
        probe.audioDictCrc^=1;
        write(names[0],'B');
        assert(!matchesGameTables(game,scanVersion(game,&version,1),probe));
    }
}
