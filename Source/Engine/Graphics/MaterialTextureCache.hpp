/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/containers/SparsePagedArray.hpp>

#include <rendering/RenderTypes.hpp>

namespace Hyperion {

class Texture;

enum class MaterialTextureKey : uint64;

class MaterialTextureCache
{
public:
    using MaterialImageViewsMap = SparsePagedArray<Array<GpuImageViewRef, RenderAllocator>, 1024, RenderAllocator>;

    MaterialTextureCache();

    MaterialTextureCache(const MaterialTextureCache& other) = delete;
    MaterialTextureCache& operator=(const MaterialTextureCache& other) = delete;

    MaterialTextureCache(MaterialTextureCache&& other) noexcept = delete;
    MaterialTextureCache& operator=(MaterialTextureCache&& other) noexcept = delete;

    ~MaterialTextureCache();

    // Maps material bound index -> array of image views for material textures
    // Use RenderProxyMaterial::boundTextureIndices for indexing
    MaterialImageViewsMap imageViews;
};

} // namespace Hyperion
