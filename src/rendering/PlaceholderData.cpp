/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/PlaceholderData.hpp>

#include <Engine.hpp>

namespace hyperion {
PlaceholderData::PlaceholderData()
    : m_image_2d_1x1_r8(g_rendering_api->MakeImage(TextureDesc {
        renderer::ImageType::TEXTURE_TYPE_2D,
        renderer::InternalFormat::R8,
        Vec3u::One(),
        renderer::FilterMode::TEXTURE_FILTER_NEAREST,
        renderer::FilterMode::TEXTURE_FILTER_NEAREST,
        renderer::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        1,
        ImageFormatCapabilities::SAMPLED
      })),
      m_image_view_2d_1x1_r8(g_rendering_api->MakeImageView(m_image_2d_1x1_r8)),
      m_image_2d_1x1_r8_storage(g_rendering_api->MakeImage(TextureDesc {
        renderer::ImageType::TEXTURE_TYPE_2D,
        renderer::InternalFormat::R8,
        Vec3u::One(),
        renderer::FilterMode::TEXTURE_FILTER_NEAREST,
        renderer::FilterMode::TEXTURE_FILTER_NEAREST,
        renderer::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        1,
        ImageFormatCapabilities::STORAGE | ImageFormatCapabilities::SAMPLED
      })),
      m_image_view_2d_1x1_r8_storage(g_rendering_api->MakeImageView(m_image_2d_1x1_r8_storage)),
      m_image_3d_1x1x1_r8(g_rendering_api->MakeImage(TextureDesc {
        renderer::ImageType::TEXTURE_TYPE_3D,
        renderer::InternalFormat::R8,
        Vec3u::One(),
        renderer::FilterMode::TEXTURE_FILTER_NEAREST,
        renderer::FilterMode::TEXTURE_FILTER_NEAREST,
        renderer::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        1,
        ImageFormatCapabilities::SAMPLED
      })),
      m_image_view_3d_1x1x1_r8(g_rendering_api->MakeImageView(m_image_3d_1x1x1_r8)),
      m_image_3d_1x1x1_r8_storage(g_rendering_api->MakeImage(TextureDesc {
        renderer::ImageType::TEXTURE_TYPE_3D,
        renderer::InternalFormat::R8,
        Vec3u::One(),
        renderer::FilterMode::TEXTURE_FILTER_NEAREST,
        renderer::FilterMode::TEXTURE_FILTER_NEAREST,
        renderer::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        1,
        ImageFormatCapabilities::STORAGE | ImageFormatCapabilities::SAMPLED
      })),
      m_image_view_3d_1x1x1_r8_storage(g_rendering_api->MakeImageView(m_image_3d_1x1x1_r8_storage)),
      m_image_cube_1x1_r8(g_rendering_api->MakeImage(TextureDesc {
        renderer::ImageType::TEXTURE_TYPE_CUBEMAP,
        renderer::InternalFormat::R8,
        Vec3u::One(),
        renderer::FilterMode::TEXTURE_FILTER_NEAREST,
        renderer::FilterMode::TEXTURE_FILTER_NEAREST,
        renderer::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        1,
        ImageFormatCapabilities::SAMPLED
      })),
      m_image_view_cube_1x1_r8(g_rendering_api->MakeImageView(m_image_cube_1x1_r8)),
      m_image_2d_1x1_r8_array(g_rendering_api->MakeImage(TextureDesc {
          renderer::ImageType::TEXTURE_TYPE_2D_ARRAY,
          renderer::InternalFormat::R8,
          Vec3u::One(),
          renderer::FilterMode::TEXTURE_FILTER_NEAREST,
          renderer::FilterMode::TEXTURE_FILTER_NEAREST,
          renderer::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
          1,
          ImageFormatCapabilities::SAMPLED
      })),
      m_image_view_2d_1x1_r8_array(g_rendering_api->MakeImageView(m_image_2d_1x1_r8_array)),
      m_sampler_linear(g_rendering_api->MakeSampler(
          renderer::FilterMode::TEXTURE_FILTER_LINEAR,
          renderer::FilterMode::TEXTURE_FILTER_LINEAR,
          renderer::WrapMode::TEXTURE_WRAP_REPEAT
      )),
      m_sampler_linear_mipmap(g_rendering_api->MakeSampler(
          renderer::FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
          renderer::FilterMode::TEXTURE_FILTER_LINEAR,
          renderer::WrapMode::TEXTURE_WRAP_REPEAT
      )),
      m_sampler_nearest(g_rendering_api->MakeSampler(
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
    DeferCreate(m_image_view_2d_1x1_r8);

    m_image_2d_1x1_r8_storage.SetName(NAME("Placeholder_2D_1x1_R8_Storage"));
    DeferCreate(m_image_2d_1x1_r8_storage);

    m_image_view_2d_1x1_r8_storage.SetName(NAME("Placeholder_2D_1x1_R8_Storage_View"));
    DeferCreate(m_image_view_2d_1x1_r8_storage);

    m_image_3d_1x1x1_r8.SetName(NAME("Placeholder_3D_1x1x1_R8"));
    DeferCreate(m_image_3d_1x1x1_r8);

    m_image_view_3d_1x1x1_r8.SetName(NAME("Placeholder_3D_1x1x1_R8_View"));
    DeferCreate(m_image_view_3d_1x1x1_r8);

    m_image_3d_1x1x1_r8_storage.SetName(NAME("Placeholder_3D_1x1x1_R8_Storage"));
    DeferCreate(m_image_3d_1x1x1_r8_storage);

    m_image_view_3d_1x1x1_r8_storage.SetName(NAME("Placeholder_3D_1x1x1_R8_Storage_View"));
    DeferCreate(m_image_view_3d_1x1x1_r8_storage);

    m_image_cube_1x1_r8.SetName(NAME("Placeholder_Cube_1x1_R8"));
    DeferCreate(m_image_cube_1x1_r8);

    m_image_view_cube_1x1_r8.SetName(NAME("Placeholder_Cube_1x1_R8_View"));
    DeferCreate(m_image_view_cube_1x1_r8);

    m_image_2d_1x1_r8_array.SetName(NAME("Placeholder_2D_1x1_R8_Array"));
    DeferCreate(m_image_2d_1x1_r8_array);

    m_image_view_2d_1x1_r8_array.SetName(NAME("Placeholder_2D_1x1_R8_Array_View"));
    DeferCreate(m_image_view_2d_1x1_r8_array);

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
    SafeRelease(std::move(m_image_2d_1x1_r8_array));
    SafeRelease(std::move(m_image_view_2d_1x1_r8_array));
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

GPUBufferRef PlaceholderData::CreateGPUBuffer(GPUBufferType buffer_type, SizeType size)
{
    GPUBufferRef gpu_buffer = g_rendering_api->MakeGPUBuffer(buffer_type, size);
    HYPERION_ASSERT_RESULT(gpu_buffer->Create());

    return gpu_buffer;
}

} // namespace hyperion
