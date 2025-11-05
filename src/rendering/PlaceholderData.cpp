/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/Texture.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/TextureAsset.hpp>

#include <core/math/Vector2.hpp>

#include <util/img/Bitmap.hpp>

#include <engine/EngineGlobals.hpp>
#include <core/Types.hpp>

namespace hyperion {

template <TextureFormat Format>
HYP_API void FillPlaceholderBuffer_Tex2D(Vec2u dimensions, ByteBuffer& outBuffer)
{
    using Helper = TextureFormatHelper<Format>;

    auto bitmap = Bitmap<Format>(dimensions.x, dimensions.y);

    // checkerboard pattern
    for (uint32 y = 0; y < dimensions.y; y++)
    {
        for (uint32 x = 0; x < dimensions.x; x++)
        {
            const bool isColor = ((x / 16) % 2) == ((y / 16) % 2);

            bitmap.SetPixel(x, y, { isColor ? 1.0f : 0.0f,
                                    0.0f,
                                    isColor ? 1.0f : 0.0f,
                                    1.0f });
        }
    }

    if constexpr (Helper::isFloatType)
    {
        outBuffer = ByteBuffer(bitmap.GetUnpackedFloats().ToByteView());
    }
    else
    {
        outBuffer = bitmap.GetUnpackedBytes(Helper::bytesPerComponent * Helper::numComponents);
    }
}

template <TextureFormat Format>
HYP_API void FillPlaceholderBuffer_Cubemap(Vec2u dimensions, ByteBuffer& outBuffer)
{
    using Helper = TextureFormatHelper<Format>;
    static_assert(!Helper::isFloatType, "FillPlaceholderBuffer_Cubemap not implemented for floating point type textures");

    auto bitmap = Bitmap<Format>(dimensions.x, dimensions.y);

    // checkerboard pattern
    for (uint32 y = 0; y < dimensions.y; y++)
    {
        for (uint32 x = 0; x < dimensions.x; x++)
        {
            const bool isColor = ((x / 16) % 2) == ((y / 16) % 2);

            bitmap.SetPixel(x, y, { isColor ? 1.0f : 0.0f,
                                    0.0f,
                                    isColor ? 1.0f : 0.0f,
                                    1.0f });
        }
    }

    ByteBuffer faceByteBuffer = bitmap.GetUnpackedBytes(Helper::bytesPerComponent * Helper::numComponents);

    outBuffer.SetSize(faceByteBuffer.Size() * 6);

    for (uint32 i = 0; i < 6; i++)
    {
        outBuffer.Write(faceByteBuffer.Size(), i * faceByteBuffer.Size(), faceByteBuffer.Data());
    }
}

template HYP_API void FillPlaceholderBuffer_Tex2D<TF_R8>(Vec2u dimensions, ByteBuffer& outBuffer);      // R8
template HYP_API void FillPlaceholderBuffer_Tex2D<TF_RGBA8>(Vec2u dimensions, ByteBuffer& outBuffer);   // RGBA8
template HYP_API void FillPlaceholderBuffer_Tex2D<TF_RGBA16F>(Vec2u dimensions, ByteBuffer& outBuffer); // RGBA16F
template HYP_API void FillPlaceholderBuffer_Tex2D<TF_RGBA32F>(Vec2u dimensions, ByteBuffer& outBuffer); // RGBA32F

template HYP_API void FillPlaceholderBuffer_Cubemap<TF_R8>(Vec2u dimensions, ByteBuffer& outBuffer);    // R8
template HYP_API void FillPlaceholderBuffer_Cubemap<TF_RGBA8>(Vec2u dimensions, ByteBuffer& outBuffer); // RGBA8

#pragma region PlaceholderData

PlaceholderData::PlaceholderData()
    : m_image2d1x1R8(g_renderBackend->MakeImage(TextureDesc {
          TT_TEX2D,
          TF_R8,
          Vec3u::One(),
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE,
          1,
          IU_SAMPLED })),
      m_imageView2d1x1R8(g_renderBackend->MakeImageView(m_image2d1x1R8)),
      m_image2d1x1R8Storage(g_renderBackend->MakeImage(TextureDesc {
          TT_TEX2D,
          TF_R8,
          Vec3u::One(),
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE,
          1,
          IU_STORAGE | IU_SAMPLED })),
      m_imageView2d1x1R8Storage(g_renderBackend->MakeImageView(m_image2d1x1R8Storage)),
      m_image3d1x1x1R8(g_renderBackend->MakeImage(TextureDesc {
          TT_TEX3D,
          TF_R8,
          Vec3u::One(),
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE,
          1,
          IU_SAMPLED })),
      m_imageView3d1x1x1R8(g_renderBackend->MakeImageView(m_image3d1x1x1R8)),
      m_image3d1x1x1R8Storage(g_renderBackend->MakeImage(TextureDesc {
          TT_TEX3D,
          TF_R8,
          Vec3u::One(),
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE,
          1,
          IU_STORAGE | IU_SAMPLED })),
      m_imageView3d1x1x1R8Storage(g_renderBackend->MakeImageView(m_image3d1x1x1R8Storage)),
      m_imageCube1x1R8(g_renderBackend->MakeImage(TextureDesc {
          TT_CUBEMAP,
          TF_R8,
          Vec3u::One(),
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE,
          1,
          IU_SAMPLED })),
      m_imageViewCube1x1R8(g_renderBackend->MakeImageView(m_imageCube1x1R8)),
      m_image2d1x1R8Array(g_renderBackend->MakeImage(TextureDesc {
          TT_TEX2D_ARRAY,
          TF_R8,
          Vec3u::One(),
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE,
          1,
          IU_SAMPLED })),
      m_imageView2d1x1R8Array(g_renderBackend->MakeImageView(m_image2d1x1R8Array)),
      m_imageCube1x1R8Array(g_renderBackend->MakeImage(TextureDesc {
          TT_CUBEMAP_ARRAY,
          TF_R8,
          Vec3u::One(),
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE,
          1,
          IU_SAMPLED })),
      m_imageViewCube1x1R8Array(g_renderBackend->MakeImageView(m_imageCube1x1R8Array)),
      m_samplerLinear(g_renderBackend->MakeSampler(
          TFM_LINEAR,
          TFM_LINEAR,
          TWM_REPEAT)),
      m_samplerLinearMipmap(g_renderBackend->MakeSampler(
          TFM_LINEAR_MIPMAP,
          TFM_LINEAR,
          TWM_REPEAT)),
      m_samplerNearest(g_renderBackend->MakeSampler(
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
    m_image2d1x1R8->SetDebugName(NAME("Placeholder_2D_1x1_R8"));
    DeferCreate(m_image2d1x1R8);

    m_imageView2d1x1R8->SetDebugName(NAME("Placeholder_2D_1x1_R8_View"));
    DeferCreate(m_imageView2d1x1R8);

    m_image2d1x1R8Storage->SetDebugName(NAME("Placeholder_2D_1x1_R8_Storage"));
    DeferCreate(m_image2d1x1R8Storage);

    m_imageView2d1x1R8Storage->SetDebugName(NAME("Placeholder_2D_1x1_R8_Storage_View"));
    DeferCreate(m_imageView2d1x1R8Storage);

    m_image3d1x1x1R8->SetDebugName(NAME("Placeholder_3D_1x1x1_R8"));
    DeferCreate(m_image3d1x1x1R8);

    m_imageView3d1x1x1R8->SetDebugName(NAME("Placeholder_3D_1x1x1_R8_View"));
    DeferCreate(m_imageView3d1x1x1R8);

    m_image3d1x1x1R8Storage->SetDebugName(NAME("Placeholder_3D_1x1x1_R8_Storage"));
    DeferCreate(m_image3d1x1x1R8Storage);

    m_imageView3d1x1x1R8Storage->SetDebugName(NAME("Placeholder_3D_1x1x1_R8_Storage_View"));
    DeferCreate(m_imageView3d1x1x1R8Storage);

    m_imageCube1x1R8->SetDebugName(NAME("Placeholder_Cube_1x1_R8"));
    DeferCreate(m_imageCube1x1R8);

    m_imageViewCube1x1R8->SetDebugName(NAME("Placeholder_Cube_1x1_R8_View"));
    DeferCreate(m_imageViewCube1x1R8);

    m_image2d1x1R8Array->SetDebugName(NAME("Placeholder_2D_1x1_R8_Array"));
    DeferCreate(m_image2d1x1R8Array);

    m_imageView2d1x1R8Array->SetDebugName(NAME("Placeholder_2D_1x1_R8_Array_View"));
    DeferCreate(m_imageView2d1x1R8Array);

    m_imageCube1x1R8Array->SetDebugName(NAME("Placeholder_Cube_1x1_R8_Array"));
    DeferCreate(m_imageCube1x1R8Array);

    m_imageViewCube1x1R8Array->SetDebugName(NAME("Placeholder_Cube_1x1_R8_Array_View"));
    DeferCreate(m_imageViewCube1x1R8Array);

#pragma endregion Image and ImageView

#pragma region Textures
    using PlaceholderBufferData = Pair<ByteBuffer, bool>;
    auto initBufferData = []<class... Args>(PlaceholderBufferData& bufferData, auto fillFn, Args&&... args)
    {
        if (!bufferData.second)
        {
            fillFn(std::forward<Args>(args)..., bufferData.first);
            bufferData.second = true;
        }
    };

    auto loadOrInitTexture = [&initBufferData]<class... Args>(Handle<Texture>& texture, const String& path, const UTF8StringView& name, const TextureDesc& textureDesc, PlaceholderBufferData& bufferData, auto fillFn, Args&&... args)
    {
        if (Handle<AssetObject> asset = g_assetManager->GetAssetRegistry()->GetAssetFromPath(path + "/" + name); asset.IsValid())
        {
            texture = ObjCast<Texture>(asset);
            Assert(texture != nullptr);
        }
        else
        {
            initBufferData(bufferData, fillFn, std::forward<Args>(args)...);

            texture = CreateObject<Texture>(textureDesc, TextureData { bufferData.first });
            texture->SetName(CreateNameFromDynamicString(*name));

            g_assetManager->GetAssetRegistry()->RegisterAsset(path, texture->GetAsset());

            InitObject(texture);

            texture->GetAsset()->SetIsPersistentlyLoaded(true);
        }
    };

    PlaceholderBufferData placeholderBufferTex2dR8 {};
    PlaceholderBufferData placeholderBufferCubemapR8 {};

    loadOrInitTexture(
        defaultTexture2d,
        "Engine/Media/Textures",
        "Placeholder_Texture_2D_1x1_R8",
        TextureDesc {
            TT_TEX2D,
            TF_R8,
            Vec3u::One(),
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_SAMPLED | IU_STORAGE },
        placeholderBufferTex2dR8,
        &FillPlaceholderBuffer_Tex2D<TF_R8>,
        Vec2u::One());

    loadOrInitTexture(
        defaultTexture3d,
        "Engine/Media/Textures",
        "Placeholder_Texture_3D_1x1x1_R8",
        TextureDesc {
            TT_TEX3D,
            TF_R8,
            Vec3u::One(),
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_SAMPLED | IU_STORAGE },
        placeholderBufferTex2dR8,
        &FillPlaceholderBuffer_Tex2D<TF_R8>,
        Vec2u::One());

    loadOrInitTexture(
        defaultCubemap,
        "Engine/Media/Textures",
        "Placeholder_Texture_Cube_1x1_R8",
        TextureDesc {
            TT_CUBEMAP,
            TF_R8,
            Vec3u::One(),
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_SAMPLED | IU_STORAGE },
        placeholderBufferCubemapR8,
        &FillPlaceholderBuffer_Cubemap<TF_R8>,
        Vec2u::One());

    loadOrInitTexture(
        defaultTexture2dArray,
        "Engine/Media/Textures",
        "Placeholder_Texture_2D_1x1_R8_Array",
        TextureDesc {
            TT_TEX2D_ARRAY,
            TF_R8,
            Vec3u::One(),
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_SAMPLED | IU_STORAGE },
        placeholderBufferTex2dR8,
        &FillPlaceholderBuffer_Tex2D<TF_R8>,
        Vec2u::One());

    loadOrInitTexture(
        defaultCubemapArray,
        "Engine/Media/Textures",
        "Placeholder_Texture_Cube_1x1_R8_Array",
        TextureDesc {
            TT_CUBEMAP_ARRAY,
            TF_R8,
            Vec3u::One(),
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_SAMPLED | IU_STORAGE },
        placeholderBufferCubemapR8,
        &FillPlaceholderBuffer_Cubemap<TF_R8>,
        Vec2u::One());

#pragma endregion Textures

#pragma region Samplers

    m_samplerLinear->SetDebugName(NAME("Placeholder_Sampler_Linear"));
    DeferCreate(m_samplerLinear);

    m_samplerLinearMipmap->SetDebugName(NAME("Placeholder_Sampler_Linear_Mipmap"));
    DeferCreate(m_samplerLinearMipmap);

    m_samplerNearest->SetDebugName(NAME("Placeholder_Sampler_Nearest"));
    DeferCreate(m_samplerNearest);

#pragma endregion Samplers
}

void PlaceholderData::Destroy()
{
    SafeDelete(std::move(m_image2d1x1R8));
    SafeDelete(std::move(m_imageView2d1x1R8));
    SafeDelete(std::move(m_image2d1x1R8Storage));
    SafeDelete(std::move(m_imageView2d1x1R8Storage));
    SafeDelete(std::move(m_image3d1x1x1R8));
    SafeDelete(std::move(m_imageView3d1x1x1R8));
    SafeDelete(std::move(m_image3d1x1x1R8Storage));
    SafeDelete(std::move(m_imageView3d1x1x1R8Storage));
    SafeDelete(std::move(m_imageCube1x1R8));
    SafeDelete(std::move(m_imageViewCube1x1R8));
    SafeDelete(std::move(m_image2d1x1R8Array));
    SafeDelete(std::move(m_imageView2d1x1R8Array));
    SafeDelete(std::move(m_imageCube1x1R8Array));
    SafeDelete(std::move(m_imageViewCube1x1R8Array));
    SafeDelete(std::move(m_samplerLinear));
    SafeDelete(std::move(m_samplerLinearMipmap));
    SafeDelete(std::move(m_samplerNearest));

    for (auto& bufferMap : m_buffers)
    {
        for (auto& it : bufferMap.second)
        {
            SafeDelete(std::move(it.second));
        }
    }

    m_buffers.Clear();
}

GpuBufferRef PlaceholderData::CreateGpuBuffer(GpuBufferType bufferType, SizeType size)
{
    GpuBufferRef gpuBuffer = g_renderBackend->MakeGpuBuffer(bufferType, size);
    gpuBuffer->SetDebugName(NAME("Placeholder_GpuBuffer"));
    HYP_GFX_ASSERT(gpuBuffer->Create());

    return gpuBuffer;
}

#pragma endregion PlaceholderData

} // namespace hyperion
