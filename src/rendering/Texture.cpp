/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Texture.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/RenderGpuImage.hpp>
#include <rendering/RenderSampler.hpp>
#include <rendering/RenderQueue.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderTextureMipmap.hpp>
#include <rendering/RenderHelpers.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/TextureAsset.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <util/img/Bitmap.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

namespace hyperion {

class Texture;
class TextureMipmapRenderer;

const FixedArray<Pair<Vec3f, Vec3f>, 6> Texture::cubemapDirections = {
    Pair<Vec3f, Vec3f> { Vec3f { 1, 0, 0 }, Vec3f { 0, 1, 0 } },
    Pair<Vec3f, Vec3f> { Vec3f { -1, 0, 0 }, Vec3f { 0, 1, 0 } },
    Pair<Vec3f, Vec3f> { Vec3f { 0, 1, 0 }, Vec3f { 0, 0, -1 } },
    Pair<Vec3f, Vec3f> { Vec3f { 0, -1, 0 }, Vec3f { 0, 0, 1 } },
    Pair<Vec3f, Vec3f> { Vec3f { 0, 0, 1 }, Vec3f { 0, 1, 0 } },
    Pair<Vec3f, Vec3f> { Vec3f { 0, 0, -1 }, Vec3f { 0, 1, 0 } }
};

static const Name g_nameTextureDefault = NAME("<unnamed texture>");

#pragma region Render commands

struct RENDER_COMMAND(CreateTextureGpuImage)
    : RenderCommand
{
    WeakHandle<Texture> textureWeak;
    ResourceHandle resourceHandle;
    ResourceState initialState;
    GpuImageRef image;

    RENDER_COMMAND(CreateTextureGpuImage)(
        const WeakHandle<Texture>& textureWeak,
        ResourceHandle&& resourceHandle,
        ResourceState initialState,
        GpuImageRef image)
        : textureWeak(textureWeak),
          resourceHandle(std::move(resourceHandle)),
          initialState(initialState),
          image(std::move(image))
    {
        Assert(this->image.IsValid());
    }

    virtual ~RENDER_COMMAND(CreateTextureGpuImage)() override = default;

    virtual RendererResult operator()() override
    {
        Handle<Texture> texture = textureWeak.Lock();

        if (!texture.IsValid())
        {
            return {};
        }

        if (!image->IsCreated())
        {
            HYP_GFX_CHECK(image->Create());

            if (texture->GetAsset().IsValid())
            {
                Assert(resourceHandle);

                TextureData* textureData = texture->GetAsset()->GetTextureData();
                Assert(textureData != nullptr);

                const TextureDesc& textureDesc = texture->GetAsset()->GetTextureDesc();

                ByteBuffer const* imageData = &textureData->imageData;
                LinkedList<ByteBuffer> placeholderBuffers;

                if (textureDesc != image->GetTextureDesc())
                {
                    HYP_LOG(Streaming, Warning, "Streamed texture data TextureDesc not equal to Image's TextureDesc!");
                }

                if (imageData->Size() != image->GetByteSize())
                {
                    HYP_LOG(Streaming, Warning, "Streamed texture data buffer size mismatch! Expected: {}, Got: {}",
                        image->GetByteSize(), imageData->Size());

                    // fill some placeholder data with zeros so we don't crash
                    ByteBuffer* placeholderBuffer = &placeholderBuffers.EmplaceBack();
                    placeholderBuffer->SetSize(image->GetByteSize());

                    const TextureFormat nonSrgbFormat = ChangeFormatSrgb(image->GetTextureFormat(), false);

                    switch (texture->GetType())
                    {
                    case TT_TEX2D:
                        switch (nonSrgbFormat)
                        {
                        case TF_R8:
                            FillPlaceholderBuffer_Tex2D<TF_R8>(image->GetExtent().GetXY(), *placeholderBuffer);
                            break;
                        case TF_RGBA8:
                            FillPlaceholderBuffer_Tex2D<TF_RGBA8>(image->GetExtent().GetXY(), *placeholderBuffer);
                            break;
                        case TF_RGBA16F:
                            FillPlaceholderBuffer_Tex2D<TF_RGBA16F>(image->GetExtent().GetXY(), *placeholderBuffer);
                            break;
                        case TF_RGBA32F:
                            FillPlaceholderBuffer_Tex2D<TF_RGBA32F>(image->GetExtent().GetXY(), *placeholderBuffer);
                            break;
                        default:
                            // no FillPlaceholderBuffer method defined
                            break;
                        }
                        break;
                    case TT_CUBEMAP:
                        switch (nonSrgbFormat)
                        {
                        case TF_R8:
                            FillPlaceholderBuffer_Cubemap<TF_R8>(image->GetExtent().GetXY(), *placeholderBuffer);
                            break;
                        case TF_RGBA8:
                            FillPlaceholderBuffer_Cubemap<TF_RGBA8>(image->GetExtent().GetXY(), *placeholderBuffer);
                            break;
                        default:
                            // no FillPlaceholderBuffer method defined
                            break;
                        }
                        break;
                    default:
                        // no FillPlaceholderBuffer method defined
                        break;
                    }

                    imageData = placeholderBuffer;
                }

                GpuBufferRef stagingBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, imageData->Size());
                HYP_GFX_CHECK(stagingBuffer->Create());
                stagingBuffer->Copy(imageData->Size(), imageData->Data());

                HYP_DEFER({ SafeRelease(std::move(stagingBuffer)); });

                FrameBase* frame = g_renderBackend->GetCurrentFrame();
                RenderQueue& renderQueue = frame->renderQueue;

                renderQueue << InsertBarrier(image, RS_COPY_DST);

                renderQueue << CopyBufferToImage(stagingBuffer, image);

                if (textureDesc.HasMipmaps())
                {
                    renderQueue << GenerateMipmaps(image);
                }

                renderQueue << InsertBarrier(image, initialState);
            }
            else if (initialState != RS_UNDEFINED)
            {
                FrameBase* frame = g_renderBackend->GetCurrentFrame();
                RenderQueue& renderQueue = frame->renderQueue;

                // Transition to initial state
                renderQueue << InsertBarrier(image, initialState);
            }
        }

        resourceHandle.Reset();

        return {};
    }
};

