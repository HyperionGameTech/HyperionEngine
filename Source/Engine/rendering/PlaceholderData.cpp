/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/Texture.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

#include <Core/math/Vector2.hpp>

#include <util/img/Bitmap.hpp>

namespace Hyperion {

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

            bitmap.SetPixel(x, y, { isColor ? 1.0f : 0.0f, 0.0f, isColor ? 1.0f : 0.0f, 1.0f });
        }
    }

    if constexpr (Helper::IsFloatingPoint)
    {
        outBuffer = ByteBuffer(bitmap.GetUnpackedFloats().ToByteView());
    }
    else
    {
        outBuffer = bitmap.GetUnpackedBytes(Helper::BytesPerComponent * Helper::NumComponents);
    }
}

template <TextureFormat Format>
HYP_API void FillPlaceholderBuffer_Cubemap(Vec2u dimensions, ByteBuffer& outBuffer)
{
    using Helper = TextureFormatHelper<Format>;
    static_assert(!Helper::IsFloatingPoint, "FillPlaceholderBuffer_Cubemap not implemented for floating point type textures");

    auto bitmap = Bitmap<Format>(dimensions.x, dimensions.y);

    // checkerboard pattern
    for (uint32 y = 0; y < dimensions.y; y++)
    {
        for (uint32 x = 0; x < dimensions.x; x++)
        {
            const bool isColor = ((x / 16) % 2) == ((y / 16) % 2);

            bitmap.SetPixel(x, y, { isColor ? 1.0f : 0.0f, 0.0f, isColor ? 1.0f : 0.0f, 1.0f });
        }
    }

    ByteBuffer faceByteBuffer = bitmap.GetUnpackedBytes(Helper::BytesPerComponent * Helper::NumComponents);

    outBuffer.SetSize(faceByteBuffer.Size() * 6);

    for (uint32 i = 0; i < 6; i++)
    {
        outBuffer.Write(faceByteBuffer.Size(), i * faceByteBuffer.Size(), faceByteBuffer.Data());
    }
}

template HYP_API void FillPlaceholderBuffer_Tex2D<TextureFormat::R8>(Vec2u dimensions, ByteBuffer& outBuffer);      // R8
template HYP_API void FillPlaceholderBuffer_Tex2D<TextureFormat::RGBA8>(Vec2u dimensions, ByteBuffer& outBuffer);   // RGBA8
template HYP_API void FillPlaceholderBuffer_Tex2D<TextureFormat::RGBA16F>(Vec2u dimensions, ByteBuffer& outBuffer); // RGBA16F
template HYP_API void FillPlaceholderBuffer_Tex2D<TextureFormat::RGBA32F>(Vec2u dimensions, ByteBuffer& outBuffer); // RGBA32F

template HYP_API void FillPlaceholderBuffer_Cubemap<TextureFormat::R8>(Vec2u dimensions, ByteBuffer& outBuffer);    // R8
template HYP_API void FillPlaceholderBuffer_Cubemap<TextureFormat::RGBA8>(Vec2u dimensions, ByteBuffer& outBuffer); // RGBA8

#pragma region PlaceholderData

PlaceholderData::PlaceholderData()
    : m_image2d1x1R8(g_renderInterface->MakeImage(TextureDesc {
          TextureType::Texture2D,
          TextureFormat::R8,
          Vec3u::One(),
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE,
          1,
          IU_SAMPLED })),
      m_imageView2d1x1R8(g_renderInterface->MakeImageView(m_image2d1x1R8)),
      m_image2d1x1R8Storage(g_renderInterface->MakeImage(TextureDesc {
          TextureType::Texture2D,
          TextureFormat::R8,
          Vec3u::One(),
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE,
          1,
          IU_STORAGE | IU_SAMPLED })),
      m_imageView2d1x1R8Storage(g_renderInterface->MakeImageView(m_image2d1x1R8Storage)),
      m_image3d1x1x1R8(g_renderInterface->MakeImage(TextureDesc {
          TextureType::Texture3D,
          TextureFormat::R8,
          Vec3u::One(),
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE,
          1,
          IU_SAMPLED })),
      m_imageView3d1x1x1R8(g_renderInterface->MakeImageView(m_image3d1x1x1R8)),
      m_image3d1x1x1R8Storage(g_renderInterface->MakeImage(TextureDesc {
          TextureType::Texture3D,
          TextureFormat::R8,
          Vec3u::One(),
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE,
          1,
          IU_STORAGE | IU_SAMPLED })),
      m_imageView3d1x1x1R8Storage(g_renderInterface->MakeImageView(m_image3d1x1x1R8Storage)),
      m_imageCube1x1R8(g_renderInterface->MakeImage(TextureDesc {
          TextureType::Cubemap,
          TextureFormat::RGBA8,
          Vec3u::One(),
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE,
          1,
          IU_SAMPLED })),
      m_imageViewCube1x1R8(g_renderInterface->MakeImageView(m_imageCube1x1R8)),
      m_image2d1x1R8Array(g_renderInterface->MakeImage(TextureDesc {
          TextureType::Texture2DArray,
          TextureFormat::R8,
          Vec3u::One(),
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE,
          1,
          IU_SAMPLED })),
      m_imageView2d1x1R8Array(g_renderInterface->MakeImageView(m_image2d1x1R8Array)),
      m_imageCube1x1R8Array(g_renderInterface->MakeImage(TextureDesc {
          TextureType::CubemapArray,
          TextureFormat::RGBA8,
          Vec3u::One(),
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE,
          1,
          IU_SAMPLED })),
      m_imageViewCube1x1R8Array(g_renderInterface->MakeImageView(m_imageCube1x1R8Array)),
      m_samplerLinear(g_renderInterface->MakeSampler(
          TFM_LINEAR,
          TFM_LINEAR,
          TWM_REPEAT)),
      m_samplerLinearMipmap(g_renderInterface->MakeSampler(
          TFM_LINEAR_MIPMAP,
          TFM_LINEAR,
          TWM_REPEAT)),
      m_samplerNearest(g_renderInterface->MakeSampler(
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE))
{
}

