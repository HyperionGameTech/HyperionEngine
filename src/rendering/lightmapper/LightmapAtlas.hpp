/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/lightmapper/LightmapTexel.hpp>

namespace hyperion {

using LightmapAtlasBitmap = Bitmap_RGBA8;

class LightmapAtlas : public LightmapTexelsBase
{
public:
    using MeshFloatDataArray = Array<float, DynamicAllocator>;
    using MeshIndexArray = Array<uint32, DynamicAllocator>;

    LightmapAtlas() = default;

    LightmapAtlas(const LightmapUVBuilderParams& params);

    LightmapAtlas(const LightmapAtlas& other) = default;
    LightmapAtlas(LightmapAtlas&& other) noexcept = default;

    LightmapAtlas& operator=(const LightmapAtlas& other) = default;
    LightmapAtlas& operator=(LightmapAtlas&& other) noexcept = default;

    ~LightmapAtlas() = default;

    HYP_FORCE_INLINE const Array<LightmapMeshData>& GetMeshData() const
    {
        return m_meshData;
    }

    HYP_FORCE_INLINE bool IsBuilt() const
    {
        return texels.Any();
    }

    Result Build();

    LightmapAtlasBitmap ToBitmapRadiance() const;
    LightmapAtlasBitmap ToBitmapIrradiance() const;

private:
    LightmapUVBuilderParams m_params;
    Array<LightmapMeshData> m_meshData;

    // Per element mesh data used for building the UV map
    Array<MeshFloatDataArray, DynamicAllocator> m_meshVertexPositions;
    Array<MeshFloatDataArray, DynamicAllocator> m_meshVertexNormals;
    Array<MeshFloatDataArray, DynamicAllocator> m_meshVertexUvs;
    Array<Array<uint32>, DynamicAllocator> m_meshIndices;
};

} // namespace hyperion