#pragma endregion Render commands

#pragma region Texture

Texture::Texture()
    : Texture(TextureDesc {
          TT_TEX2D,
          TF_RGBA8,
          Vec3u { 1, 1, 1 },
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE })
{
}

Texture::Texture(const TextureDesc& textureDesc)
    : m_asset(CreateObject<TextureAsset>(g_nameTextureDefault, TextureData { textureDesc }))
{
    SetName(g_nameTextureDefault);
}

Texture::Texture(const TextureData& textureData)
    : m_asset(CreateObject<TextureAsset>(g_nameTextureDefault, textureData))
{
    SetName(g_nameTextureDefault);
}

Texture::Texture(const Handle<TextureAsset>& asset)
    : m_asset(asset)
{
    SetName(g_nameTextureDefault);
}

Texture::~Texture()
{
    SafeRelease(std::move(m_gpuImage));
}

void Texture::Init()
{
    AddDelegateHandler(g_engineDriver->GetDelegates().OnShutdown.Bind([this]()
        {
            SafeRelease(std::move(m_gpuImage));
        }));

    if (m_asset.IsValid() && !m_asset->IsRegistered())
    {
        if (!m_asset->GetName().IsValid() && m_name.IsValid() && m_name != g_nameTextureDefault)
        {
            m_asset->Rename(m_name);
        }

        g_assetManager->GetAssetRegistry()->RegisterAsset("$Memory/Media/Textures", m_asset);
    }

    m_gpuImage = g_renderBackend->MakeImage(GetTextureDesc());

    PUSH_RENDER_COMMAND(
        CreateTextureGpuImage,
        WeakHandleFromThis(),
        m_asset.IsValid() ? ResourceHandle(*m_asset->GetResource()) : ResourceHandle(),
        RS_SHADER_RESOURCE,
        m_gpuImage);

    SetReady(true);
}

