/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/PlaceholderData.hpp>

#include <Engine.hpp>

namespace hyperion {
PlaceholderData::PlaceholderData()
    : m_image_2d_1x1_r8(MakeRenderObject<Image>(TextureDesc {
        renderer::ImageType::TEXTURE_TYPE_2D,
        renderer::InternalFormat::R8,
        Vec3u::One(),
        renderer::FilterMode::TEXTURE_FILTER_NEAREST,
        renderer::FilterMode::TEXTURE_FILTER_NEAREST,
        renderer::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        1,
        ImageFormatCapabilities::SAMPLED
      })),
      m_image_view_2d_1x1_r8(MakeRenderObject<ImageView>()),
      m_image_2d_1x1_r8_storage(MakeRenderObject<Image>(TextureDesc {
        renderer::ImageType::TEXTURE_TYPE_2D,
        renderer::InternalFormat::R8,
        Vec3u::One(),
        renderer::FilterMode::TEXTURE_FILTER_NEAREST,
        renderer::FilterMode::TEXTURE_FILTER_NEAREST,
        renderer::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        1,
        ImageFormatCapabilities::STORAGE | ImageFormatCapabilities::SAMPLED
      })),
      m_image_view_2d_1x1_r8_storage(MakeRenderObject<ImageView>()),
      m_image_3d_1x1x1_r8(MakeRenderObject<Image>(TextureDesc {
        renderer::ImageType::TEXTURE_TYPE_3D,
        renderer::InternalFormat::R8,
        Vec3u::One(),
        renderer::FilterMode::TEXTURE_FILTER_NEAREST,
        renderer::FilterMode::TEXTURE_FILTER_NEAREST,
        renderer::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        1,
        ImageFormatCapabilities::SAMPLED
      })),
      m_image_view_3d_1x1x1_r8(MakeRenderObject<ImageView>()),
      m_image_3d_1x1x1_r8_storage(MakeRenderObject<Image>(TextureDesc {
        renderer::ImageType::TEXTURE_TYPE_3D,
        renderer::InternalFormat::R8,
        Vec3u::One(),
        renderer::FilterMode::TEXTURE_FILTER_NEAREST,
        renderer::FilterMode::TEXTURE_FILTER_NEAREST,
        renderer::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        1,
        ImageFormatCapabilities::STORAGE | ImageFormatCapabilities::SAMPLED
      })),
      m_image_view_3d_1x1x1_r8_storage(MakeRenderObject<ImageView>()),
      m_image_cube_1x1_r8(MakeRenderObject<Image>(TextureDesc {
        renderer::ImageType::TEXTURE_TYPE_CUBEMAP,
        renderer::InternalFormat::R8,
        Vec3u::One(),
        renderer::FilterMode::TEXTURE_FILTER_NEAREST,
        renderer::FilterMode::TEXTURE_FILTER_NEAREST,
        renderer::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        1,
        ImageFormatCapabilities::SAMPLED
      })),
      m_image_view_cube_1x1_r8(MakeRenderObject<ImageView>()),
      m_sampler_linear(MakeRenderObject<Sampler>(
          renderer::FilterMode::TEXTURE_FILTER_LINEAR,
          renderer::FilterMode::TEXTURE_FILTER_LINEAR,
          renderer::WrapMode::TEXTURE_WRAP_REPEAT
      )),
      m_sampler_linear_mipmap(MakeRenderObject<Sampler>(
          renderer::FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
          renderer::FilterMode::TEXTURE_FILTER_LINEAR,
          renderer::WrapMode::TEXTURE_WRAP_REPEAT
      )),
      m_sampler_nearest(MakeRenderObject<Sampler>(
          renderer::FilterMode::TEXTURE_FILTER_NEAREST,
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
    m_image_2d_1x1_r8.SetName(NAME("Placeholder_2D_1x1_R8"));
    DeferCreate(m_image_2d_1x1_r8);

    m_image_view_2d_1x1_r8.SetName(NAME("Placeholder_2D_1x1_R8_View"));
    DeferCreate(m_image_view_2d_1x1_r8, m_image_2d_1x1_r8);

    m_image_2d_1x1_r8_storage.SetName(NAME("Placeholder_2D_1x1_R8_Storage"));
    DeferCreate(m_image_2d_1x1_r8_storage);

    m_image_view_2d_1x1_r8_storage.SetName(NAME("Placeholder_2D_1x1_R8_Storage_View"));
    DeferCreate(m_image_view_2d_1x1_r8_storage, m_image_2d_1x1_r8_storage);

    m_image_3d_1x1x1_r8.SetName(NAME("Placeholder_3D_1x1x1_R8"));
    DeferCreate(m_image_3d_1x1x1_r8);

    m_image_view_3d_1x1x1_r8.SetName(NAME("Placeholder_3D_1x1x1_R8_View"));
    DeferCreate(m_image_view_3d_1x1x1_r8, m_image_3d_1x1x1_r8);

    m_image_3d_1x1x1_r8_storage.SetName(NAME("Placeholder_3D_1x1x1_R8_Storage"));
    DeferCreate(m_image_3d_1x1x1_r8_storage);

    m_image_view_3d_1x1x1_r8_storage.SetName(NAME("Placeholder_3D_1x1x1_R8_Storage_View"));
    DeferCreate(m_image_view_3d_1x1x1_r8_storage, m_image_3d_1x1x1_r8_storage);

    m_image_cube_1x1_r8.SetName(NAME("Placeholder_Cube_1x1_R8"));
    DeferCreate(m_image_cube_1x1_r8);

    m_image_view_cube_1x1_r8.SetName(NAME("Placeholder_Cube_1x1_R8_View"));
    DeferCreate(m_image_view_cube_1x1_r8, m_image_cube_1x1_r8);

    m_sampler_linear.SetName(NAME("Placeholder_Sampler_Linear"));
    DeferCreate(m_sampler_linear);

    m_sampler_linear_mipmap.SetName(NAME("Placeholder_Sampler_Linear_Mipmap"));
    DeferCreate(m_sampler_linear_mipmap);

    m_sampler_nearest.SetName(NAME("Placeholder_Sampler_Nearest"));
    DeferCreate(m_sampler_nearest);
}

void PlaceholderData::Destroy()
{
    SafeRelease(std::move(m_image_2d_1x1_r8));
    SafeRelease(std::move(m_image_view_2d_1x1_r8));
    SafeRelease(std::move(m_image_2d_1x1_r8_storage));
    SafeRelease(std::move(m_image_view_2d_1x1_r8_storage));
    SafeRelease(std::move(m_image_3d_1x1x1_r8));
    SafeRelease(std::move(m_image_view_3d_1x1x1_r8));
    SafeRelease(std::move(m_image_3d_1x1x1_r8_storage));
    SafeRelease(std::move(m_image_view_3d_1x1x1_r8_storage));
    SafeRelease(std::move(m_image_cube_1x1_r8));
    SafeRelease(std::move(m_image_view_cube_1x1_r8));
    SafeRelease(std::move(m_sampler_linear));
    SafeRelease(std::move(m_sampler_linear_mipmap));
    SafeRelease(std::move(m_sampler_nearest));

    for (auto &buffer_map : m_buffers) {
        for (auto &it : buffer_map.second) {
            SafeRelease(std::move(it.second));
        }
    }

    m_buffers.Clear();
}

} // namespace hyperion
