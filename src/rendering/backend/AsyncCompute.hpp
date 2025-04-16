/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_ASYNC_COMPUTE_HPP
#define HYPERION_BACKEND_RENDERER_ASYNC_COMPUTE_HPP

#include <core/Defines.hpp>

#include <core/containers/FixedArray.hpp>
#include <core/containers/ArrayMap.hpp>

#include <rendering/rhi/RHICommandList.hpp>

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererBuffer.hpp>

#include <core/math/MathUtil.hpp>
#include <core/math/Extent.hpp>

#include <Types.hpp>

namespace hyperion::renderer {

class AsyncComputeBase
{
public:
    virtual ~AsyncComputeBase() = default;

    virtual bool IsSupported() const = 0;

    HYP_FORCE_INLINE RHICommandList &GetCommandList()
        { return m_command_list; }

    HYP_FORCE_INLINE const RHICommandList &GetCommandList(uint32 frame_index) const
        { return m_command_list; }

protected:
    RHICommandList  m_command_list;
};

} // namespace hyperion::renderer

#endif