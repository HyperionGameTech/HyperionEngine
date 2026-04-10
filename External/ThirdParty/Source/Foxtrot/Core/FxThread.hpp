#pragma once

#include "FxDefines.hpp"

#ifdef FX_PLATFORM_MACOS
#include <pthread.h>
#endif

class FxThread
{
public:
    void Create();
    static void Join(FxThread &thread);
    void Destroy();

    ~FxThread()
    {
        Destroy();
    }
#ifdef FX_PLATFORM_MACOS
    operator pthread_t() const { return mThread; }
    operator pthread_t *() { return &mThread; }

#endif

private:
#ifdef FX_PLATFORM_MACOS
    pthread_t mThread;
#endif
};
