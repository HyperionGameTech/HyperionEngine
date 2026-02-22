/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/SparsePagedArray.hpp>

#include <rendering/RenderObject.hpp>

namespace Hyperion {

class Material;
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
