/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Animation/BoneMask.hpp>
#include <Scene/Animation/Skeleton.hpp>

#include <BoneMask.generated.inl>

namespace Hyperion {

BoneMask::BoneMask() = default;

BoneMask::BoneMask(Name name)
{
    SetName(name);
}

BoneMask::~BoneMask() = default;

Array<float, AnimAllocator> BoneMask::BuildSlotWeights(const SkeletonBindData& bindData) const
{
    Array<float, AnimAllocator> weights;
    weights.ResizeUninitialized(bindData.NumSlots());

    for (float& weight : weights)
    {
        weight = m_defaultWeight;
    }

    for (const BoneMaskEntry& entry : m_entries)
    {
        const uint32 entrySlot = bindData.FindSlot(entry.boneName);

        if (entrySlot == SkeletonBindData::InvalidSlot)
        {
            continue;
        }

        if (!entry.includeDescendants)
        {
            weights[entrySlot] = entry.weight;

            continue;
        }

        for (uint32 slot = 0; slot < bindData.NumSlots(); ++slot)
        {
            if (bindData.IsSlotWithin(slot, entry.boneName))
            {
                weights[slot] = entry.weight;
            }
        }
    }

    return weights;
}

Array<float> BoneMask::BuildExcludingSubtree(const SkeletonBindData& bindData, Name excludedBoneName)
{
    Array<float> weights;
    weights.ResizeUninitialized(bindData.NumSlots());

    for (uint32 slot = 0; slot < bindData.NumSlots(); ++slot)
    {
        weights[slot] = excludedBoneName.IsValid() && bindData.IsSlotWithin(slot, excludedBoneName) ? 0.0f : 1.0f;
    }

    return weights;
}

} // namespace Hyperion
