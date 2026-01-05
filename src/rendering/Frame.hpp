/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderResult.hpp>
#include <rendering/RenderObject.hpp>

#include <rendering/RenderQueue.hpp>

#include <core/Defines.hpp>

#include <core/containers/HashSet.hpp>

#include <core/functional/Delegate.hpp>

#include <core/Types.hpp>

// #define HYP_DEBUG_USED_DESCRIPTOR_SETS

namespace Hyperion {

HYP_CLASS(Abstract, NoScriptBindings)
class FrameBase : public ObjectBase
{
    HYP_OBJECT_BODY(FrameBase);

public:
    virtual ~FrameBase() override = default;

    virtual bool IsCreated() const = 0;
    virtual RendererResult Create() = 0;
    virtual RendererResult ResetFrameState() = 0;

    void UpdateUsedDescriptorSets();

    HYP_FORCE_INLINE uint32 GetFrameIndex() const
    {
        return m_frameIndex;
    }

    void MarkDescriptorSetUsed(DescriptorSet* descriptorSet);

    Delegate<void, Frame*> OnPresent;
    Delegate<void, Frame*> OnFrameEnd;

    RenderQueue renderQueue;
    RenderQueue preRenderQueue;
    RenderQueue postRenderQueue;

protected:
    explicit FrameBase(uint32 frameIndex)
        : m_frameIndex(frameIndex)
    {
    }

    uint32 m_frameIndex;
    HashSet<DescriptorSet*> m_usedDescriptorSets;
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <rendering/vulkan/VulkanFrame.hpp>
#elif HYP_DX12
#include <rendering/dx12/DX12Frame.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif
