#include "../platform/3ds/source/cat3ds_resource_probe.cpp"
#include <cassert>

int main()
{
    std::vector<uint8_t> exe(32+190112), image;
    auto word=[&](int offset,unsigned value) {
        exe[offset]=uint8_t(value); exe[offset+1]=uint8_t(value>>8);
    };
    word(0,0x5a4d);
    word(2,exe.size()%512);
    word(4,(exe.size()+511)/512);
    word(8,2);
    word(24,28);
    exe[32]=42;
    bool plain=false;
    assert(unpackGameExe(exe,image,plain) && plain && image.size()==190112 && image[0]==42);
    word(10,0xffff);
    assert(!unpackGameExe(exe,image,plain));
    word(10,0); word(6,100);
    assert(!unpackGameExe(exe,image,plain));
    word(6,0); exe.pop_back();
    assert(!unpackGameExe(exe,image,plain));
}
