/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/RenderableAttributes.hpp>
#include <rendering/ShaderManager.hpp>

#include <RenderableAttributes.generated.inl>

namespace Hyperion {

RuntimeMaterialAttributes::RuntimeMaterialAttributes()
    : shaderCacheId(InvalidShaderCacheId),
      bucket(RB_OPAQUE),
      fillMode(FM_FILL),
      cullFaces(FCM_BACK),
      flags(MAF_DEPTH_WRITE | MAF_DEPTH_TEST),
      stencilReference(0),
      stencilFunction(),
      blendFunction(BlendFunction::None()),
      textureMask(0)
{
}

RuntimeMaterialAttributes::RuntimeMaterialAttributes(const MaterialAttributes& materialAttributes)
{
    shaderCacheId = g_shaderManager->GetShaderCacheId(materialAttributes.shaderDefinition, /* createIfNotExists */ true);
    bucket = materialAttributes.bucket;
    fillMode = materialAttributes.fillMode;
    cullFaces = materialAttributes.cullFaces;
    flags = materialAttributes.flags;
    stencilReference = materialAttributes.stencilReference;
    stencilFunction = materialAttributes.stencilFunction;
    blendFunction = materialAttributes.blendFunction;
    textureMask = materialAttributes.textureMask;
}

RuntimeMaterialAttributes::operator MaterialAttributes() const
{
    MaterialAttributes materialAttributes;
    
    const ShaderDefinition* shaderDefinition = g_shaderManager->GetShaderDefinition(shaderCacheId);
    AssertDebug(shaderDefinition != nullptr);

    if (shaderDefinition)
    {
        materialAttributes.shaderDefinition = *shaderDefinition;
    }

    materialAttributes.bucket = bucket;
    materialAttributes.fillMode = fillMode;
    materialAttributes.cullFaces = cullFaces;
    materialAttributes.flags = flags;
    materialAttributes.stencilReference = stencilReference;
    materialAttributes.stencilFunction = stencilFunction;
    materialAttributes.blendFunction = blendFunction;
    materialAttributes.textureMask = textureMask;

    return materialAttributes;
}

} // namespace Hyperion
