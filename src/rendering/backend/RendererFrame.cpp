/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RenderConfig.hpp>

#include <rendering/Buffers.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {
void FrameBase::MarkDescriptorSetUsed(DescriptorSetBase* descriptor_set)
{
    AssertThrow(descriptor_set != nullptr);

    m_used_descriptor_sets.Insert(descriptor_set);

#ifdef HYP_DESCRIPTOR_SET_TRACK_FRAME_USAGE
    descriptor_set->GetCurrentFrames().Insert(WeakHandleFromThis());
#endif
}

void FrameBase::UpdateUsedDescriptorSets()
{
    for (DescriptorSetBase* descriptor_set : m_used_descriptor_sets)
    {
        AssertDebugMsg(descriptor_set->IsCreated(),
            "Descriptor set '%s' is not yet created when updating the frame's used descriptor sets!",
            descriptor_set->GetLayout().GetName().LookupString());

        bool is_dirty = false;
        descriptor_set->UpdateDirtyState(&is_dirty);

        if (!is_dirty)
        {
            // If the descriptor set is not dirty, we don't need to update it
            continue;
        }

#if defined(HYP_DEBUG_MODE) && defined(HYP_DESCRIPTOR_SET_TRACK_FRAME_USAGE)
        // Check to see if other frames are using the same descriptor set while we're updating them so we can assert an error if they are
        const SizeType count = descriptor_set->GetCurrentFrames().Size() - descriptor_set->GetCurrentFrames().Count(this);

        if (count != 0)
        {
            for (auto it = descriptor_set->GetCurrentFrames().Begin(); it != descriptor_set->GetCurrentFrames().End(); ++it)
            {
                if ((*it) == this)
                {
                    // If the current frame is already in the descriptor set's current frames, we don't need to add it again
                    continue;
                }

                HYP_FAIL("Descriptor set \"%s\" (debug name: %s, index: %u) already in use by frame \"%s\" (index: %u)!",
                    descriptor_set->GetLayout().GetName().LookupString(),
                    descriptor_set->GetDebugName().LookupString(),
                    descriptor_set->GetHeader_Internal()->index,
                    it->header->debug_name.LookupString(),
                    it->header->index);
            }
        }
#endif

        HYP_LOG(Rendering, Debug, "Updating descriptor set '{}' for frame '{}' (index: {})",
            descriptor_set->GetLayout().GetName().LookupString(),
            GetDebugName().LookupString(),
            m_frame_index);

        descriptor_set->Update();
    }
}

} // namespace hyperion