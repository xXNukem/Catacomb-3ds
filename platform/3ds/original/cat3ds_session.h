#ifndef CAT3DS_SESSION_H
#define CAT3DS_SESSION_H
#include "cat3ds_menu.h"
#include <cstdlib>
#include <cstring>

// run() must unwind the game and shut down the backend before returning.
// These ranges contain only the original engines' and backend's static storage.
template<class Run, class RestoreDirectory>
int Cat3DS_RunSessions(unsigned char *data, size_t dataSize, unsigned char *zero, size_t zeroSize,
                       Run run, RestoreDirectory restoreDirectory)
{
    void *initial = std::malloc(dataSize);
    if (!initial) return 1;
    std::memcpy(initial, data, dataSize);
    int status;
    for (;;) {
        cat3dsSessionAction = CatResume;
        status = run();
        if (status || cat3dsSessionAction != CatSwitch) break;
        if (!restoreDirectory()) { status = 1; break; }
        std::memcpy(data, initial, dataSize);
        std::memset(zero, 0, zeroSize);
    }
    std::free(initial);
    return status;
}
#endif
