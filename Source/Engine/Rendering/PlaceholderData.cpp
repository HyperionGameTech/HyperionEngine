/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <Rendering/PlaceholderData.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/Texture.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Core/Math/Vector2.hpp>

#include <Util/Img/Bitmap.hpp>

namespace Hyperion {

template <TextureFormat Format, FillPattern Pattern>
void FillPlaceholderBuffer_Tex2D(Vec2u dimensions, ByteBuffer& outBuffer)
{
    using Helper = TextureFormatHelper<Format>;

    auto bitmap = Bitmap<Format>(dimensions.x, dimensions.y);

    for (uint32 y = 0; y < dimensions.y; y++)
    {
        for (uint32 x = 0; x < dimensions.x; x++)
        {

            switch (Pattern)
            {
            case FillPattern::SolidWhite:
                bitmap.SetPixel(x, y, { 1.0f, 1.0f, 1.0f, 1.0f });
                break;
            case FillPattern::SolidBlack:
                bitmap.SetPixel(x, y, { 0.0f, 0.0f, 0.0f, 1.0f });
                break;
            case FillPattern::Checkerboard:
            {
                const bool isColor = ((x / 4) % 2) == ((y / 4) % 2);
                bitmap.SetPixel(x, y, { isColor ? 1.0f : 0.0f, 0.0f, isColor ? 1.0f : 0.0f, 1.0f });
                break;
            }
            }
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

template <TextureFormat Format, FillPattern Pattern>
void FillPlaceholderBuffer_Cubemap(Vec2u dimensions, ByteBuffer& outBuffer)
{
    using Helper = TextureFormatHelper<Format>;
    static_assert(!Helper::IsFloatingPoint, "FillPlaceholderBuffer_Cubemap not implemented for floating point type textures");

    auto bitmap = Bitmap<Format>(dimensions.x, dimensions.y);

    // checkerboard pattern
    for (uint32 y = 0; y < dimensions.y; y++)
    {
        for (uint32 x = 0; x < dimensions.x; x++)
        {
            switch (Pattern)
            {
                case FillPattern::SolidBlack:
                    bitmap.SetPixel(x, y, { 0.0f, 0.0f, 0.0f, 1.0f });
                    break;
                case FillPattern::SolidWhite:
                    bitmap.SetPixel(x, y, { 1.0f, 1.0f, 1.0f, 1.0f });
                    break;
                case FillPattern::Checkerboard:
                    const bool isColor = ((x / 16) % 2) == ((y / 16) % 2);
                    bitmap.SetPixel(x, y, { isColor ? 1.0f : 0.0f, 0.0f, isColor ? 1.0f : 0.0f, 1.0f });
                    break;
            }
        }
    }

    ByteBuffer faceByteBuffer = bitmap.GetUnpackedBytes(Helper::BytesPerComponent * Helper::NumComponents);

    outBuffer.SetSize(faceByteBuffer.Size() * 6);

    for (uint32 i = 0; i < 6; i++)
    {
        outBuffer.Write(faceByteBuffer.Size(), i * faceByteBuffer.Size(), faceByteBuffer.Data());
    }
}

template void FillPlaceholderBuffer_Tex2D<TextureFormat::R8, FillPattern::SolidBlack>(Vec2u dimensions, ByteBuffer& outBuffer);      // R8
template void FillPlaceholderBuffer_Tex2D<TextureFormat::RGBA8, FillPattern::SolidBlack>(Vec2u dimensions, ByteBuffer& outBuffer);   // RGBA8
template void FillPlaceholderBuffer_Tex2D<TextureFormat::RGBA16F, FillPattern::SolidBlack>(Vec2u dimensions, ByteBuffer& outBuffer); // RGBA16F
template void FillPlaceholderBuffer_Tex2D<TextureFormat::RGBA32F, FillPattern::SolidBlack>(Vec2u dimensions, ByteBuffer& outBuffer); // RGBA32F

template void FillPlaceholderBuffer_Cubemap<TextureFormat::R8, FillPattern::SolidBlack>(Vec2u dimensions, ByteBuffer& outBuffer);    // R8
template void FillPlaceholderBuffer_Cubemap<TextureFormat::RGBA8, FillPattern::SolidBlack>(Vec2u dimensions, ByteBuffer& outBuffer); // RGBA8

#pragma region PlaceholderData

PlaceholderData::PlaceholderData()
    : m_image2d1x1R8(RI.MakeImage(TextureDesc {
          TextureType::Texture2D,
          TextureFormat::R8,
          Vec3u::One(),
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE,
          1,
          IU_SAMPLED })),
      m_imageView2d1x1R8(RI.MakeImageView(m_image2d1x1R8)),
      m_image2d1x1R8Storage(RI.MakeImage(TextureDesc {
          TextureType::Texture2D,
          TextureFormat::R8,
          Vec3u::One(),
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE,
          1,
          IU_STORAGE | IU_SAMPLED })),
      m_imageView2d1x1R8Storage(RI.MakeImageView(m_image2d1x1R8Storage)),
      m_image3d1x1x1R8(RI.MakeImage(TextureDesc {
          TextureType::Texture3D,
          TextureFormat::R8,
          Vec3u::One(),
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE,
          1,
          IU_SAMPLED })),
      m_imageView3d1x1x1R8(RI.MakeImageView(m_image3d1x1x1R8)),
      m_image3d1x1x1R8Storage(RI.MakeImage(TextureDesc {
          TextureType::Texture3D,
          TextureFormat::R8,
          Vec3u::One(),
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE,
          1,
          IU_STORAGE | IU_SAMPLED })),
      m_imageView3d1x1x1R8Storage(RI.MakeImageView(m_image3d1x1x1R8Storage)),
      m_imageCube1x1R8(RI.MakeImage(TextureDesc {
          TextureType::Cubemap,
          TextureFormat::RGBA8,
          Vec3u::One(),
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE,
          1,
          IU_SAMPLED })),
      m_imageViewCube1x1R8(RI.MakeImageView(m_imageCube1x1R8)),
      m_image2d1x1R8Array(RI.MakeImage(TextureDesc {
          TextureType::Texture2DArray,
          TextureFormat::R8,
          Vec3u::One(),
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE,
          1,
          IU_SAMPLED })),
      m_imageView2d1x1R8Array(RI.MakeImageView(m_image2d1x1R8Array)),
      m_imageCube1x1R8Array(RI.MakeImage(TextureDesc {
          TextureType::CubemapArray,
          TextureFormat::RGBA8,
          Vec3u::One(),
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE,
          1,
          IU_SAMPLED })),
      m_imageViewCube1x1R8Array(RI.MakeImageView(m_imageCube1x1R8Array)),
      m_samplerLinear(RI.MakeSampler(SamplerDesc {
          TFM_LINEAR,
          TFM_LINEAR,
          TWM_REPEAT
      })),
      m_samplerLinearMipmap(RI.MakeSampler(SamplerDesc {
          TFM_LINEAR_MIPMAP,
          TFM_LINEAR,
          TWM_REPEAT
      })),
      m_samplerNearest(RI.MakeSampler(SamplerDesc {
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE
      }))
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

    auto LoadOrInitTexture = [&InitBufferData]<class... Args>(Handle<Texture>& outTexture, Name name, const TextureDesc& textureDesc, PlaceholderBufferData& bufferData, auto fillFn, Args&&... args)
    {
        if (Handle<AssetObject> asset = GetEngineAssetRegistry()->GetAsset(AssetBuckets::Textures, name); asset.IsValid())
        {
            Handle<Texture> textureAsset = DynamicCast<Texture>(asset);
            Assert(textureAsset != nullptr);

            textureAsset->SetPersistentRequested(true, /* setFlag */ true);

            CheckResult(textureAsset->Create());

            outTexture = textureAsset;

            return;
        }

        InitBufferData(bufferData, fillFn, textureDesc.extent.GetXY(), std::forward<Args>(args)...);

        outTexture = MakeHandle<Texture>(textureDesc, bufferData.first.ToByteView());
        outTexture->SetName(name);
        outTexture->SetPersistentRequested(true, /* setFlag */ true);

        GetEngineAssetRegistry()->PutAsset(outTexture);

        CheckResult(outTexture->Create());
    };

    PlaceholderBufferData placeholderBufferTex2d {};
    PlaceholderBufferData placeholderBufferTex3d {};
    PlaceholderBufferData placeholderBufferCubemap {};

    LoadOrInitTexture(
        defaultTexture2d,
        NAME("Checkerboard"),
        TextureDesc {
            TextureType::Texture2D,
            TextureFormat::RGBA8,
            Vec3u { 8, 8, 1 },
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_REPEAT,
            1,
            IU_SAMPLED
        },
        placeholderBufferTex2d,
        &FillPlaceholderBuffer_Tex2D<TextureFormat::RGBA8, FillPattern::Checkerboard>);

    LoadOrInitTexture(
        defaultTexture3d,
        NAME("Checkerboard_3D"),
        TextureDesc {
            TextureType::Texture3D,
            TextureFormat::RGBA8,
            Vec3u { 1, 1, 1 },
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_SAMPLED
        },
        placeholderBufferTex3d,
        &FillPlaceholderBuffer_Tex2D<TextureFormat::RGBA8, FillPattern::Checkerboard>);

    LoadOrInitTexture(
        defaultCubemap,
        NAME("Checkerboard_Cube"),
        TextureDesc {
            TextureType::Cubemap,
            TextureFormat::RGBA8,
            Vec3u { 8, 8, 1 },
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_REPEAT,
            1,
            IU_SAMPLED
        },
        placeholderBufferCubemap,
        &FillPlaceholderBuffer_Cubemap<TextureFormat::RGBA8, FillPattern::Checkerboard>);

    LoadOrInitTexture(
        defaultTexture2dArray,
        NAME("Checkerboard_Array2D"),
        TextureDesc {
            TextureType::Texture2DArray,
            TextureFormat::RGBA8,
            Vec3u { 8, 8, 1 },
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_REPEAT,
            1,
            IU_SAMPLED
        },
        placeholderBufferTex2d,
        &FillPlaceholderBuffer_Tex2D<TextureFormat::RGBA8, FillPattern::Checkerboard>);

    LoadOrInitTexture(
        defaultCubemapArray,
        NAME("Checkerboard_ArrayCube"),
        TextureDesc {
            TextureType::CubemapArray,
            TextureFormat::RGBA8,
            Vec3u { 8, 8, 1 },
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_REPEAT,
            1,
            IU_SAMPLED
        },
        placeholderBufferCubemap,
        &FillPlaceholderBuffer_Cubemap<TextureFormat::RGBA8, FillPattern::Checkerboard>);

    LoadOrInitTexture(
        textureSolidWhite,
        NAME("SolidWhite"),
        TextureDesc {
            TextureType::Texture2D,
            TextureFormat::RGBA8,
            Vec3u { 1, 1, 1 },
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_REPEAT,
            1,
            IU_SAMPLED
        },
        placeholderBufferTex2d,
        &FillPlaceholderBuffer_Tex2D<TextureFormat::RGBA8, FillPattern::SolidWhite>);

    LoadOrInitTexture(
        textureSolidBlack,
        NAME("SolidBlack"),
        TextureDesc {
            TextureType::Texture2D,
            TextureFormat::RGBA8,
            Vec3u { 1, 1, 1 },
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_REPEAT,
            1,
            IU_SAMPLED
        },
        placeholderBufferTex2d,
        &FillPlaceholderBuffer_Tex2D<TextureFormat::RGBA8, FillPattern::SolidBlack>);

#pragma endregion Textures

#pragma region Samplers
    
#ifdef HYP_RHI_DEBUG_NAMES
    m_samplerLinear->SetDebugName(NAME("Placeholder_Sampler_Linear"));
#endif

    CheckResult(m_samplerLinear->Create());

#if HYP_DEBUG_MODE
    m_samplerLinearMipmap->SetDebugName(NAME("Placeholder_Sampler_Linear_Mipmap"));
#endif

    CheckResult(m_samplerLinearMipmap->Create());
    
#ifdef HYP_RHI_DEBUG_NAMES
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
    GpuBufferRef gpuBuffer = RI.MakeGpuBuffer(bufferType, size);
#ifdef HYP_RHI_DEBUG_NAMES
    gpuBuffer->SetDebugName(NAME("Placeholder_GpuBuffer"));
#endif
    CheckResult(gpuBuffer->Create());

    return gpuBuffer;
}

#pragma endregion PlaceholderData

} // namespace Hyperion
