/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/lightmapper/LightmapTexel.hpp>

namespace hyperion {

class LightmapVolume;
class Mesh;

/*! \brief Base class for lightmap texel source, used to trace rays from and store texel data to. */
class LightmapDataBase
{
public:
    // HashMap from mesh id to an array of UV indices. Uses dynamic node allocation to reduce number of moves needed when adding or removing elements.
    using MeshToUVIndicesMap = HashMap<ObjId<Mesh>, Array<uint32, DynamicAllocator>>;

    struct TexelRange
    {
        uint32 start = 0;
        uint32 count = 0; // number of consecutive texels
    };

    using MeshToTexelRangesMap = HashMap<ObjId<Mesh>, Array<TexelRange, DynamicAllocator>>;

    uint32 width = 0;
    uint32 height = 0;

    /// Texels in UV space
    Array<LightmapTexel> texels;

    // Mapping from mesh Id to the indices of the UVs that correspond to that mesh.
    MeshToUVIndicesMap meshToUvIndices;

    // Texel indices per mesh
    MeshToTexelRangesMap meshToTexelRanges;

    LightmapDataBase() = default;
    virtual ~LightmapDataBase() = default;
};

template <class T>
class LightmapData;

template <>
class LightmapData<LightmapVolume> : public LightmapDataBase
{
public:
    using BitmapType = Bitmap_RGBA8;

    using MeshFloatDataArray = Array<float, DynamicAllocator>;
    using MeshIndexArray = Array<uint32, DynamicAllocator>;

    LightmapData() = default;

    LightmapData(const LightmapUVBuilderParams& params);

    LightmapData(const LightmapData& other) = default;
    LightmapData(LightmapData&& other) noexcept = default;

    LightmapData& operator=(const LightmapData& other) = default;
    LightmapData& operator=(LightmapData&& other) noexcept = default;

    ~LightmapData() = default;

    HYP_FORCE_INLINE const Array<LightmapMeshData>& GetMeshData() const
    {
        return m_meshData;
    }

    HYP_FORCE_INLINE bool IsBuilt() const
    {
        return texels.Any();
    }

    Result Build();

    BitmapType ToBitmapRadiance() const;
    BitmapType ToBitmapIrradiance() const;

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
