/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderResult.hpp>
#include <rendering/RenderObject.hpp>
#include <core/Defines.hpp>

namespace Hyperion {

HYP_CLASS(Abstract, NoScriptBindings)
class GpuImageViewBase : public ObjectBase
{
    HYP_OBJECT_BODY(GpuImageViewBase);

public:
    virtual ~GpuImageViewBase() override;

    Name GetDebugName() const
    {
        return m_debugName;
    }

    virtual void SetDebugName(Name name)
    {
        m_debugName = name;
    }

    HYP_FORCE_INLINE const GpuImageRef& GetImage() const
    {
        return m_image;
    }

    virtual bool IsCreated() const = 0;

    virtual RendererResult Create() = 0;

protected:
    explicit GpuImageViewBase(const GpuImageRef& image);

    GpuImageViewBase(
        const GpuImageRef& image,
        uint32 mipIndex,
        uint32 numMips,
        uint32 layerIndex,
        uint32 numLayers);

    GpuImageRef m_image;
    uint32 m_mipIndex;
    uint32 m_numMips;
    uint32 m_layerIndex;
    uint32 m_numLayers;

    Name m_debugName;
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <rendering/vulkan/VulkanGpuImageView.hpp>
#elif HYP_DX12
#include <rendering/dx12/DX12GpuImageView.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif
