/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <rendering/RenderResult.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/Shared.hpp>

#include <Core/Defines.hpp>

namespace Hyperion {

HYP_CLASS(Abstract, NoScriptBindings)
class GpuImageViewBase : public ObjectBase
{
    HYP_OBJECT_BODY(GpuImageViewBase);

public:
    static Pool* GetAllocator() { return g_rhiPool; }
    
    virtual ~GpuImageViewBase() override;
    
#if HYP_DEBUG_MODE
    Name GetDebugName() const
    {
        return m_debugName;
    }

    virtual void SetDebugName(Name name)
    {
        m_debugName = name;
    }
#endif

    HYP_FORCE_INLINE const GpuImageRef& GetImage() const
    {
        return m_image;
    }

    HYP_FORCE_INLINE uint8 GetMipIndex() const
    {
        return m_subResource.baseMipLevel;
    }

    HYP_FORCE_INLINE uint8 NumMips() const
    {
        return m_subResource.numLevels;
    }

    HYP_FORCE_INLINE uint8 GetLayerIndex() const
    {
        return m_subResource.baseArrayLayer;
    }

    HYP_FORCE_INLINE uint8 NumArrayLayers() const
    {
        return m_subResource.numLayers;
    }

    HYP_FORCE_INLINE const ImageSubResource& GetImageSubResource() const
    {
        return m_subResource;
    }

    virtual bool IsCreated() const = 0;

    virtual RendererResult Create() = 0;

protected:
    explicit GpuImageViewBase(const GpuImageRef& image);
    GpuImageViewBase(const GpuImageRef& image, const ImageSubResource& subResource);

    GpuImageRef m_image;
    ImageSubResource m_subResource;
    
#if HYP_DEBUG_MODE
    Name m_debugName;
#endif
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