void Texture::SetName(Name name)
{
    if (name == m_name)
    {
        return;
    }

    m_name = name;

    if (m_asset.IsValid() && !m_asset->IsRegistered() && IsInitCalled())
    {
        if (!m_asset->GetName().IsValid() && m_name.IsValid() && m_name != g_nameTextureDefault)
        {
            m_asset->Rename(m_name);
        }

        g_assetManager->GetAssetRegistry()->RegisterAsset("$Memory/Media/Textures", m_asset);
    }
}

const TextureDesc& Texture::GetTextureDesc() const
{
    static const TextureDesc defaultTextureDesc {};
    return m_asset.IsValid() ? m_asset->GetTextureDesc() : defaultTextureDesc;
}

void Texture::SetTextureDesc(const TextureDesc& textureDesc)
{
    TextureDesc currentTextureDesc = GetTextureDesc();

    if (currentTextureDesc == textureDesc)
    {
        return;
    }

    // create new asset
    if (m_asset.IsValid())
    {
        Handle<TextureAsset> prevAsset = m_asset;

        Handle<AssetPackage> package = prevAsset->GetPackage();

        ResourceHandle resourceHandle(*prevAsset->GetResource());

        TextureData textureData = std::move(*prevAsset->GetTextureData());
        textureData.desc = textureDesc;

        m_asset = CreateObject<TextureAsset>(prevAsset->GetName(), textureData);

        if (package.IsValid())
        {
            package->RemoveAssetObject(prevAsset);
            package->AddAssetObject(m_asset);
        }
    }
    else
    {
        m_asset = CreateObject<TextureAsset>(GetName(), TextureData { textureDesc });
    }

    if (IsInitCalled() && !m_asset->IsRegistered())
    {
        g_assetManager->GetAssetRegistry()->RegisterAsset("$Memory/Media/Textures", m_asset);
    }
}

void Texture::GenerateMipmaps()
{
    HYP_SCOPE;
    AssertReady();

    TextureMipmapRenderer::RenderMipmaps(HandleFromThis());
}

void Texture::Readback(ByteBuffer& outByteBuffer)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertReady();

    GpuBufferRef gpuBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, m_gpuImage->GetByteSize());
    HYP_GFX_ASSERT(gpuBuffer->Create());
    gpuBuffer->Map();

    UniquePtr<SingleTimeCommands> singleTimeCommands = g_renderBackend->GetSingleTimeCommands();

    singleTimeCommands->Push([this, &gpuBuffer](RenderQueue& renderQueue)
        {
            const ResourceState previousResourceState = m_gpuImage->GetResourceState();

            renderQueue << InsertBarrier(m_gpuImage, RS_COPY_SRC);
            renderQueue << InsertBarrier(gpuBuffer, RS_COPY_DST);

            renderQueue << CopyImageToBuffer(m_gpuImage, gpuBuffer);

            renderQueue << InsertBarrier(m_gpuImage, previousResourceState);
        });

    RendererResult result = singleTimeCommands->Execute();

    if (result.HasError())
    {
        HYP_LOG(Rendering, Error, "Failed to readback texture data! {}", result.GetError().GetMessage());

        return;
    }

    outByteBuffer.SetSize(gpuBuffer->Size());
    gpuBuffer->Read(outByteBuffer.Size(), outByteBuffer.Data());

    gpuBuffer->Destroy();
}

void Texture::EnqueueReadback(Proc<void(ByteBuffer&& byteBuffer)>&& callback)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertReady();

    FrameBase* currentFrame = g_renderBackend->GetCurrentFrame();

    // No current frame, fallback to blocking Readback() call.
    if (!currentFrame)
    {
        ByteBuffer byteBuffer;
        Readback(byteBuffer);

        callback(std::move(byteBuffer));

        return;
    }

    RenderQueue& renderQueue = currentFrame->renderQueue;

    const ResourceState previousResourceState = m_gpuImage->GetResourceState();

    GpuBufferRef stagingBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, m_gpuImage->GetByteSize());
    HYP_GFX_ASSERT(stagingBuffer->Create());
    stagingBuffer->Map();

    renderQueue << InsertBarrier(m_gpuImage, RS_COPY_SRC);
    renderQueue << InsertBarrier(stagingBuffer, RS_COPY_DST);

    renderQueue << CopyImageToBuffer(m_gpuImage, stagingBuffer);

    renderQueue << InsertBarrier(m_gpuImage, previousResourceState);

    currentFrame->OnFrameEnd
        .Bind([gpuImageRef = m_gpuImage /* hold a strong reference to our buffer to ensure it is kept alive */,
                  stagingBuffer = std::move(stagingBuffer),
                  callback = std::move(callback)](...) mutable
            {
                ByteBuffer byteBuffer;
                byteBuffer.SetSize(stagingBuffer->Size());

                stagingBuffer->Read(byteBuffer.Size(), byteBuffer.Data());

                SafeRelease(std::move(stagingBuffer));
                SafeRelease(std::move(gpuImageRef));

                callback(std::move(byteBuffer));
            })
        .Detach();
}

