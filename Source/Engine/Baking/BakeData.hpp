/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Baking/LightmapTexel.hpp>

namespace Hyperion {

class LightmapVolume;
class ReflectionProbe;
class FogVolume;
class Light;
class Mesh;
class VoxelOctree;

namespace Baking {

class BakeDataBase
{
public:
    // Map from mesh id to an array of UV indices. Uses dynamic node allocation to reduce number of moves needed when adding or removing elements.
    using MeshToUVIndicesMap = Map<ObjId<Mesh>, Array<uint32, DynamicAllocator>>;

    struct TexelRange
    {
        uint32 start = 0;
        uint32 count = 0; // number of consecutive texels
    };

    using MeshToTexelRangesMap = Map<ObjId<Mesh>, Array<TexelRange, DynamicAllocator>>;

    /// Texels in UV space
    Array<LightmapTexel> texels;

    // Mapping from mesh id to the indices of the UVs that correspond to that mesh.
    MeshToUVIndicesMap meshToUvIndices;

    // Texel indices per mesh
    MeshToTexelRangesMap meshToTexelRanges;

    Span<const BakeEntity> bakeEntities;

    Vec3u dimensions; // only useful for some lightmap data types that use 2D/3D textures.

    BakeDataBase() = default;

    explicit BakeDataBase(Span<const BakeEntity> bakeEntities)
        : bakeEntities(bakeEntities)
    {
    }

    virtual ~BakeDataBase() = default;

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
class BakeData;

} // namespace Baking

} // namespace Hyperion
