/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RenderConfig.hpp>

#include <rendering/Buffers.hpp>

namespace hyperion {
namespace renderer {

void FrameBase::UpdateUsedDescriptorSets()
{
    for (const DescriptorSetRef& descriptor_set : m_used_descriptor_sets)
    {
        AssertThrow(descriptor_set.IsValid());
        AssertThrowMsg(descriptor_set->IsCreated(),
            "Descriptor set '%s' is not yet created when updating the frame's used descriptor sets!",
            descriptor_set->GetLayout().GetName().LookupString());

        descriptor_set->Update();
    }

    SafeRelease(std::move(m_used_descriptor_sets));
}

} // namespace renderer
} // namespace hyperion