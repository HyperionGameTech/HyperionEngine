/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <WebGPUPch.hpp>

#include <Rendering/WebGPU/WebGPUSampler.hpp>
#include <Rendering/WebGPU/WebGPURenderInterface.hpp>
#include <Rendering/WebGPU/WebGPUHelpers.hpp>

#include <WebGPUSampler.generated.inl>

namespace Hyperion {

extern WebGPURenderInterface RI;

WebGPUSampler::WebGPUSampler(const SamplerDesc& desc)
    : SamplerBase(desc),
      m_sampler(nullptr),
      m_isFiltering(false)
{
}

WebGPUSampler::~WebGPUSampler()
{
    if (m_sampler != nullptr)
    {
        wgpuSamplerRelease(m_sampler);
    }
}

bool WebGPUSampler::IsCreated() const
{
    return m_sampler != nullptr;
}

RendererResult WebGPUSampler::Create()
{
    if (m_sampler != nullptr)
    {
        return HYP_MAKE_ERROR(RendererError, "Sampler already created");
    }

    WGPUSamplerDescriptor descriptor = WGPU_SAMPLER_DESCRIPTOR_INIT;

    const WGPUAddressMode addressMode = ToWGPUAddressMode(m_wrapMode);
    descriptor.addressModeU = addressMode;
    descriptor.addressModeV = addressMode;
    descriptor.addressModeW = addressMode;

    descriptor.minFilter = WGPUFilterMode_Nearest;
    descriptor.magFilter = WGPUFilterMode_Nearest;
    descriptor.mipmapFilter = WGPUMipmapFilterMode_Nearest;

    switch (m_minFilterMode)
    {
    case TextureFilterMode::Nearest:
    case TextureFilterMode::NearestMipmap:
    case TextureFilterMode::MinMaxMipmap:
        if (m_magFilterMode == TextureFilterMode::Linear)
        {
            descriptor.magFilter = WGPUFilterMode_Linear;
        }

        break;
    case TextureFilterMode::Linear:
        descriptor.minFilter = WGPUFilterMode_Linear;
        descriptor.mipmapFilter = WGPUMipmapFilterMode_Linear;

        if (m_magFilterMode != TextureFilterMode::Nearest)
        {
            descriptor.magFilter = WGPUFilterMode_Linear;
        }

        break;
    case TextureFilterMode::LinearMipmap:
        descriptor.minFilter = WGPUFilterMode_Linear;
        descriptor.magFilter = WGPUFilterMode_Linear;
        descriptor.mipmapFilter = WGPUMipmapFilterMode_Linear;
        descriptor.maxAnisotropy = 8;
        break;
    default:
        descriptor.minFilter = WGPUFilterMode_Linear;
        descriptor.magFilter = WGPUFilterMode_Linear;
        descriptor.mipmapFilter = WGPUMipmapFilterMode_Linear;
        break;
    }

    if (m_compareOp != SamplerCompareOp::None)
    {
        descriptor.compare = ToWGPUCompareFunction(m_compareOp);
    }

    m_isFiltering = descriptor.minFilter == WGPUFilterMode_Linear
        || descriptor.magFilter == WGPUFilterMode_Linear
        || descriptor.mipmapFilter == WGPUMipmapFilterMode_Linear;

    m_sampler = wgpuDeviceCreateSampler(RI.GetDevice(), &descriptor);

    if (m_sampler == nullptr)
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to create WebGPU sampler");
    }

    return {};
}

#ifdef HYP_RHI_DEBUG_NAMES
void WebGPUSampler::SetDebugName(Name name)
{
    SamplerBase::SetDebugName(name);

    if (m_sampler != nullptr && name.IsValid())
    {
        wgpuSamplerSetLabel(m_sampler, ToWGPUStringView(*name));
    }
}
#endif

} // namespace Hyperion