Vec4f Texture::Sample(Vec3f uvw, uint32 faceIndex)
{
    if (!IsReady())
    {
        HYP_LOG_ONCE(Texture, Warning, "Texture is not ready, cannot sample");

        return Vec4f::Zero();
    }

    if (faceIndex >= NumFaces())
    {
        HYP_LOG_ONCE(Texture, Error, "Face index out of bounds: {} >= {}", faceIndex, NumFaces());

        return Vec4f::Zero();
    }

    if (!m_asset.IsValid())
    {
        HYP_LOG_ONCE(Texture, Warning, "Texture asset is not valid, cannot sample");

        return Vec4f::Zero();
    }

    if (!m_asset->IsLoaded())
    {
        HYP_LOG_ONCE(Texture, Debug, "Texture does not have data loaded, will be streamed in");
    }

    ResourceHandle resourceHandle = ResourceHandle(*m_asset->GetResource());

    if (!resourceHandle)
    {
        HYP_LOG_ONCE(Texture, Warning, "Texture resource handle is not valid, cannot sample");

        return Vec4f::Zero();
    }

    TextureData* textureData = m_asset->GetTextureData();
    Assert(textureData != nullptr);

    Vec3u coord = {
        uint32(std::fmodf(uvw.x, 1.0f) * float(textureData->desc.extent.x - 1) + 0.5f),
        uint32(std::fmodf(uvw.y, 1.0f) * float(textureData->desc.extent.y - 1) + 0.5f),
        uint32(std::fmodf(uvw.z, 1.0f) * float(textureData->desc.extent.z - 1) + 0.5f)
    };

    const uint32 bytesPerComponent = BytesPerComponent(textureData->desc.format);

    if (bytesPerComponent != 1)
    {
        HYP_LOG_ONCE(Texture, Warning, "Unsupported bytes per component to use with Sample(): {}", bytesPerComponent);

        HYP_BREAKPOINT;

        return Vec4f::Zero();
    }

    const uint32 numComponents = NumComponents(textureData->desc.format);

    const uint32 index = faceIndex * (textureData->desc.extent.x * textureData->desc.extent.y * textureData->desc.extent.z * bytesPerComponent * numComponents)
        + coord.z * (textureData->desc.extent.x * textureData->desc.extent.y * bytesPerComponent * numComponents)
        + coord.y * (textureData->desc.extent.x * bytesPerComponent * numComponents)
        + coord.x * bytesPerComponent * numComponents;

    if (index + (bytesPerComponent * numComponents) >= textureData->imageData.Size())
    {
        HYP_LOG_ONCE(Texture, Warning, "Sample() call would attempt to read out of bounds of data for Texture {} ({})!\n\tTexel index: {}, texture data buffer size: {}, coord: {}, dimensions: {}, num faces: {}",
            GetName(), Id(),
            index, textureData->imageData.Size(),
            coord, textureData->desc.extent, NumFaces());

        return Vec4f::Zero();
    }

    if ((textureData->desc.format >= TF_R16F && textureData->desc.format <= TF_RGBA32F) || textureData->desc.format == TF_R11G11B10F)
    {
        // FP format
        switch (numComponents)
        {
        case 1:
            return PixelReference<float, 1>(textureData->imageData.Data() + index).GetRGBA();
        case 2:
            return PixelReference<float, 2>(textureData->imageData.Data() + index).GetRGBA();
        case 3:
            return PixelReference<float, 3>(textureData->imageData.Data() + index).GetRGBA();
        case 4:
            return PixelReference<float, 4>(textureData->imageData.Data() + index).GetRGBA();
        default:
            break;
        }
    }
    else if (textureData->desc.format >= TF_R16 && textureData->desc.format <= TF_RGBA16)
    {
        // 16 bit integer format
        switch (numComponents)
        {
        case 1:
            return PixelReference<uint16, 1>(textureData->imageData.Data() + index).GetRGBA();
        case 2:
            return PixelReference<uint16, 2>(textureData->imageData.Data() + index).GetRGBA();
        case 3:
            return PixelReference<uint16, 3>(textureData->imageData.Data() + index).GetRGBA();
        case 4:
            return PixelReference<uint16, 4>(textureData->imageData.Data() + index).GetRGBA();
        default:
            break;
        }
    }
    else if (textureData->desc.format >= TF_R32 && textureData->desc.format <= TF_RGBA32)
    {
        // 32 bit integer format
        switch (numComponents)
        {
        case 1:
            return PixelReference<uint32, 1>(textureData->imageData.Data() + index).GetRGBA();
        case 2:
            return PixelReference<uint32, 2>(textureData->imageData.Data() + index).GetRGBA();
        case 3:
            return PixelReference<uint32, 3>(textureData->imageData.Data() + index).GetRGBA();
        case 4:
            return PixelReference<uint32, 4>(textureData->imageData.Data() + index).GetRGBA();
        default:
            break;
        }
    }
    else
    {
        // ubyte format
        switch (numComponents)
        {
        case 1:
            return PixelReference<ubyte, 1>(textureData->imageData.Data() + index).GetRGBA();
        case 2:
            return PixelReference<ubyte, 2>(textureData->imageData.Data() + index).GetRGBA();
        case 3:
            return PixelReference<ubyte, 3>(textureData->imageData.Data() + index).GetRGBA();
        case 4:
            return PixelReference<ubyte, 4>(textureData->imageData.Data() + index).GetRGBA();
        default:
            break;
        }
    }

    HYP_LOG_ONCE(Texture, Error, "Unsupported texture format to read on CPU: {}", int(textureData->desc.format));

    return Vec4f::Zero();
}

