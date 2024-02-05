#include "PlaceholderData.hpp"

#include <Engine.hpp>

namespace hyperion::v2 {
PlaceholderData::PlaceholderData()
    : m_image_2d_1x1_r8(MakeRenderObject<Image>(TextureImage2D(
          Extent2D(1, 1),
          renderer::InternalFormat::R8,
          renderer::FilterMode::TEXTURE_FILTER_NEAREST,
          nullptr
      ))),
      m_image_view_2d_1x1_r8(MakeRenderObject<ImageView>()),
      m_image_3d_1x1x1_r8(MakeRenderObject<Image>(TextureImage3D(
          Extent3D(1, 1, 1),
          renderer::InternalFormat::R8,
          renderer::FilterMode::TEXTURE_FILTER_NEAREST,
          nullptr
      ))),
      m_image_view_3d_1x1x1_r8(MakeRenderObject<ImageView>()),
      m_image_3d_1x1x1_r8_storage(MakeRenderObject<Image>(StorageImage(
          Extent3D(1, 1, 1),
          InternalFormat::R8,
          ImageType::TEXTURE_TYPE_3D,
          FilterMode::TEXTURE_FILTER_NEAREST
      ))),
      m_image_view_3d_1x1x1_r8_storage(MakeRenderObject<ImageView>()),
      m_image_cube_1x1_r8(MakeRenderObject<Image>(TextureImageCube(
          Extent2D(1, 1),
          renderer::InternalFormat::R8,
          renderer::FilterMode::TEXTURE_FILTER_NEAREST,
          nullptr
      ))),
      m_image_view_cube_1x1_r8(MakeRenderObject<ImageView>()),
      m_sampler_linear(MakeRenderObject<Sampler>(
          renderer::FilterMode::TEXTURE_FILTER_LINEAR,
          renderer::WrapMode::TEXTURE_WRAP_REPEAT
      )),
      m_sampler_linear_mipmap(MakeRenderObject<Sampler>(
          renderer::FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
          renderer::WrapMode::TEXTURE_WRAP_REPEAT
      )),
      m_sampler_nearest(MakeRenderObject<Sampler>(
          renderer::FilterMode::TEXTURE_FILTER_NEAREST,
          renderer::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
      ))
{
}

PlaceholderData::~PlaceholderData()
{
    DebugLog(LogType::Debug, "PlaceholderData destructor\n");
}

void PlaceholderData::Create()
{
    auto *device = g_engine->GetGPUDevice();

    DeferCreate(m_image_2d_1x1_r8, device);
    DeferCreate(m_image_view_2d_1x1_r8, device, m_image_2d_1x1_r8.Get());
    DeferCreate(m_image_3d_1x1x1_r8, device);
    DeferCreate(m_image_view_3d_1x1x1_r8, device, m_image_3d_1x1x1_r8.Get());
    DeferCreate(m_image_3d_1x1x1_r8_storage, device);
    DeferCreate(m_image_view_3d_1x1x1_r8_storage, device, m_image_3d_1x1x1_r8_storage.Get());
    DeferCreate(m_image_cube_1x1_r8, device);
    DeferCreate(m_image_view_cube_1x1_r8, device, m_image_cube_1x1_r8.Get());
    DeferCreate(m_sampler_linear, device);
    DeferCreate(m_sampler_linear_mipmap, device);
    DeferCreate(m_sampler_nearest, device);

    HYP_SYNC_RENDER(); // wait for all objects to be created

    AssertThrow(m_image_2d_1x1_r8.IsValid());
    AssertThrow(m_image_view_cube_1x1_r8.IsValid());
}

void PlaceholderData::Destroy()
{
    SafeRelease(std::move(m_image_2d_1x1_r8));
    SafeRelease(std::move(m_image_view_2d_1x1_r8));
    SafeRelease(std::move(m_image_3d_1x1x1_r8));
    SafeRelease(std::move(m_image_view_3d_1x1x1_r8));
    SafeRelease(std::move(m_image_3d_1x1x1_r8_storage));
    SafeRelease(std::move(m_image_view_3d_1x1x1_r8_storage));
    SafeRelease(std::move(m_image_cube_1x1_r8));
    SafeRelease(std::move(m_image_view_cube_1x1_r8));
    SafeRelease(std::move(m_sampler_linear));
    SafeRelease(std::move(m_sampler_linear_mipmap));
    SafeRelease(std::move(m_sampler_nearest));

    for (auto &it : m_buffers) {
        for (auto &buffer_map_it : it.second) {
            SafeRelease(std::move(buffer_map_it.second));
        }
    }
}

} // namespace hyperion::v2
