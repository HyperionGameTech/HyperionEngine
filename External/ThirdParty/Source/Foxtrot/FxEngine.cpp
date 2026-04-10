#include "FxEngine.hpp"

#include <Asset/AxManager.hpp>
#include <Asset/FxShaderCompiler.hpp>
#include <Core/MemPool/FxMemPool.hpp>
#include <FxMaterial.hpp>
#include <FxObjectManager.hpp>
#include <Physics/PhJolt.hpp>

PhJolt* gPhysics = nullptr;
FxShaderCompiler* gShaderCompiler = nullptr;
AxManager* gAssetManager = nullptr;
FxObjectManager* gObjectManager = nullptr;
FxMaterialManager* gMaterialManager = nullptr;
FxMemPool* gEnginePool = nullptr;

#define DESTROY_GLOBAL(name_)                                                                                          \
    delete name_;                                                                                                      \
    name_ = nullptr


namespace FxGlobals {

void Init()
{
    gPhysics = new PhJolt;
    gAssetManager = new AxManager;
    gShaderCompiler = new FxShaderCompiler;
    gObjectManager = new FxObjectManager;
    gMaterialManager = new FxMaterialManager;
}


void Destroy()
{
    DESTROY_GLOBAL(gPhysics);
    DESTROY_GLOBAL(gShaderCompiler);
    DESTROY_GLOBAL(gObjectManager);
    DESTROY_GLOBAL(gMaterialManager);
    DESTROY_GLOBAL(gAssetManager);
}

} // namespace FxGlobals
