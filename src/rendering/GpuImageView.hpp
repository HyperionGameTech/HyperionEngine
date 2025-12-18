/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderResult.hpp>
#include <rendering/RenderObject.hpp>
#include <core/Defines.hpp>

namespace hyperion {

HYP_CLASS(Abstract, NoScriptBindings)
class GpuImageViewBase : public ObjectBase
{
    HYP_OBJECT_BODY(GpuImageViewBase);

public:
    virtual ~GpuImageViewBase() override = default;

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
    explicit GpuImageViewBase(const GpuImageRef& image)
        : m_image(image),
          m_mipIndex(0),
          m_numMips(0),
          m_layerIndex(0),
          m_numLayers(0)
    {
    }

    GpuImageViewBase(
        const GpuImageRef& image,
        uint32 mipIndex,
        uint32 numMips,
        uint32 layerIndex,
        uint32 numLayers)
        : m_image(image),
          m_mipIndex(mipIndex),
          m_numMips(numMips),
          m_layerIndex(layerIndex),
          m_numLayers(numLayers)
    {
    }

    GpuImageRef m_image;
    uint32 m_mipIndex;
    uint32 m_numMips;
    uint32 m_layerIndex;
    uint32 m_numLayers;

    Name m_debugName;
};

} // namespace hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#ifdef HYP_VULKAN
#include <rendering/vulkan/VulkanGpuImageView.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif
