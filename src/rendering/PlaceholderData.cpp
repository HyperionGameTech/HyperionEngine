/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/PlaceholderData.hpp>

#include <core/math/Vector2.hpp>

#include <scene/Texture.hpp>

#include <util/img/Bitmap.hpp>

#include <Types.hpp>
#include <Engine.hpp>

namespace hyperion {

template <uint32 NumComponents>
static auto CreatePlaceholderBitmap(Vec2u dimensions)
{
    auto bitmap = Bitmap<NumComponents, ubyte>(dimensions.x, dimensions.y);

    // Set to default color to assist in debugging
    for (uint32 y = 0; y < dimensions.y; y++)
    {
        for (uint32 x = 0; x < dimensions.x; x++)
        {
            bitmap.SetPixel(x, y, { 0xFF, 0x00, 0xFF, 0xFF });
        }
    }

    return bitmap;
}

template <uint32 NumComponents, uint32 BytesPerPixel>
static ByteBuffer CreatePlaceholderCubemap(Vec2u dimensions)
{
    ByteBuffer byte_buffer;

    auto bitmap = Bitmap<NumComponents, ubyte>(dimensions.x, dimensions.y);

    // Set to default color to assist in debugging
    for (uint32 y = 0; y < dimensions.y; y++)
    {
        for (uint32 x = 0; x < dimensions.x; x++)
        {
            bitmap.SetPixel(x, y, { 0xFF, 0x00, 0xFF, 0xFF });
        }
    }

    ByteBuffer face_byte_buffer = bitmap.GetUnpackedBytes(BytesPerPixel);

    byte_buffer.SetSize(face_byte_buffer.Size() * 6);

    for (uint32 i = 0; i < 6; i++)
    {
        byte_buffer.Write(face_byte_buffer.Size(), i * face_byte_buffer.Size(), face_byte_buffer.Data());
    }

    return byte_buffer;
}

PlaceholderData::PlaceholderData()
    : m_image_2d_1x1_r8(g_render_backend->MakeImage(TextureDesc {
          TT_TEX2D,
          TF_R8,
          Vec3u::One(),
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE,
          1,
          IU_SAMPLED })),
      m_image_view_2d_1x1_r8(g_render_backend->MakeImageView(m_image_2d_1x1_r8)),
      m_image_2d_1x1_r8_storage(g_render_backend->MakeImage(TextureDesc {
          TT_TEX2D,
          TF_R8,
          Vec3u::One(),
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE,
          1,
          IU_STORAGE | IU_SAMPLED })),
      m_image_view_2d_1x1_r8_storage(g_render_backend->MakeImageView(m_image_2d_1x1_r8_storage)),
      m_image_3d_1x1x1_r8(g_render_backend->MakeImage(TextureDesc {
          TT_TEX3D,
          TF_R8,
          Vec3u::One(),
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE,
          1,
          IU_SAMPLED })),
      m_image_view_3d_1x1x1_r8(g_render_backend->MakeImageView(m_image_3d_1x1x1_r8)),
      m_image_3d_1x1x1_r8_storage(g_render_backend->MakeImage(TextureDesc {
          TT_TEX3D,
          TF_R8,
          Vec3u::One(),
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE,
          1,
          IU_STORAGE | IU_SAMPLED })),
      m_image_view_3d_1x1x1_r8_storage(g_render_backend->MakeImageView(m_image_3d_1x1x1_r8_storage)),
      m_image_cube_1x1_r8(g_render_backend->MakeImage(TextureDesc {
          TT_CUBEMAP,
          TF_R8,
          Vec3u::One(),
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE,
          1,
          IU_SAMPLED })),
      m_image_view_cube_1x1_r8(g_render_backend->MakeImageView(m_image_cube_1x1_r8)),
      m_image_2d_1x1_r8_array(g_render_backend->MakeImage(TextureDesc {
          TT_TEX2D_ARRAY,
          TF_R8,
          Vec3u::One(),
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE,
          1,
          IU_SAMPLED })),
      m_image_view_2d_1x1_r8_array(g_render_backend->MakeImageView(m_image_2d_1x1_r8_array)),
      m_image_cube_1x1_r8_array(g_render_backend->MakeImage(TextureDesc {
          TT_CUBEMAP_ARRAY,
          TF_R8,
          Vec3u::One(),
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE,
          1,
          IU_SAMPLED })),
      m_image_view_cube_1x1_r8_array(g_render_backend->MakeImageView(m_image_cube_1x1_r8_array)),
      m_sampler_linear(g_render_backend->MakeSampler(
          TFM_LINEAR,
          TFM_LINEAR,
          TWM_REPEAT)),
      m_sampler_linear_mipmap(g_render_backend->MakeSampler(
          TFM_LINEAR_MIPMAP,
          TFM_LINEAR,
          TWM_REPEAT)),
      m_sampler_nearest(g_render_backend->MakeSampler(
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE))
{
}

PlaceholderData::~PlaceholderData()
{
    DebugLog(LogType::Debug, "PlaceholderData destructor\n");
}

void PlaceholderData::Create()
{
#pragma region Image and ImageView
    // These will soon be deprecated (except the samplers) - we will instead use Texture instead of individual image/image view
    m_image_2d_1x1_r8->SetDebugName(NAME("Placeholder_2D_1x1_R8"));
    DeferCreate(m_image_2d_1x1_r8);

    m_image_view_2d_1x1_r8->SetDebugName(NAME("Placeholder_2D_1x1_R8_View"));
    DeferCreate(m_image_view_2d_1x1_r8);

    m_image_2d_1x1_r8_storage->SetDebugName(NAME("Placeholder_2D_1x1_R8_Storage"));
    DeferCreate(m_image_2d_1x1_r8_storage);

    m_image_view_2d_1x1_r8_storage->SetDebugName(NAME("Placeholder_2D_1x1_R8_Storage_View"));
    DeferCreate(m_image_view_2d_1x1_r8_storage);

    m_image_3d_1x1x1_r8->SetDebugName(NAME("Placeholder_3D_1x1x1_R8"));
    DeferCreate(m_image_3d_1x1x1_r8);

    m_image_view_3d_1x1x1_r8->SetDebugName(NAME("Placeholder_3D_1x1x1_R8_View"));
    DeferCreate(m_image_view_3d_1x1x1_r8);

    m_image_3d_1x1x1_r8_storage->SetDebugName(NAME("Placeholder_3D_1x1x1_R8_Storage"));
    DeferCreate(m_image_3d_1x1x1_r8_storage);

    m_image_view_3d_1x1x1_r8_storage->SetDebugName(NAME("Placeholder_3D_1x1x1_R8_Storage_View"));
    DeferCreate(m_image_view_3d_1x1x1_r8_storage);

    m_image_cube_1x1_r8->SetDebugName(NAME("Placeholder_Cube_1x1_R8"));
    DeferCreate(m_image_cube_1x1_r8);

    m_image_view_cube_1x1_r8->SetDebugName(NAME("Placeholder_Cube_1x1_R8_View"));
    DeferCreate(m_image_view_cube_1x1_r8);

    m_image_2d_1x1_r8_array->SetDebugName(NAME("Placeholder_2D_1x1_R8_Array"));
    DeferCreate(m_image_2d_1x1_r8_array);

    m_image_view_2d_1x1_r8_array->SetDebugName(NAME("Placeholder_2D_1x1_R8_Array_View"));
    DeferCreate(m_image_view_2d_1x1_r8_array);

    m_image_cube_1x1_r8_array->SetDebugName(NAME("Placeholder_Cube_1x1_R8_Array"));
    DeferCreate(m_image_cube_1x1_r8_array);

    m_image_view_cube_1x1_r8_array->SetDebugName(NAME("Placeholder_Cube_1x1_R8_Array_View"));
    DeferCreate(m_image_view_cube_1x1_r8_array);

#pragma endregion Image and ImageView

#pragma region Textures

    DefaultTexture2D = CreateObject<Texture>(TextureData {
        TextureDesc {
            TT_TEX2D,
            TF_RGBA8,
            Vec3u::One(),
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_SAMPLED | IU_STORAGE },
        CreatePlaceholderBitmap<4>(Vec2u::One()).GetUnpackedBytes(4) });

    DefaultTexture2D->SetName(NAME("Placeholder_Texture_2D_1x1_R8"));
    InitObject(DefaultTexture2D);
    DefaultTexture2D->SetPersistentRenderResourceEnabled(true);

    DefaultTexture3D = CreateObject<Texture>(TextureData {
        TextureDesc {
            TT_TEX3D,
            TF_R8,
            Vec3u::One(),
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_SAMPLED | IU_STORAGE } });

    DefaultTexture3D->SetName(NAME("Placeholder_Texture_3D_1x1x1_R8"));
    InitObject(DefaultTexture3D);
    DefaultTexture3D->SetPersistentRenderResourceEnabled(true);

    DefaultCubemap = CreateObject<Texture>(TextureData {
        TextureDesc {
            TT_CUBEMAP,
            TF_RGBA8,
            Vec3u::One(),
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_SAMPLED | IU_STORAGE },
        CreatePlaceholderCubemap<4, 4>(Vec2u::One()) });

    DefaultCubemap->SetName(NAME("Placeholder_Texture_Cube_1x1_R8"));
    InitObject(DefaultCubemap);
    DefaultCubemap->SetPersistentRenderResourceEnabled(true);

    DefaultTexture2DArray = CreateObject<Texture>(TextureData {
        TextureDesc {
            TT_TEX2D_ARRAY,
            TF_R8,
            Vec3u::One(),
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_SAMPLED | IU_STORAGE } });

    DefaultTexture2DArray->SetName(NAME("Placeholder_Texture_2D_1x1_R8_Array"));
    InitObject(DefaultTexture2DArray);
    DefaultTexture2DArray->SetPersistentRenderResourceEnabled(true);

    DefaultCubemapArray = CreateObject<Texture>(TextureData {
        TextureDesc {
            TT_CUBEMAP_ARRAY,
            TF_R8,
            Vec3u::One(),
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_SAMPLED | IU_STORAGE } });

    DefaultCubemapArray->SetName(NAME("Placeholder_Texture_Cube_1x1_R8_Array"));
    InitObject(DefaultCubemapArray);
    DefaultCubemapArray->SetPersistentRenderResourceEnabled(true);

#pragma endregion Textures

#pragma region Samplers

    m_sampler_linear->SetDebugName(NAME("Placeholder_Sampler_Linear"));
    DeferCreate(m_sampler_linear);

    m_sampler_linear_mipmap->SetDebugName(NAME("Placeholder_Sampler_Linear_Mipmap"));
    DeferCreate(m_sampler_linear_mipmap);

    m_sampler_nearest->SetDebugName(NAME("Placeholder_Sampler_Nearest"));
    DeferCreate(m_sampler_nearest);

#pragma endregion Samplers
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
    SafeRelease(std::move(m_image_cube_1x1_r8_array));
    SafeRelease(std::move(m_image_view_cube_1x1_r8_array));
    SafeRelease(std::move(m_sampler_linear));
    SafeRelease(std::move(m_sampler_linear_mipmap));
    SafeRelease(std::move(m_sampler_nearest));

    for (auto& buffer_map : m_buffers)
    {
        for (auto& it : buffer_map.second)
        {
            SafeRelease(std::move(it.second));
        }
    }

    m_buffers.Clear();
}

GpuBufferRef PlaceholderData::CreateGpuBuffer(GpuBufferType buffer_type, SizeType size)
{
    GpuBufferRef gpu_buffer = g_render_backend->MakeGpuBuffer(buffer_type, size);
    HYPERION_ASSERT_RESULT(gpu_buffer->Create());

    return gpu_buffer;
}

} // namespace hyperion
