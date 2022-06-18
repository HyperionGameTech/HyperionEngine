#include "dummy_data.h"

#include <engine.h>

namespace hyperion::v2 {
DummyData::DummyData()
    : m_image_2d_1x1_r8(
          renderer::Extent2D(1, 1),
          renderer::Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_R8,
          renderer::Image::FilterMode::TEXTURE_FILTER_NEAREST,
          nullptr
      ),
      m_sampler(
          renderer::Image::FilterMode::TEXTURE_FILTER_LINEAR,
          renderer::Image::WrapMode::TEXTURE_WRAP_REPEAT
      )
{
}

void DummyData::Create(Engine *engine)
{
    auto *device = engine->GetDevice();

    HYPERION_ASSERT_RESULT(m_image_2d_1x1_r8.Create(device));
    HYPERION_ASSERT_RESULT(m_image_view_2d_1x1_r8.Create(device, &m_image_2d_1x1_r8));
    HYPERION_ASSERT_RESULT(m_sampler.Create(device, &m_image_view_2d_1x1_r8));
}

void DummyData::Destroy(Engine *engine)
{
    auto *device = engine->GetDevice();

    HYPERION_ASSERT_RESULT(m_image_2d_1x1_r8.Destroy(device));
    HYPERION_ASSERT_RESULT(m_image_view_2d_1x1_r8.Destroy(device));
    HYPERION_ASSERT_RESULT(m_sampler.Destroy(device));
}

} // namespace hyperion::v2
