#include "RxShaderCache.hpp"

#include <Core/FxRefUtil.hpp>


RxShaderCache::RxShaderCache() { mCache.InitSize(RxShaderNameUtil::scNumShaders); }

FxRef<RxShader> RxShaderCache::Request(const RxShaderName id)
{
    FxRef<RxShader>& shader = mCache[id];
    if (!shader.IsValid()) {
        // Shader is not in cache, create a new one
        shader = FxMakeRef<RxShader>(RxShaderNameUtil::GetName(id));
    }

    return shader;
}
