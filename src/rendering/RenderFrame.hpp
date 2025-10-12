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

namespace hyperion {

HYP_CLASS(Abstract, NoScriptBindings)
class FrameBase : public HypObjectBase
{
    HYP_OBJECT_BODY(FrameBase);

public:
    virtual ~FrameBase() override = default;

    virtual RendererResult Create() = 0;

    virtual RendererResult ResetFrameState() = 0;

    void UpdateUsedDescriptorSets();

    HYP_FORCE_INLINE uint32 GetFrameIndex() const
    {
        return m_frameIndex;
    }

    void MarkDescriptorSetUsed(DescriptorSetBase* descriptorSet);

    Delegate<void, FrameBase*> OnPresent;
    Delegate<void, FrameBase*> OnFrameEnd;

    RenderQueue renderQueue;
    RenderQueue preRenderQueue;

protected:
    FrameBase(uint32 frameIndex)
        : m_frameIndex(frameIndex)
    {
    }

    uint32 m_frameIndex;
    HashSet<DescriptorSetBase*> m_usedDescriptorSets;
};

} // namespace hyperion