Vec4f Texture::Sample2D(Vec2f uv)
{
    if (GetType() != TT_TEX2D)
    {
        HYP_LOG_ONCE(Texture, Warning, "Unsupported texture type to use with Sample2D(): {}", GetType());

        return Vec4f::Zero();
    }

    return Sample(Vec3f { uv.x, uv.y, 0.0f }, 0);
}

/// https://www.gamedev.net/forums/topic/687535-implementing-a-cube-map-lookup-function/5337472/
Vec4f Texture::SampleCube(Vec3f direction)
{
    if (GetType() != TT_CUBEMAP)
    {
        HYP_LOG_ONCE(Texture, Warning, "Unsupported texture type to use with SampleCube(): {}", GetType());

        return Vec4f::Zero();
    }

    Vec3f absDir = MathUtil::Abs(direction);
    uint32 faceIndex = 0;

    float mag;
    Vec2f uv;

    if (absDir.z >= absDir.x && absDir.z >= absDir.y)
    {
        mag = absDir.z;

        if (direction.z < 0.0f)
        {
            faceIndex = 5;
            uv = Vec2f(-direction.x, -direction.y);
        }
        else
        {
            faceIndex = 4;
            uv = Vec2f(direction.x, -direction.y);
        }
    }
    else if (absDir.y >= absDir.x)
    {
        mag = absDir.y;

        if (direction.y < 0.0f)
        {
            faceIndex = 3;
            uv = Vec2f(direction.x, -direction.z);
        }
        else
        {
            faceIndex = 2;
            uv = Vec2f(direction.x, direction.z);
        }
    }
    else
    {
        mag = absDir.x;

        if (direction.x < 0.0f)
        {
            faceIndex = 1;
            uv = Vec2f(direction.z, -direction.y);
        }
        else
        {
            faceIndex = 0;
            uv = Vec2f(-direction.z, -direction.y);
        }
    }

    return Sample(Vec3f { uv / mag * 0.5f + 0.5f, 0.0f }, faceIndex);
}

#pragma endregion Texture

} // namespace hyperion
