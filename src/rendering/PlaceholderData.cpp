/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/PlaceholderData.hpp>

#include <core/math/Vector2.hpp>

#include <scene/Texture.hpp>

#include <util/img/Bitmap.hpp>

#include <Types.hpp>
#include <Engine.hpp>

namespace hyperion {

template <TextureFormat Format>
struct TextureFormatHelper
{
    static constexpr uint32 num_components = NumComponents(Format);
    static constexpr uint32 num_bytes = NumBytes(Format);
    static constexpr bool is_float_type = uint32(Format) >= TF_RGBA16F && uint32(Format) <= TF_RGBA32F;

    using ElementType = std::conditional_t<is_float_type, float, ubyte>;
};

template <TextureFormat Format>
HYP_API void FillPlaceholderBuffer_Tex2D(Vec2u dimensions, ByteBuffer& out_buffer)
{
    using Helper = TextureFormatHelper<Format>;

    auto bitmap = Bitmap<Helper::num_components, typename Helper::ElementType>(dimensions.x, dimensions.y);

    // Set to default color to assist in debugging
    for (uint32 y = 0; y < dimensions.y; y++)
    {
        for (uint32 x = 0; x < dimensions.x; x++)
        {
            bitmap.SetPixel(x, y, { 0xFF, 0x00, 0xFF, 0xFF });
        }
    }

    if constexpr (Helper::is_float_type)
    {
        out_buffer = ByteBuffer(bitmap.GetUnpackedFloats().ToByteView());
    }
    else
    {
        out_buffer = bitmap.GetUnpackedBytes(Helper::num_bytes * Helper::num_components);
    }
}

template <TextureFormat Format>
HYP_API void FillPlaceholderBuffer_Cubemap(Vec2u dimensions, ByteBuffer& out_buffer)
{
    using Helper = TextureFormatHelper<Format>;
    static_assert(!Helper::is_float_type, "FillPlaceholderBuffer_Cubemap not implemented for floating point type textures");

    auto bitmap = Bitmap<Helper::num_components, typename Helper::ElementType>(dimensions.x, dimensions.y);

    // Set to default color to assist in debugging
    for (uint32 y = 0; y < dimensions.y; y++)
    {
        for (uint32 x = 0; x < dimensions.x; x++)
        {
            bitmap.SetPixel(x, y, { 0xFF, 0x00, 0xFF, 0xFF });
        }
    }

    ByteBuffer face_byte_buffer = bitmap.GetUnpackedBytes(Helper::num_bytes * Helper::num_components);

    out_buffer.SetSize(face_byte_buffer.Size() * 6);

    for (uint32 i = 0; i < 6; i++)
    {
        out_buffer.Write(face_byte_buffer.Size(), i * face_byte_buffer.Size(), face_byte_buffer.Data());
    }
}

template void FillPlaceholderBuffer_Tex2D<TF_R8>(Vec2u dimensions, ByteBuffer& out_buffer);      // R8
template void FillPlaceholderBuffer_Tex2D<TF_RGBA8>(Vec2u dimensions, ByteBuffer& out_buffer);   // RGBA8
template void FillPlaceholderBuffer_Tex2D<TF_RGBA16F>(Vec2u dimensions, ByteBuffer& out_buffer); // RGBA16F
template void FillPlaceholderBuffer_Tex2D<TF_RGBA32F>(Vec2u dimensions, ByteBuffer& out_buffer); // RGBA32F

template void FillPlaceholderBuffer_Cubemap<TF_R8>(Vec2u dimensions, ByteBuffer& out_buffer);    // R8
template void FillPlaceholderBuffer_Cubemap<TF_RGBA8>(Vec2u dimensions, ByteBuffer& out_buffer); // RGBA8

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
    ByteBuffer placeholder_buffer_tex2d_rgba8;
    FillPlaceholderBuffer_Tex2D<TF_RGBA8>(Vec2u::One(), placeholder_buffer_tex2d_rgba8);

    ByteBuffer placeholder_buffer_cubemap_rgba8;
    FillPlaceholderBuffer_Cubemap<TF_RGBA8>(Vec2u::One(), placeholder_buffer_cubemap_rgba8);

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
        placeholder_buffer_tex2d_rgba8 });

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
        placeholder_buffer_cubemap_rgba8 });

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
