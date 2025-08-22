/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderFrame.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/RenderConfig.hpp>

#include <rendering/Buffers.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {
void FrameBase::MarkDescriptorSetUsed(DescriptorSetBase* descriptorSet)
{
    HYP_GFX_ASSERT(descriptorSet != nullptr);

    m_usedDescriptorSets.Insert(descriptorSet);

#ifdef HYP_DESCRIPTOR_SET_TRACK_FRAME_USAGE
    descriptorSet->GetCurrentFrames().Insert(WeakHandleFromThis());
#endif
}

void FrameBase::UpdateUsedDescriptorSets()
{
    for (DescriptorSetBase* descriptorSet : m_usedDescriptorSets)
    {
        HYP_GFX_ASSERT(descriptorSet->IsCreated(),
            "Descriptor set '%s' is not yet created when updating the frame's used descriptor sets!",
            descriptorSet->GetLayout().GetName().LookupString());

        // TEMP DEBUGGING!
        bool isDirty = true;// false;
        descriptorSet->UpdateDirtyState(&isDirty);

        if (!isDirty)
        {
            // If the descriptor set is not dirty, we don't need to update it
            continue;
        }

#if defined(HYP_DEBUG_MODE) && defined(HYP_DESCRIPTOR_SET_TRACK_FRAME_USAGE)
        // Check to see if other frames are using the same descriptor set while we're updating them so we can assert an error if they are
        const SizeType count = descriptorSet->GetCurrentFrames().Size() - descriptorSet->GetCurrentFrames().Count(this);

        if (count != 0)
        {
            for (auto it = descriptorSet->GetCurrentFrames().Begin(); it != descriptorSet->GetCurrentFrames().End(); ++it)
            {
                if ((*it) == this)
                {
                    // If the current frame is already in the descriptor set's current frames, we don't need to add it again
                    continue;
                }

                HYP_FAIL("Descriptor set \"%s\" (debug name: %s, index: %u) already in use",
                    descriptorSet->GetLayout().GetName().LookupString(),
                    descriptorSet->GetDebugName().LookupString(),
                    descriptorSet->GetHeader_Internal()->index);
            }
        }
#endif

        HYP_LOG(Rendering, Debug, "Updating descriptor set {} ({}) (index: {})",
            (void*)descriptorSet,
            descriptorSet->GetLayout().GetName().LookupString(),
            m_frameIndex);

        descriptorSet->Update();
    }
}

} // namespace hyperion
