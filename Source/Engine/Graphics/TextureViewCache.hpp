/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/reflection/Handle.hpp>

#include <rendering/RenderTypes.hpp>

namespace Hyperion {

class Texture;
struct ImageSubResource;

enum class TextureType : uint8;

class TextureViewCacheBase
{
public:
    virtual ~TextureViewCacheBase() = default;

    virtual const GpuImageViewRef& GetOrCreate(
        Texture* texture,
        uint32 mipIndex = 0,
        uint32 numMips = ~0u,
        uint32 layerIndex = 0,
        uint32 numLayers = ~0u) = 0;

    virtual const GpuImageViewRef& GetOrCreate(
        Texture* texture,
        const ImageSubResource& subResource) = 0;

    virtual const GpuImageViewRef& GetOrCreate(
        Texture* texture,
        const ImageSubResource& subResource,
        TextureType viewTextureType) = 0;

    virtual void RemoveTexture(const Texture* texture) = 0;

    virtual void CleanupUnusedTextures() = 0;
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <rendering/vulkan/VulkanTextureViewCache.hpp>
#elif HYP_DX12
#include <rendering/dx12/DX12TextureViewCache.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif
