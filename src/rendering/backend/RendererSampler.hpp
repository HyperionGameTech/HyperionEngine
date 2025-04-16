/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_SAMPLER_HPP
#define HYPERION_BACKEND_RENDERER_SAMPLER_HPP

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <core/Defines.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
struct SamplerPlatformImpl;

template <PlatformType PLATFORM>
class Sampler
{
public:
    static constexpr PlatformType platform = PLATFORM;
    
    HYP_API Sampler(
        FilterMode min_filter_mode = FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode mag_filter_mode = FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode wrap_mode = WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    );

    Sampler(const Sampler &other)               = delete;
    Sampler &operator=(const Sampler &other)    = delete;
    HYP_API Sampler(Sampler &&other) noexcept;
    HYP_API Sampler &operator=(Sampler &&other) noexcept;
    HYP_API ~Sampler();
    
    HYP_FORCE_INLINE SamplerPlatformImpl<PLATFORM> &GetPlatformImpl()
        { return m_platform_impl; }
    
    HYP_FORCE_INLINE const SamplerPlatformImpl<PLATFORM> &GetPlatformImpl() const
        { return m_platform_impl; }
    
    HYP_FORCE_INLINE FilterMode GetMinFilterMode() const
        { return m_min_filter_mode; }
    
    HYP_FORCE_INLINE FilterMode GetMagFilterMode() const
        { return m_mag_filter_mode; }
    
    HYP_FORCE_INLINE WrapMode GetWrapMode() const
        { return m_wrap_mode; }

    HYP_API RendererResult Create();
    HYP_API RendererResult Destroy();

private:
    SamplerPlatformImpl<PLATFORM>   m_platform_impl;
    FilterMode                      m_min_filter_mode;
    FilterMode                      m_mag_filter_mode;
    WrapMode                        m_wrap_mode;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererSampler.hpp>
#else
#error Unsupported rendering backend
#endif


namespace hyperion {
namespace renderer {

using Sampler = platform::Sampler<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif