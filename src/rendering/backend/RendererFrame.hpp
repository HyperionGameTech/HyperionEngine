/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_FRAME_HPP
#define HYPERION_BACKEND_RENDERER_FRAME_HPP

#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererSemaphore.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <rendering/rhi/RHICommandList.hpp>

#include <core/Defines.hpp>

#include <core/functional/Delegate.hpp>

#include <Types.hpp>

namespace hyperion {
namespace renderer {

class FrameBase : public RenderObject<FrameBase>
{
public:
    virtual ~FrameBase() override = default;

    HYP_API virtual RendererResult Create() = 0;
    HYP_API virtual RendererResult Destroy() = 0;

    HYP_FORCE_INLINE uint32 GetFrameIndex() const
        { return m_frame_index; }

    HYP_FORCE_INLINE RHICommandList &GetCommandList()
        { return m_command_list; }

    HYP_FORCE_INLINE const RHICommandList &GetCommandList() const
        { return m_command_list; }

    Delegate<void, FrameBase *> OnFrameEnd;

protected:
    FrameBase(uint32 frame_index)
        : m_frame_index(frame_index)
    {
    }

    uint32              m_frame_index;
    RHICommandList      m_command_list;
};

} // namespace renderer
} // namespace hyperion

#endif