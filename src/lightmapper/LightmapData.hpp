/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <lightmapper/LightmapTexel.hpp>

namespace Hyperion {

class LightmapVolume;
class ReflectionProbe;
class FogVolume;
class Mesh;
class VoxelOctree;

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

    /// Texels in UV space
    Array<LightmapTexel> texels;

    // Mapping from mesh Id to the indices of the UVs that correspond to that mesh.
    MeshToUVIndicesMap meshToUvIndices;

    // Texel indices per mesh
    MeshToTexelRangesMap meshToTexelRanges;

    Span<const LightmapSubElement> subElements;

    Vec3u dimensions; // only useful for some lightmap data types that use 2D/3D textures.

    LightmapDataBase() = default;

    explicit LightmapDataBase(Span<const LightmapSubElement> subElements)
        : subElements(subElements)
    {
    }

    virtual ~LightmapDataBase() = default;

    virtual Result Build() = 0;

    HYP_FORCE_INLINE bool IsBuilt() const
    {
        return texels.Any();
    }

    HYP_FORCE_INLINE uint32 GetWidth() const
    {
        return dimensions.x;
    }

    HYP_FORCE_INLINE uint32 GetHeight() const
    {
        return dimensions.y;
    }

    HYP_FORCE_INLINE uint32 GetDepth() const
    {
        return dimensions.z;
    }
};

template <class T>
class LightmapData;

template <>
class LightmapData<LightmapVolume> : public LightmapDataBase
{
public:
    using BitmapType = Bitmap_RGBA32F;

    using MeshFloatDataArray = Array<float, DynamicAllocator>;
    using MeshIndexArray = Array<uint32, DynamicAllocator>;

    LightmapData()
        : m_volume(nullptr)
    {
    }

    LightmapData(Span<const LightmapSubElement> subElements, LightmapVolume* volume);

    LightmapData(const LightmapData& other) = default;
    LightmapData(LightmapData&& other) noexcept = default;

    LightmapData& operator=(const LightmapData& other) = default;
    LightmapData& operator=(LightmapData&& other) noexcept = default;

    ~LightmapData() override = default;

    HYP_FORCE_INLINE const Array<LightmapMeshData>& GetMeshData() const
    {
        return m_meshData;
    }

    virtual Result Build() override;

    BitmapType ToBitmapIrradiance() const;
    BitmapType ToBitmapRadiance() const;

private:
    LightmapVolume* m_volume;

    Array<LightmapMeshData> m_meshData;

    Array<LightmapRay> m_rays;

    // Per element mesh data used for building the UV map
    Array<MeshFloatDataArray, DynamicAllocator> m_meshVertexPositions;
    Array<MeshFloatDataArray, DynamicAllocator> m_meshVertexNormals;
    Array<MeshFloatDataArray, DynamicAllocator> m_meshVertexUvs;
    Array<Array<uint32>, DynamicAllocator> m_meshIndices;
};

template <>
class LightmapData<ReflectionProbe> : public LightmapDataBase
{
public:
    using BitmapType = Bitmap_RGBA32F;

    LightmapData()
        : m_envProbe(nullptr)
    {
    }

    LightmapData(Span<const LightmapSubElement> subElements, ReflectionProbe* envProbe)
        : LightmapDataBase(subElements),
          m_envProbe(envProbe)
    {
    }

    LightmapData(const LightmapData& other) = default;
    LightmapData(LightmapData&& other) noexcept = default;

    LightmapData& operator=(const LightmapData& other) = default;
    LightmapData& operator=(LightmapData&& other) noexcept = default;

    ~LightmapData() override = default;

    virtual Result Build() override;

    BitmapType ToBitmap() const;

protected:
    ReflectionProbe* m_envProbe;
    Array<LightmapRay> m_rays;
};

template <>
class LightmapData<FogVolume> : public LightmapDataBase
{
public:
    static constexpr uint32 MaxNoiseBitmapExtent = 32;

    using VolumeBitmap = Bitmap3D_RG16F;
    using NoiseBitmap = Bitmap3D_R8;

    LightmapData()
        : m_fogVolume(nullptr)
    {
    }

    LightmapData(Span<const LightmapSubElement> subElements, FogVolume* fogVolume)
        : LightmapDataBase(subElements),
          m_fogVolume(fogVolume)
    {
    }

    LightmapData(const LightmapData& other) = delete;
    LightmapData(LightmapData&& other) noexcept = default;

    LightmapData& operator=(const LightmapData& other) = delete;
    LightmapData& operator=(LightmapData&& other) noexcept = default;

    ~LightmapData() override = default;

    HYP_FORCE_INLINE VoxelOctree* GetVoxelOctree() const
    {
        return m_voxelOctree.Get();
    }

    HYP_FORCE_INLINE VolumeBitmap& GetVolumeBitmap()
    {
        return m_volumeBitmap;
    }

    HYP_FORCE_INLINE const VolumeBitmap& GetVolumeBitmap() const
    {
        return m_volumeBitmap;
    }

    HYP_FORCE_INLINE NoiseBitmap& GetNoiseBitmap()
    {
        return m_noiseBitmap;
    }

    HYP_FORCE_INLINE const NoiseBitmap& GetNoiseBitmap() const
    {
        return m_noiseBitmap;
    }

    virtual Result Build() override;

protected:
    FogVolume* m_fogVolume;
    UniquePtr<VoxelOctree> m_voxelOctree;
    VolumeBitmap m_volumeBitmap;
    NoiseBitmap m_noiseBitmap;
};

} // namespace Hyperion