PlaceholderData::~PlaceholderData() = default;

void PlaceholderData::Initialize()
{
    AssertOnThread(g_renderThread);

#pragma region Image and ImageView
    // These will soon be deprecated (except the samplers) - we will instead use Texture instead of individual image/image view
    CheckResult(m_image2d1x1R8->Create());
    CheckResult(m_imageView2d1x1R8->Create());
    CheckResult(m_image2d1x1R8Storage->Create());
    CheckResult(m_imageView2d1x1R8Storage->Create());
    CheckResult(m_image3d1x1x1R8->Create());
    CheckResult(m_imageView3d1x1x1R8->Create());
    CheckResult(m_image3d1x1x1R8Storage->Create());
    CheckResult(m_imageView3d1x1x1R8Storage->Create());
    CheckResult(m_imageCube1x1R8->Create());
    CheckResult(m_imageViewCube1x1R8->Create());
    CheckResult(m_image2d1x1R8Array->Create());
    CheckResult(m_imageView2d1x1R8Array->Create());
    CheckResult(m_imageCube1x1R8Array->Create());
    CheckResult(m_imageViewCube1x1R8Array->Create());

#pragma endregion Image and ImageView

#pragma region Textures
    using PlaceholderBufferData = Pair<ByteBuffer, bool>;
    auto InitBufferData = []<class... Args>(PlaceholderBufferData& bufferData, auto fillFn, Args&&... args)
    {
        if (!bufferData.second)
        {
            fillFn(std::forward<Args>(args)..., bufferData.first);
            bufferData.second = true;
        }
    };

    auto LoadOrInitTexture = [&InitBufferData]<class... Args>(Handle<Texture>& outTexture, const String& path, const UTF8StringView& name, const TextureDesc& textureDesc, PlaceholderBufferData& bufferData, auto fillFn, Args&&... args)
    {
        if (Handle<AssetObject> asset = g_assetManager->GetAssetRegistry()->GetAssetFromPath(path + "/" + name); asset.IsValid())
        {
            Handle<Texture> textureAsset = ObjCast<Texture>(asset);
            Assert(textureAsset != nullptr);

            textureAsset->SetPersistentRequested(true, /* setFlag */ true);

            CheckResult(textureAsset->Create());

            outTexture = textureAsset;

            return;
        }

        InitBufferData(bufferData, fillFn, textureDesc.extent.GetXY(), std::forward<Args>(args)...);

        outTexture = MakeHandle<Texture>(textureDesc, bufferData.first.ToByteView());
        outTexture->SetName(CreateNameFromDynamicString(*name));
        outTexture->SetPersistentRequested(true, /* setFlag */ true);
        
        g_assetManager->GetAssetRegistry()->RegisterAsset(path, outTexture);

        CheckResult(outTexture->Create());
    };

    PlaceholderBufferData placeholderBufferTex2d {};
    PlaceholderBufferData placeholderBufferTex3d {};
    PlaceholderBufferData placeholderBufferCubemap {};

    LoadOrInitTexture(
        defaultTexture2d,
        "Engine/Textures",
        "Placeholder_Texture_2D_1x1",
        TextureDesc {
            TextureType::Texture2D,
            TextureFormat::RGBA8,
            Vec3u { 4, 4, 1 },
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_REPEAT,
            1,
            IU_SAMPLED | IU_STORAGE },
        placeholderBufferTex2d,
        &FillPlaceholderBuffer_Tex2D<TextureFormat::RGBA8>);

    LoadOrInitTexture(
        defaultTexture3d,
        "Engine/Textures",
        "Placeholder_Texture_3D_1x1x1",
        TextureDesc {
            TextureType::Texture3D,
            TextureFormat::RGBA8,
            Vec3u::One(),
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_SAMPLED | IU_STORAGE },
        placeholderBufferTex3d,
        &FillPlaceholderBuffer_Tex2D<TextureFormat::RGBA8>);

    LoadOrInitTexture(
        defaultCubemap,
        "Engine/Textures",
        "Placeholder_Texture_Cube_1x1",
        TextureDesc {
            TextureType::Cubemap,
            TextureFormat::RGBA8,
            Vec3u { 4, 4, 1 },
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_REPEAT,
            1,
            IU_SAMPLED | IU_STORAGE },
        placeholderBufferCubemap,
        &FillPlaceholderBuffer_Cubemap<TextureFormat::RGBA8>);

    LoadOrInitTexture(
        defaultTexture2dArray,
        "Engine/Textures",
        "Placeholder_Texture_2D_1x1_Array",
        TextureDesc {
            TextureType::Texture2DArray,
            TextureFormat::RGBA8,
            Vec3u { 4, 4, 1 },
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_REPEAT,
            1,
            IU_SAMPLED | IU_STORAGE },
        placeholderBufferTex2d,
        &FillPlaceholderBuffer_Tex2D<TextureFormat::RGBA8>);

    LoadOrInitTexture(
        defaultCubemapArray,
        "Engine/Textures",
        "Placeholder_Texture_Cube_1x1_Array",
        TextureDesc {
            TextureType::CubemapArray,
            TextureFormat::RGBA8,
            Vec3u { 4, 4, 1 },
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_REPEAT,
            1,
            IU_SAMPLED | IU_STORAGE },
        placeholderBufferCubemap,
        &FillPlaceholderBuffer_Cubemap<TextureFormat::RGBA8>);

#pragma endregion Textures

#pragma region Samplers
    
#if HYP_DEBUG_MODE
    m_samplerLinear->SetDebugName(NAME("Placeholder_Sampler_Linear"));
#endif

    CheckResult(m_samplerLinear->Create());
    
#if HYP_DEBUG_MODE
    m_samplerLinearMipmap->SetDebugName(NAME("Placeholder_Sampler_Linear_Mipmap"));
#endif

    CheckResult(m_samplerLinearMipmap->Create());
    
#if HYP_DEBUG_MODE
    m_samplerNearest->SetDebugName(NAME("Placeholder_Sampler_Nearest"));
#endif

    CheckResult(m_samplerNearest->Create());

#pragma endregion Samplers
}

