#include "PlaceholderData.hpp"

#include <Engine.hpp>

namespace hyperion::v2 {
PlaceholderData::PlaceholderData()
    : m_image_2d_1x1_r8(
          renderer::Extent2D(1, 1),
          renderer::InternalFormat::R8,
          renderer::FilterMode::TEXTURE_FILTER_NEAREST,
          nullptr
      ),
      m_image_3d_1x1x1_r8(
          renderer::Extent3D(1, 1, 1),
          renderer::InternalFormat::R8,
          renderer::FilterMode::TEXTURE_FILTER_NEAREST,
          nullptr
      ),
      m_image_3d_1x1x1_r8_storage(
          renderer::Extent3D(1, 1, 1),
          InternalFormat::R8,
          ImageType::TEXTURE_TYPE_3D,
          FilterMode::TEXTURE_FILTER_NEAREST
      ),
      m_image_cube_1x1_r8(
          renderer::Extent2D(1, 1),
          renderer::InternalFormat::R8,
          renderer::FilterMode::TEXTURE_FILTER_NEAREST,
          nullptr
      ),
      m_sampler_linear(
          renderer::FilterMode::TEXTURE_FILTER_LINEAR,
          renderer::WrapMode::TEXTURE_WRAP_REPEAT
      ),
      m_sampler_linear_mipmap(
          renderer::FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
          renderer::WrapMode::TEXTURE_WRAP_REPEAT
      ),
      m_sampler_nearest(
          renderer::FilterMode::TEXTURE_FILTER_NEAREST,
          renderer::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
      )
{
}

void PlaceholderData::Create()
{
    auto *device = Engine::Get()->GetGPUDevice();

    HYPERION_ASSERT_RESULT(m_image_2d_1x1_r8.Create(device));
    HYPERION_ASSERT_RESULT(m_image_view_2d_1x1_r8.Create(device, &m_image_2d_1x1_r8));
    HYPERION_ASSERT_RESULT(m_image_3d_1x1x1_r8.Create(device));
    HYPERION_ASSERT_RESULT(m_image_view_3d_1x1x1_r8.Create(device, &m_image_3d_1x1x1_r8));
    HYPERION_ASSERT_RESULT(m_image_3d_1x1x1_r8_storage.Create(device));
    HYPERION_ASSERT_RESULT(m_image_view_3d_1x1x1_r8_storage.Create(device, &m_image_3d_1x1x1_r8_storage));
    HYPERION_ASSERT_RESULT(m_image_cube_1x1_r8.Create(device));
    HYPERION_ASSERT_RESULT(m_image_view_cube_1x1_r8.Create(device, &m_image_cube_1x1_r8));
    HYPERION_ASSERT_RESULT(m_sampler_linear.Create(device));
    HYPERION_ASSERT_RESULT(m_sampler_linear_mipmap.Create(device));
    HYPERION_ASSERT_RESULT(m_sampler_nearest.Create(device));
}

void PlaceholderData::Destroy()
{
    auto *device = Engine::Get()->GetGPUDevice();

    HYPERION_ASSERT_RESULT(m_image_2d_1x1_r8.Destroy(device));
    HYPERION_ASSERT_RESULT(m_image_view_2d_1x1_r8.Destroy(device));
    HYPERION_ASSERT_RESULT(m_image_3d_1x1x1_r8.Destroy(device));
    HYPERION_ASSERT_RESULT(m_image_view_3d_1x1x1_r8.Destroy(device));
    HYPERION_ASSERT_RESULT(m_image_3d_1x1x1_r8_storage.Destroy(device));
    HYPERION_ASSERT_RESULT(m_image_view_3d_1x1x1_r8_storage.Destroy(device));
    HYPERION_ASSERT_RESULT(m_image_cube_1x1_r8.Destroy(device));
    HYPERION_ASSERT_RESULT(m_image_view_cube_1x1_r8.Destroy(device));
    HYPERION_ASSERT_RESULT(m_sampler_linear.Destroy(device));
    HYPERION_ASSERT_RESULT(m_sampler_linear_mipmap.Destroy(device));
    HYPERION_ASSERT_RESULT(m_sampler_nearest.Destroy(device));

    for (auto &it : m_buffers) {
        DebugLog(
            LogType::Debug,
            "Destroy %llu placeholder GPUBuffer: %u\n",
            it.second.Size()
        );

        for (auto &buffer_map_it : it.second) {
            AssertThrow(buffer_map_it.second != nullptr);
            HYPERION_ASSERT_RESULT(buffer_map_it.second->Destroy(device));
        }
    }
}

} // namespace hyperion::v2
