#ifndef HYPERION_V2_DUMMY_DATA_H
#define HYPERION_V2_DUMMY_DATA_H

#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>

#include <memory>

namespace hyperion::v2 {

using renderer::TextureImage2D;
using renderer::TextureImageCube;
using renderer::ImageView;
using renderer::Sampler;

class Engine;

class DummyData {
public:
    DummyData();

#define HYP_DEF_DUMMY_DATA(type, getter, member) \
    public: \
        type &Get##getter()             { return member; } \
        const type &Get##getter() const { return member; } \
    private: \
        type member

    HYP_DEF_DUMMY_DATA(TextureImage2D, Image2D1x1R8, m_image_2d_1x1_r8);
    HYP_DEF_DUMMY_DATA(ImageView, ImageView2D1x1R8, m_image_view_2d_1x1_r8);
    HYP_DEF_DUMMY_DATA(TextureImageCube, ImageCube1x1R8, m_image_cube_1x1_r8);
    HYP_DEF_DUMMY_DATA(ImageView, ImageViewCube1x1R8, m_image_view_cube_1x1_r8);
    HYP_DEF_DUMMY_DATA(Sampler, Sampler, m_sampler);

#undef HYP_DEF_DUMMY_DATA

public:
    void Create(Engine *engine);
    void Destroy(Engine *engine);
};

} // namespace hyperion::v2

#endif
