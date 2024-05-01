#include <rendering/RenderResourceManager.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <scene/animation/Skeleton.hpp>

namespace hyperion {

RenderResourceManager::RenderResourceManager()
{
    InitResourceUsageMap<Mesh>();
    InitResourceUsageMap<Material>();
    InitResourceUsageMap<Skeleton>();
}

RenderResourceManager::~RenderResourceManager()
{
    Reset();
}

void RenderResourceManager::CollectNeededResourcesForBits(ResourceUsageType type, const Bitset &bits)
{
    ResourceUsageMapBase *map = m_resource_usage_maps[uint32(type)].Get();

    Bitset new_bits = bits;
    Bitset prev_bits = map->usage_bits;

    // If any of the bitsets are different sizes, resize them to match the largest one,
    // this makes ~ and & operations work as expected
    if (prev_bits.NumBits() > new_bits.NumBits()) {
        new_bits.Resize(prev_bits.NumBits());
    } else if (prev_bits.NumBits() < new_bits.NumBits()) {
        prev_bits.Resize(new_bits.NumBits());
    }

    SizeType first_set_bit_index;

    Bitset removed_id_bits = prev_bits & ~new_bits;
    Bitset newly_added_id_bits = new_bits & ~prev_bits;

    // Iterate over the bits that were removed, and drop the references to them.
    while ((first_set_bit_index = removed_id_bits.FirstSetBitIndex()) != -1) {
        // Remove the reference
        switch (type) {
        case RESOURCE_USAGE_TYPE_MESH:
            SetIsUsed(map, ID<Mesh>::FromIndex(first_set_bit_index), false);
            break;
        case RESOURCE_USAGE_TYPE_MATERIAL:
            SetIsUsed(map, ID<Material>::FromIndex(first_set_bit_index), false);
            break;
        case RESOURCE_USAGE_TYPE_SKELETON:
            SetIsUsed(map, ID<Skeleton>::FromIndex(first_set_bit_index), false);
            break;
        }

        removed_id_bits.Set(first_set_bit_index, false);
    }

    while ((first_set_bit_index = newly_added_id_bits.FirstSetBitIndex()) != -1) {
        // Create a reference to it in the resources list.
        switch (type) {
        case RESOURCE_USAGE_TYPE_MESH:
            SetIsUsed(map, ID<Mesh>::FromIndex(first_set_bit_index), true);
            break;
        case RESOURCE_USAGE_TYPE_MATERIAL:
            SetIsUsed(map, ID<Material>::FromIndex(first_set_bit_index), true);
            break;
        case RESOURCE_USAGE_TYPE_SKELETON:
            SetIsUsed(map, ID<Skeleton>::FromIndex(first_set_bit_index), true);
            break;
        }

        newly_added_id_bits.Set(first_set_bit_index, false);
    }
}

void RenderResourceManager::Reset()
{
    for (auto &map : m_resource_usage_maps) {
        map->Reset();
    }
}


} // namespace hyperion