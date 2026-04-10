#pragma once

class RxShadowDirectional;
class RxRenderBackend;
class RxShaderCache;
class RxDescriptorCache;
class RxState;

extern RxRenderBackend* gRenderer;
extern RxShadowDirectional* gShadowRenderer;
extern RxShaderCache* gShaderCache;
extern RxDescriptorCache* gDescriptorCache;
extern RxState* gState;

namespace RxGlobals {
void Init();
void Destroy();
}; // namespace RxGlobals
