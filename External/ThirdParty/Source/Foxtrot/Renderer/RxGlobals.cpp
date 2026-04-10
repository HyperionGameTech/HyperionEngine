#include "RxGlobals.hpp"

#include "Backend/RxDescriptorCache.hpp"
#include "RxRenderBackend.hpp"
#include "RxShaderCache.hpp"
#include "RxShadowDirectional.hpp"
#include "RxState.hpp"

RxRenderBackend* gRenderer = nullptr;
RxShadowDirectional* gShadowRenderer = nullptr;
RxShaderCache* gShaderCache = nullptr;
RxDescriptorCache* gDescriptorCache = nullptr;
RxState* gState = nullptr;

#define DESTROY_GLOBAL(name_)                                                                                          \
    delete name_;                                                                                                      \
    name_ = nullptr

namespace RxGlobals {

void Init()
{
    gRenderer = new RxRenderBackend;
    gShaderCache = new RxShaderCache;
    gDescriptorCache = new RxDescriptorCache;
    gState = new RxState;
}

void Destroy()
{
    if (gShadowRenderer) {
        DESTROY_GLOBAL(gShadowRenderer);
    }

    DESTROY_GLOBAL(gState);
    DESTROY_GLOBAL(gDescriptorCache);
    DESTROY_GLOBAL(gShaderCache);
    DESTROY_GLOBAL(gRenderer);
}

}; // namespace RxGlobals
