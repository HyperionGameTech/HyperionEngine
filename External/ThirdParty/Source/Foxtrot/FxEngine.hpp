#pragma once

class PhJolt;
class AxManager;
class FxShaderCompiler;
class FxObjectManager;
class FxMaterialManager;
class FxMemPool;

extern PhJolt* gPhysics;
extern AxManager* gAssetManager;
extern FxObjectManager* gObjectManager;
extern FxShaderCompiler* gShaderCompiler;
extern FxMaterialManager* gMaterialManager;
extern FxMemPool* gEnginePool;

namespace FxGlobals {
void Init();
void Destroy();
} // namespace FxGlobals
