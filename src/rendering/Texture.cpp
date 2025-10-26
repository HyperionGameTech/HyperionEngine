/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Texture.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderGpuImage.hpp>
#include <rendering/RenderSampler.hpp>
#include <rendering/RenderQueue.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderHelpers.hpp>

#include <rendering/util/SafeDeleter.hpp>
#include <rendering/util/TextureMipmapRenderer.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/TextureAsset.hpp>

#include <core/utilities/DeferredScope.hpp>

// for EnumToString
#include <core/reflection/HypEnum.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <util/img/Bitmap.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

namespace hyperion {

class Texture;
class TextureMipmapRenderer;

const FixedArray<Pair<Vec3f, Vec3f>, 6> Texture::s_cubemapDirections = {
    Pair<Vec3f, Vec3f> { Vec3f { 1, 0, 0 }, Vec3f { 0, 1, 0 } },
    Pair<Vec3f, Vec3f> { Vec3f { -1, 0, 0 }, Vec3f { 0, 1, 0 } },
    Pair<Vec3f, Vec3f> { Vec3f { 0, 1, 0 }, Vec3f { 0, 0, -1 } },
    Pair<Vec3f, Vec3f> { Vec3f { 0, -1, 0 }, Vec3f { 0, 0, 1 } },
    Pair<Vec3f, Vec3f> { Vec3f { 0, 0, 1 }, Vec3f { 0, 1, 0 } },
    Pair<Vec3f, Vec3f> { Vec3f { 0, 0, -1 }, Vec3f { 0, 1, 0 } }
};

static const Name s_nameTextureDefault = NAME("<unnamed texture>");

#pragma region Render commands

struct RENDER_COMMAND(CreateTextureGpuImage)
    : RenderCommand
{
    Handle<TextureAsset> textureAsset;
    ResourceHandle resourceHandle;
    ResourceState initialState;
    GpuImageRef image;
    bool uploadTextureData;

    RENDER_COMMAND(CreateTextureGpuImage)(
        Handle<TextureAsset>&& textureAsset,
        ResourceHandle&& resourceHandle,
        ResourceState initialState,
        GpuImageRef image,
        bool uploadTextureData)
        : textureAsset(std::move(textureAsset)),
          resourceHandle(std::move(resourceHandle)),
          initialState(initialState),
          image(std::move(image)),
          uploadTextureData(uploadTextureData)
    {
        Assert(this->image.IsValid());
    }

    virtual ~RENDER_COMMAND(CreateTextureGpuImage)() override = default;

    virtual RendererResult operator()() override
    {
        AssertDebug(!image->IsCreated());
        Assert(image->Create());

        if (uploadTextureData)
        {
            Assert(textureAsset && resourceHandle);

            TextureData* textureData = textureAsset->GetTextureData();
            Assert(textureData != nullptr);

            const TextureDesc& textureDesc = textureAsset->GetTextureDesc();

            ByteBuffer const* imageData = &textureData->imageData;
            LinkedList<ByteBuffer> placeholderBuffers;

            if (textureDesc != image->GetTextureDesc())
            {
                HYP_LOG(Streaming, Warning, "Streamed texture data TextureDesc not equal to Image's TextureDesc!");
            }

            if (imageData->Size() != image->GetByteSize())
            {
                HYP_LOG(Streaming, Warning, "Streamed texture data buffer size mismatch for texture asset {}! Expected: {}, Got: {}",
                    textureAsset->GetName(), image->GetByteSize(), imageData->Size());

                // throw an error in debug mode
                //AssertDebug(false, "Streamed texture data buffer size mismatch!");

                // fill some placeholder data with zeros so we don't crash
                ByteBuffer* placeholderBuffer = &placeholderBuffers.EmplaceBack();
                placeholderBuffer->SetSize(image->GetByteSize());

                const TextureFormat nonSrgbFormat = ChangeFormatSrgb(image->GetTextureFormat(), false);

                switch (textureAsset->GetTextureDesc().type)
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
            stagingBuffer->SetDebugName(NAME_FMT("Texture_StagingBuffer_{}", textureAsset->GetName().IsValid() ? textureAsset->GetName() : NAME("Invalid")));
            HYP_GFX_CHECK(stagingBuffer->Create());
            stagingBuffer->Copy(imageData->Size(), imageData->Data());

            HYP_DEFER({ SafeDelete(std::move(stagingBuffer)); });

            FrameBase* frame = g_renderBackend->GetCurrentFrame();

            // Needs to be done after rendering; otherwise might try to insert barriers during a render pass.
            RenderQueue& renderQueue = frame->postRenderQueue;

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
            RenderQueue& renderQueue = frame->postRenderQueue;

            // Transition to initial state
            renderQueue << InsertBarrier(image, initialState);
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
    : AssetObject(s_nameTextureDefault),
      m_assetReference(CreateObject<TextureAsset>(s_nameTextureDefault, textureDesc, TextureData {}))
{
}

Texture::Texture(const TextureDesc& textureDesc, const TextureData& textureData)
    : AssetObject(s_nameTextureDefault),
      m_assetReference(CreateObject<TextureAsset>(s_nameTextureDefault, textureDesc, textureData))
{
}

Texture::Texture(const Handle<TextureAsset>& asset)
    : AssetObject(s_nameTextureDefault),
      m_assetReference(asset)
{
}

Texture::~Texture()
{
    if (m_gpuImage)
    {
        SafeDelete(std::move(m_gpuImage));
    }
}

void Texture::Init()
{
    AddDelegateHandler(g_engineDriver->GetDelegates().OnShutdown.Bind([this]()
        {
            SafeDelete(std::move(m_gpuImage));
        }));

    if (const Handle<TextureAsset>& asset = GetAsset())
    {
        if (!asset->IsRegistered())
        {
            if (Result renameResult = asset->Rename(m_name); renameResult.HasError())
            {
                HYP_LOG(Assets, Error, "Failed to rename texture asset!", renameResult.GetError().GetMessage());
            }

            g_assetManager->GetAssetRegistry()->RegisterAsset("$Memory/Media/Textures", asset);
        }

        m_assetReference = TAssetReference(asset);
    }

    m_gpuImage = g_renderBackend->MakeImage(GetTextureDesc());

    if (m_name.IsValid())
    {
        m_gpuImage->SetDebugName(m_name);
    }
    
    ResourceHandle resourceHandle;
    bool uploadTextureData = false;

    Handle<TextureAsset> textureAsset = GetAsset();

    if (textureAsset)
    {
        resourceHandle = ResourceHandle(*textureAsset->GetResource());

        const TextureData* textureData = textureAsset->GetTextureData();
        uploadTextureData = textureData && !textureData->imageData.Empty();
    }

    PUSH_RENDER_COMMAND(
        CreateTextureGpuImage,
        std::move(textureAsset),
        std::move(resourceHandle),
        RS_SHADER_RESOURCE,
        m_gpuImage,
        uploadTextureData);
    
    AssetObject::Init();

    SetReady(true);
}

Result Texture::Rename(Name name)
{
    if (name == m_name)
    {
        return {};
    }

    if (Result result = AssetObject::Rename(name); result.HasError())
    {
        return result;
    }

    const Handle<TextureAsset>& asset = GetAsset();

    if (asset.IsValid())
    {
        if (!asset->IsRegistered())
        {
            if (Result renameResult = asset->Rename(m_name); renameResult.HasError())
            {
                HYP_LOG(Assets, Error, "Failed to rename texture asset!", renameResult.GetError().GetMessage());
            }

            if (IsInitCalled())
            {
                g_assetManager->GetAssetRegistry()->RegisterAsset("$Memory/Media/Textures", asset);
            }
        }
    }

    return {};
}

const Handle<TextureAsset>& Texture::GetAsset() const
{
    return m_assetReference.Resolve();
}

const TextureDesc& Texture::GetTextureDesc() const
{
    static const TextureDesc s_defaultTextureDesc {};

    const Handle<TextureAsset>& asset = GetAsset();
    return asset ? asset->GetTextureDesc() : s_defaultTextureDesc;
}

void Texture::SetTextureDesc(const TextureDesc& textureDesc)
{
    TextureDesc currentTextureDesc = GetTextureDesc();

    if (currentTextureDesc == textureDesc)
    {
        return;
    }

    Handle<TextureAsset> asset = GetAsset();

    // create new asset
    if (asset != nullptr)
    {
        Handle<TextureAsset> prevAsset = asset;

        Handle<AssetPackage> package = prevAsset->GetPackage();

        ResourceHandle resourceHandle(*prevAsset->GetResource());

        TextureData textureData = std::move(*prevAsset->GetTextureData());

        asset = CreateObject<TextureAsset>(prevAsset->GetName(), textureDesc, textureData);

        if (package.IsValid())
        {
            package->RemoveAssetObject(prevAsset);
            package->AddAssetObject(asset);
        }
    }
    else
    {
        asset = CreateObject<TextureAsset>(GetName(), textureDesc, TextureData {});
    }

    if (IsInitCalled())
    {
        if (!asset->IsRegistered())
        {
            g_assetManager->GetAssetRegistry()->RegisterAsset("$Memory/Media/Textures", asset);
        }
    }

    m_assetReference = TAssetReference<TextureAsset>(asset);
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
    gpuBuffer->SetDebugName(NAME_FMT("Texture_Readback_StagingBuffer_{}", GetName().IsValid() ? GetName() : NAME("Invalid")));
    HYP_GFX_ASSERT(gpuBuffer->Create());
    gpuBuffer->Map();

    UniquePtr<SingleTimeCommands> singleTimeCommands = g_renderBackend->GetSingleTimeCommands();

    singleTimeCommands->Push([this, &gpuBuffer](RenderQueue& renderQueue)
        {
            const ResourceState previousResourceState = m_gpuImage->GetResourceState();

            renderQueue << InsertBarrier(m_gpuImage, RS_COPY_SRC);
            renderQueue << InsertBarrier(gpuBuffer, RS_COPY_DST);

            renderQueue << CopyImageToBuffer(m_gpuImage, gpuBuffer);

            if (previousResourceState != RS_UNDEFINED && previousResourceState != RS_PRE_INITIALIZED)
            {
                renderQueue << InsertBarrier(m_gpuImage, previousResourceState);
            }
            else
            {
                renderQueue << InsertBarrier(m_gpuImage, RS_SHADER_RESOURCE);
            }
        });

    RendererResult result = singleTimeCommands->Execute();

    if (result.HasError())
    {
        HYP_LOG(Rendering, Error, "Failed to readback texture data! {}", result.GetError().GetMessage());

        return;
    }

    outByteBuffer.SetSize(gpuBuffer->Size());
    gpuBuffer->Read(outByteBuffer.Size(), outByteBuffer.Data());
    gpuBuffer.Reset();
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
    stagingBuffer->SetDebugName(NAME_FMT("Texture_EnqueueReadback_StagingBuffer_{}", GetName().IsValid() ? GetName() : NAME("Invalid")));
    HYP_GFX_ASSERT(stagingBuffer->Create());
    stagingBuffer->Map();

    renderQueue << InsertBarrier(m_gpuImage, RS_COPY_SRC);
    renderQueue << InsertBarrier(stagingBuffer, RS_COPY_DST);

    renderQueue << CopyImageToBuffer(m_gpuImage, stagingBuffer);

    if (previousResourceState != RS_UNDEFINED && previousResourceState != RS_PRE_INITIALIZED)
    {
        renderQueue << InsertBarrier(m_gpuImage, previousResourceState);
    }
    else
    {
        renderQueue << InsertBarrier(m_gpuImage, RS_SHADER_RESOURCE);
    }

    currentFrame->OnFrameEnd
        .Bind([gpuImageRef = MakeStrongRef(m_gpuImage), /* hold a strong reference to our buffer to ensure it is kept alive */
                  stagingBuffer = std::move(stagingBuffer),
                  callback = std::move(callback)](...) mutable
            {
                TByteBuffer<RenderAllocator> byteBuffer;
                byteBuffer.SetSize(stagingBuffer->Size());

                HYP_LOG_TEMP("Reading {} bytes from staging buffer", byteBuffer.Size());

                stagingBuffer->Read(byteBuffer.Size(), byteBuffer.Data());

                SafeDelete(std::move(stagingBuffer));
                SafeDelete(std::move(gpuImageRef));

                callback(std::move(byteBuffer));
            })
        .Detach();
}

Vec4f Texture::Sample(Vec3f uvw, uint32 faceIndex)
{
    if (!IsReady())
    {
        HYP_LOG_ONCE(Texture, Warning, "Texture is not ready, cannot sample");

        HYP_BREAKPOINT;

        return Vec4f::Zero();
    }

    if (faceIndex >= NumFaces())
    {
        HYP_LOG_ONCE(Texture, Error, "Face index out of bounds: {} >= {}", faceIndex, NumFaces());

        HYP_BREAKPOINT;

        return Vec4f::Zero();
    }

    const Handle<TextureAsset>& asset = GetAsset();

    if (!asset)
    {
        HYP_LOG_ONCE(Texture, Warning, "Texture asset is not valid, cannot sample");

        HYP_BREAKPOINT;

        return Vec4f::Zero();
    }

    ResourceHandle resourceHandle = ResourceHandle(*asset->GetResource());

    if (!resourceHandle)
    {
        HYP_LOG_ONCE(Texture, Warning, "Texture resource handle is not valid, cannot sample");

        HYP_BREAKPOINT;

        return Vec4f::Zero();
    }

    TextureData* textureData = asset->GetTextureData();
    Assert(textureData != nullptr);

    const TextureDesc& textureDesc = asset->GetTextureDesc();

    Vec3u coord = {
        uint32(MathUtil::Abs(std::fmodf(uvw.x, 1.0f)) * float(textureDesc.extent.x - 1) + 0.5f),
        uint32(MathUtil::Abs(std::fmodf(uvw.y, 1.0f)) * float(textureDesc.extent.y - 1) + 0.5f),
        uint32(MathUtil::Abs(std::fmodf(uvw.z, 1.0f)) * float(textureDesc.extent.z - 1) + 0.5f)
    };

    const uint32 bytesPerComponent = BytesPerComponent(textureDesc.format);

    if (bytesPerComponent != 1)
    {
        HYP_LOG_ONCE(Texture, Warning, "Unsupported bytes per component to use with Sample(): {}", bytesPerComponent);

        HYP_BREAKPOINT;

        return Vec4f::Zero();
    }

    const uint32 numComponents = NumComponents(textureDesc.format);

    const uint32 index = faceIndex * (textureDesc.extent.x * textureDesc.extent.y * textureDesc.extent.z * bytesPerComponent * numComponents)
        + coord.z * (textureDesc.extent.x * textureDesc.extent.y * bytesPerComponent * numComponents)
        + coord.y * (textureDesc.extent.x * bytesPerComponent * numComponents)
        + coord.x * bytesPerComponent * numComponents;

    if (index + (bytesPerComponent * numComponents) > textureData->imageData.Size())
    {
        HYP_LOG_ONCE(Texture, Warning, "Sample() call would attempt to read out of bounds of data for Texture {} ({})!\n"
                                       "Texture format: {}, Texel index: {}, texture data buffer size: {}, coord: {}, dimensions: {}, num faces: {}, bytes per component: {}, num components: {}",
            GetName(), Id(),
            EnumToString(textureDesc.format),
            index, textureData->imageData.Size(),
            coord, textureDesc.extent, NumFaces(),
            bytesPerComponent, numComponents);

        HYP_BREAKPOINT;

        return Vec4f::Zero();
    }

    if ((textureDesc.format >= TF_R16F && textureDesc.format <= TF_RGBA32F) || textureDesc.format == TF_R11G11B10F)
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
    else if (textureDesc.format >= TF_R16 && textureDesc.format <= TF_RGBA16)
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
    else if (textureDesc.format >= TF_R32 && textureDesc.format <= TF_RGBA32)
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
        if (IsSrgbFormat(textureDesc.format))
        {
            // convert from sRGB to linear
            switch (numComponents)
            {
            case 1:
                return PixelReference<ubyte, 1, true>(textureData->imageData.Data() + index).GetRGBA();
            case 2:
                return PixelReference<ubyte, 2, true>(textureData->imageData.Data() + index).GetRGBA();
            case 3:
                return PixelReference<ubyte, 3, true>(textureData->imageData.Data() + index).GetRGBA();
            case 4:
                return PixelReference<ubyte, 4, true>(textureData->imageData.Data() + index).GetRGBA();
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
    }

    HYP_LOG_ONCE(Texture, Error, "Unsupported texture format to read on CPU: {}", int(textureDesc.format));

    HYP_BREAKPOINT;

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
