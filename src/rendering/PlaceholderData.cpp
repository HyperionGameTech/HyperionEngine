#include "PlaceholderData.hpp"

#include <Engine.hpp>

namespace hyperion::v2 {
PlaceholderData::PlaceholderData()
    : m_image_2d_1x1_r8(
          renderer::Extent2D(1, 1),
          renderer::Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_R8,
          renderer::Image::FilterMode::TEXTURE_FILTER_NEAREST,
          nullptr
      ),
      m_image_3d_1x1x1_r8(
          renderer::Extent3D(1, 1, 1),
          renderer::Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_R8,
          renderer::Image::FilterMode::TEXTURE_FILTER_NEAREST,
          nullptr
      ),
      m_image_3d_1x1x1_r8_storage(
          renderer::Extent3D(1, 1, 1),
          Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_R8,
          Image::Type::TEXTURE_TYPE_3D,
          Image::FilterMode::TEXTURE_FILTER_NEAREST
      ),
      m_image_cube_1x1_r8(
          renderer::Extent2D(1, 1),
          renderer::Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_R8,
          renderer::Image::FilterMode::TEXTURE_FILTER_NEAREST,
          nullptr
      ),
      m_sampler_linear(
          renderer::Image::FilterMode::TEXTURE_FILTER_LINEAR,
          renderer::Image::WrapMode::TEXTURE_WRAP_REPEAT
      ),
      m_sampler_nearest(
          renderer::Image::FilterMode::TEXTURE_FILTER_NEAREST,
          renderer::Image::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
      )
{
}

void PlaceholderData::Create(Engine *engine)
{
    auto *device = engine->GetDevice();

    HYPERION_ASSERT_RESULT(m_image_2d_1x1_r8.Create(device));
    HYPERION_ASSERT_RESULT(m_image_view_2d_1x1_r8.Create(device, &m_image_2d_1x1_r8));
    HYPERION_ASSERT_RESULT(m_image_3d_1x1x1_r8.Create(device));
    HYPERION_ASSERT_RESULT(m_image_view_3d_1x1x1_r8.Create(device, &m_image_3d_1x1x1_r8));
    HYPERION_ASSERT_RESULT(m_image_3d_1x1x1_r8_storage.Create(device));
    HYPERION_ASSERT_RESULT(m_image_view_3d_1x1x1_r8_storage.Create(device, &m_image_3d_1x1x1_r8_storage));
    HYPERION_ASSERT_RESULT(m_image_cube_1x1_r8.Create(device));
    HYPERION_ASSERT_RESULT(m_image_view_cube_1x1_r8.Create(device, &m_image_cube_1x1_r8));
    HYPERION_ASSERT_RESULT(m_sampler_linear.Create(device));
    HYPERION_ASSERT_RESULT(m_sampler_nearest.Create(device));
}

void PlaceholderData::Destroy(Engine *engine)
{
    auto *device = engine->GetDevice();

    HYPERION_ASSERT_RESULT(m_image_2d_1x1_r8.Destroy(device));
    HYPERION_ASSERT_RESULT(m_image_view_2d_1x1_r8.Destroy(device));
    HYPERION_ASSERT_RESULT(m_image_3d_1x1x1_r8.Destroy(device));
    HYPERION_ASSERT_RESULT(m_image_view_3d_1x1x1_r8.Destroy(device));
    HYPERION_ASSERT_RESULT(m_image_3d_1x1x1_r8_storage.Destroy(device));
    HYPERION_ASSERT_RESULT(m_image_view_3d_1x1x1_r8_storage.Destroy(device));
    HYPERION_ASSERT_RESULT(m_image_cube_1x1_r8.Destroy(device));
    HYPERION_ASSERT_RESULT(m_image_view_cube_1x1_r8.Destroy(device));
    HYPERION_ASSERT_RESULT(m_sampler_linear.Destroy(device));
    HYPERION_ASSERT_RESULT(m_sampler_nearest.Destroy(device));

    for (auto &it : m_buffers) {
        DebugLog(
            LogType::Debug,
            "Destroy %llu placeholder GPUBuffer objects of type id: %u\n",
            it.second.Size(),
            it.first.value
        );

        for (auto &buffer_map_it : it.second) {
            AssertThrow(buffer_map_it.second != nullptr);
            HYPERION_ASSERT_RESULT(buffer_map_it.second->Destroy(device));
        }
    }

    m_empty_mesh.Reset();
}

Ref<Mesh> PlaceholderData::GetEmptyMesh(Engine *engine)
{
    if (m_empty_mesh == nullptr) {
        m_empty_mesh = engine->resources.meshes.Add(std::make_unique<Mesh>());
        m_empty_mesh.Init();
    }

    return m_empty_mesh.IncRef();
}


} // namespace hyperion::v2