void PlaceholderData::Shutdown()
{
    EnqueueDeletion(std::move(m_image2d1x1R8));
    EnqueueDeletion(std::move(m_imageView2d1x1R8));
    EnqueueDeletion(std::move(m_image2d1x1R8Storage));
    EnqueueDeletion(std::move(m_imageView2d1x1R8Storage));
    EnqueueDeletion(std::move(m_image3d1x1x1R8));
    EnqueueDeletion(std::move(m_imageView3d1x1x1R8));
    EnqueueDeletion(std::move(m_image3d1x1x1R8Storage));
    EnqueueDeletion(std::move(m_imageView3d1x1x1R8Storage));
    EnqueueDeletion(std::move(m_imageCube1x1R8));
    EnqueueDeletion(std::move(m_imageViewCube1x1R8));
    EnqueueDeletion(std::move(m_image2d1x1R8Array));
    EnqueueDeletion(std::move(m_imageView2d1x1R8Array));
    EnqueueDeletion(std::move(m_imageCube1x1R8Array));
    EnqueueDeletion(std::move(m_imageViewCube1x1R8Array));
    EnqueueDeletion(std::move(m_samplerLinear));
    EnqueueDeletion(std::move(m_samplerLinearMipmap));
    EnqueueDeletion(std::move(m_samplerNearest));

    for (auto& bufferMap : m_buffers)
    {
        for (auto& it : bufferMap.second)
        {
            EnqueueDeletion(std::move(it.second));
        }
    }

    m_buffers.Clear();
}

GpuBufferRef PlaceholderData::CreateGpuBuffer(GpuBufferType bufferType, size_t size)
{
    GpuBufferRef gpuBuffer = g_renderInterface->MakeGpuBuffer(bufferType, size);
#if HYP_DEBUG_MODE
    gpuBuffer->SetDebugName(NAME("Placeholder_GpuBuffer"));
#endif
    CheckResult(gpuBuffer->Create());

    return gpuBuffer;
}

#pragma endregion PlaceholderData

} // namespace Hyperion
