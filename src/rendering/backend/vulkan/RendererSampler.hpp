#ifndef RENDERER_SAMPLER_H
#define RENDERER_SAMPLER_H

#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererImage.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {

class ImageView;

namespace platform {
template <PlatformType PLATFORM>
class Device;
} // namespace platform

using Device = platform::Device<Platform::VULKAN>;

class Sampler
{
public:
    Sampler(
        FilterMode filter_mode = FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode wrap_mode = WrapMode::TEXTURE_WRAP_CLAMP_TO_BORDER
    );

    Sampler(const Sampler &other) = delete;
    Sampler &operator=(const Sampler &other) = delete;
    Sampler(Sampler &&other) noexcept;
    Sampler &operator=(Sampler &&other) noexcept;
    ~Sampler();

    VkSampler &GetSampler() { return m_sampler; }
    const VkSampler &GetSampler() const { return m_sampler; }

    FilterMode GetFilterMode() const { return m_filter_mode; }
    WrapMode GetWrapMode() const { return m_wrap_mode; }

    Result Create(Device *device);
    Result Destroy(Device *device);

private:
    VkSampler m_sampler;
    FilterMode m_filter_mode;
    WrapMode m_wrap_mode;
};

} // namespace renderer
} // namespace hyperion

#endif