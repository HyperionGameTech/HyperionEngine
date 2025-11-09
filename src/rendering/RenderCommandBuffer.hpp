/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderDevice.hpp>
#include <rendering/RenderGpuBuffer.hpp>

#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

#include <core/Defines.hpp>

namespace hyperion {

HYP_CLASS(Abstract, NoScriptBindings)
class CommandBufferBase : public ObjectBase
{
    HYP_OBJECT_BODY(CommandBufferBase);

public:
    virtual ~CommandBufferBase() override = default;

    virtual bool IsCreated() const = 0;

    virtual RendererResult Create() = 0;

    virtual void BindVertexBuffer(const GpuBufferBase* buffer) = 0;
    virtual void BindIndexBuffer(const GpuBufferBase* buffer, GpuElemType elemType = GET_UNSIGNED_INT) = 0;

    virtual void DrawIndexed(
        uint32 numIndices,
        uint32 numInstances = 1,
        uint32 instanceIndex = 0) const = 0;

    virtual void DrawIndexedIndirect(
        const GpuBufferBase* buffer,
        uint32 bufferOffset) const = 0;

    void ResetStencilState()
    {
        stencilReference = 0;
        stencilCompareMask = 0xFF;
        stencilWriteMask = 0xFF;
    }

    uint8 stencilReference = 0;
    uint8 stencilCompareMask = 0xFF;
    uint8 stencilWriteMask = 0xFF;
};

} // namespace hyperion
