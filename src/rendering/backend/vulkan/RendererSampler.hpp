#ifndef RENDERER_SAMPLER_H
#define RENDERER_SAMPLER_H

#include "RendererResult.hpp"
#include "RendererImage.hpp"

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {
class ImageView;
class Device;
class Sampler {
public:
    Sampler(
        Image::FilterMode filter_mode = Image::FilterMode::TEXTURE_FILTER_NEAREST,
        Image::WrapMode wrap_mode = Image::WrapMode::TEXTURE_WRAP_CLAMP_TO_BORDER
    );

    Sampler(const Sampler &other) = delete;
    Sampler &operator=(const Sampler &other) = delete;
    ~Sampler();

    inline VkSampler &GetSampler() { return m_sampler; }
    inline const VkSampler &GetSampler() const { return m_sampler; }

    inline Image::FilterMode GetFilterMode() const { return m_filter_mode; }
    inline Image::WrapMode GetWrapMode() const { return m_wrap_mode; }

    Result Create(Device *device);
    Result Destroy(Device *device);

private:
    VkSampler m_sampler;
    Image::FilterMode m_filter_mode;
    Image::WrapMode m_wrap_mode;
};

} // namespace renderer
} // namespace hyperion

#endif