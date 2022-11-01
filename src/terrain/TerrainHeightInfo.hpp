#ifndef HYPERION_V2_TERRAIN_HEIGHT_INFO_HPP
#define HYPERION_V2_TERRAIN_HEIGHT_INFO_HPP

#include <scene/controllers/PagingController.hpp>
#include <core/Containers.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

struct TerrainHeight
{
    Float height;
    Float erosion;
    Float sediment;
    Float water;
    Float new_water;
    Float displacement;
};

struct TerrainHeightData
{
    PatchInfo patch_info;
    Array<TerrainHeight> heights;

    TerrainHeightData(const PatchInfo &patch_info)
        : patch_info(patch_info)
    {
        heights.Resize(patch_info.extent.width * patch_info.extent.depth);
    }

    UInt GetHeightIndex(UInt x, UInt z) const
    {
        return ((x + patch_info.extent.width) % patch_info.extent.width)
                + ((z + patch_info.extent.depth) % patch_info.extent.depth) * patch_info.extent.width;
    }

};

} // namespace hyperion::v2

#endif