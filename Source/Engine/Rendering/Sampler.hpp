/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderResult.hpp>
#include <Rendering/Shared.hpp>

#include <Core/Defines.hpp>

namespace Hyperion {

HYP_CLASS(Abstract, NoScriptBindings)
class SamplerBase : public ObjectBase
{
    HYP_OBJECT_BODY(SamplerBase);

public:
    static Pool* GetAllocator() { return g_rhiPool; }

    virtual ~SamplerBase() override = default;

    HYP_FORCE_INLINE TextureFilterMode GetMinFilterMode() const
    {
        return m_minFilterMode;
    }

    HYP_FORCE_INLINE TextureFilterMode GetMagFilterMode() const
    {
        return m_magFilterMode;
    }

    HYP_FORCE_INLINE TextureWrapMode GetWrapMode() const
    {
        return m_wrapMode;
    }

    HYP_FORCE_INLINE SamplerCompareOp GetCompareOp() const
    {
        return m_compareOp;
    }

    virtual bool IsCreated() const = 0;

    virtual RendererResult Create() = 0;

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

protected:
    SamplerBase() = default;

    explicit SamplerBase(const SamplerDesc& desc)
        : m_minFilterMode(desc.minFilterMode),
          m_magFilterMode(desc.magFilterMode),
          m_wrapMode(desc.wrapMode),
          m_compareOp(desc.compareOp)
    {
    }

    TextureFilterMode m_minFilterMode = TFM_NEAREST;
    TextureFilterMode m_magFilterMode = TFM_NEAREST;
    TextureWrapMode m_wrapMode = TWM_CLAMP_TO_EDGE;
    SamplerCompareOp m_compareOp = SamplerCompareOp::None;

#if HYP_DEBUG_MODE
    Name m_debugName;
#endif
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <Rendering/vulkan/VulkanSampler.hpp>
#elif HYP_DX12
#include <Rendering/dx12/DX12Sampler.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif
