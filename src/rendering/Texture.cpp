/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/Texture.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/GpuImage.hpp>
#include <rendering/Sampler.hpp>
#include <rendering/RenderQueue.hpp>
#include <rendering/Frame.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderHelpers.hpp>

#include <rendering/util/SafeDeleter.hpp>
#include <rendering/util/TextureMipmapRenderer.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/TextureAsset.hpp>

#include <core/utilities/DeferredScope.hpp>

// for EnumToString
#include <core/reflection/Enum.hpp>

#include <util/img/Bitmap.hpp>

#include <engine/EngineDriver.hpp>

#include <Texture.generated.inl>

#include <stb_image_resize.h>

namespace Hyperion {

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

struct CreateTextureGpuImage : RenderCommand
{
    Handle<TextureAsset> textureAsset;
    ResourceHandle resourceHandle;
    ResourceState initialState;
    GpuImageRef image;
    bool uploadTextureData;

    CreateTextureGpuImage(
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

        if (uploadTextureData)
        {
            AssertDebug(CheckImageData());
        }
    }

    virtual ~CreateTextureGpuImage() override = default;

    bool CheckImageData() const
    {
        if (!textureAsset || !image)
        {
            return false;
        }

        const TextureData* textureData = textureAsset->GetTextureData();
        if (!textureData)
        {
            return false;
        }

        const TextureDesc& textureDesc = textureAsset->GetTextureDesc();

        const uint32 mip0Size = textureDesc.HasStoredMips()
            ? textureDesc.mipOffsets[0]
            : uint32(textureData->imageData.Size());

        ConstByteView mip0Slice = textureData->imageData.ToByteView().Slice(0, mip0Size);

        if (textureDesc != image->GetTextureDesc())
        {
            HYP_LOG(Streaming, Warning, "Streamed texture data TextureDesc not equal to Image's TextureDesc!");
        }

        if (mip0Slice.Size() != image->GetByteSize())
        {
            HYP_LOG(Streaming, Warning, "Streamed texture data buffer size mismatch for texture asset {}! Expected: {}, Got: {}",
                textureAsset->GetName(), image->GetByteSize(), mip0Slice.Size());

            return false;
        }

        return true;
    }

    virtual RendererResult operator()() override
    {
        AssertDebug(!image->IsCreated());
        Assert(image->Create());

        if (uploadTextureData)
        {
            const TextureData* textureData = textureAsset->GetTextureData();
            Assert(textureData != nullptr);

            const TextureDesc& textureDesc = textureAsset->GetTextureDesc();
            const ByteBuffer* imageData = &textureData->imageData;

            Span<const uint32> mipOffsets = textureDesc.mipOffsets.ToSpan();

            Optional<ByteBuffer> placeholderBuffer;

            if (!CheckImageData())
            {
                // throw an error in debug mode
                AssertDebug(false, "Image contains invalid data!");

                static const uint32 s_placeholderMipOffsets[TextureDesc::MaxMips] { 0 };
                mipOffsets = { s_placeholderMipOffsets, TextureDesc::MaxMips };

                // fill some placeholder data with zeros so we don't crash
                imageData = &placeholderBuffer.Emplace();
                placeholderBuffer->SetSize(image->GetByteSize());

                const TextureFormat nonSrgbFormat = TextureUtils::ChangeFormatSrgb(image->GetTextureFormat(), false);

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
            }

            GpuBufferRef stagingBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, imageData->Size());
            stagingBuffer->SetDebugName(NAME_FMT("Texture_StagingBuffer_{}", textureAsset->GetName().IsValid() ? textureAsset->GetName() : NAME("Invalid")));
            HYP_GFX_CHECK(stagingBuffer->Create());
            stagingBuffer->Copy(imageData->Size(), imageData->Data());

            HYP_DEFER({ SafeDelete(std::move(stagingBuffer)); });

            Frame* frame = g_renderBackend->GetCurrentFrame();

            RenderQueue& renderQueue = frame->preRenderQueue;

            renderQueue << InsertBarrier(image, RS_COPY_DST);

            if (textureDesc.HasMipMaps() && imageData != placeholderBuffer.TryGet())
            {
                const bool hasPreGeneratedMips = textureDesc.mipOffsets[0] != 0;

                if (hasPreGeneratedMips)
                {
                    const uint32 numMips = textureDesc.NumMips();
                    const uint32 numArrayLayers = textureDesc.NumArrayLayers();

                    for (uint8 mipIndex = 0; mipIndex < uint8(numMips); mipIndex++)
                    {
                        const uint32 mipSize = textureDesc.GetMipByteSize(mipIndex);

                        uint32 mipBlockStart = 0;
                        if (mipIndex != 0)
                        {
                            mipBlockStart = mipOffsets[mipIndex - 1];

                            AssertDebug(mipBlockStart != 0);
                        }

                        for (uint16 layerIndex = 0; layerIndex < uint16(numArrayLayers); layerIndex++)
                        {
                            uint32 finalOffset = mipBlockStart + (layerIndex * mipSize);

                            AssertDebug(finalOffset + mipSize <= stagingBuffer->Size());

                            renderQueue << CopyBufferToImage(
                                stagingBuffer,
                                image,
                                /* byteOffset */ finalOffset,
                                /* dstMipIndex */ mipIndex,
                                /* dstArrayLayer */ layerIndex);
                        }
                    }
                }
                // else
                //{
                //     // runtime mip generation fallback
                //     const uint32 numArrayLayers = textureDesc.NumArrayLayers();
                //     const uint32 mipSize = textureDesc.GetMipByteSize(0);

                //    for (uint16 layerIndex = 0; layerIndex < uint16(numArrayLayers); layerIndex++)
                //    {
                //        renderQueue << CopyBufferToImage(
                //            stagingBuffer,
                //            image,
                //            layerIndex * mipSize, // Offset assumes simple Layer packing for Mip 0
                //            0,
                //            layerIndex);
                //    }

                //    renderQueue << GenerateMipmaps(image);
                //}
            }
            else
            {
                // No mips, just base level
                renderQueue << CopyBufferToImage(stagingBuffer, image);
            }

            renderQueue << InsertBarrier(image, initialState);
        }
        else if (initialState != RS_UNDEFINED)
        {
            Frame* frame = g_renderBackend->GetCurrentFrame();
            RenderQueue& renderQueue = frame->preRenderQueue;

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
      m_assetReference(CreateObject<TextureAsset>(s_nameTextureDefault, textureDesc))
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

void Texture::SetAsset(const Handle<TextureAsset>& asset)
{
    if (!asset && !m_assetReference.IsValid())
    {
        // both invalid
        return;
    }

    if (asset && IsInitCalled())
    {
        InitObject(asset);

        if (!asset->IsRegistered())
        {
            g_assetManager->GetAssetRegistry()->RegisterAsset("$Memory/Media/Textures", asset);
        }
    }

    m_assetReference = TAssetReference<TextureAsset>(asset);
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

        // @NOTE: Don't use std::move with prev data, the texture may be in use elsewhere (e.g uploading in render command)
        TextureData newTextureData = *prevAsset->GetTextureData();

        asset = CreateObject<TextureAsset>(prevAsset->GetName(), textureDesc, newTextureData);

        if (package.IsValid())
        {
            package->RemoveAssetObject(prevAsset);
            package->AddAssetObject(asset);
        }
    }
    else
    {
        asset = CreateObject<TextureAsset>(GetName(), textureDesc);
    }

    if (IsInitCalled())
    {
        InitObject(asset);

        if (!asset->IsRegistered())
        {
            g_assetManager->GetAssetRegistry()->RegisterAsset("$Memory/Media/Textures", asset);
        }
    }

    m_assetReference = TAssetReference<TextureAsset>(asset);
}

void Texture::GenerateMipmaps(TextureDesc& desc, TextureData& data)
{
    const uint32 numMipLevels = desc.NumMips();
    const uint32 numArrayLayers = desc.NumArrayLayers();

    if (numMipLevels <= 1)
    {
        return;
    }

    const bool canGenerateMips = (desc.format >= TF_R16F && desc.format <= TF_RGBA32F)
        || (desc.format >= TF_R8 && desc.format <= TF_RGBA8);

    if (!canGenerateMips)
    {
        return;
    }

    // base mip size
    const uint32 baseMipSize = desc.GetMipByteSize(0) * numArrayLayers;
    AssertDebug(data.imageData.Size() == baseMipSize);

    uint32 totalSize = baseMipSize;

    for (uint32 mip = 1; mip < numMipLevels; mip++)
    {
        totalSize += desc.GetMipByteSize(mip) * numArrayLayers;
    }

    data.imageData.SetSize(totalSize);

    uint32 srcBlockStart = 0;
    uint32 dstWriteOffset = baseMipSize;

    for (uint32 dstMipLevel = 1; dstMipLevel < numMipLevels; dstMipLevel++)
    {
        uint32 srcMipLevel = dstMipLevel - 1;

        const uint32 srcMipSize = desc.GetMipByteSize(srcMipLevel);
        const uint32 dstMipSize = desc.GetMipByteSize(dstMipLevel);

        const Vec3u srcExtent = desc.GetMipExtent(srcMipLevel);
        const Vec3u dstExtent = desc.GetMipExtent(dstMipLevel);

        // mipOffsets stores the start of the block for given mip level but skips the first elem
        // so we can check if we have pregenerated mips by doing mipOffsets[0] != 0
        desc.mipOffsets[dstMipLevel - 1] = dstWriteOffset;

        uint32 currentBlockStart = dstWriteOffset;

        for (uint32 layer = 0; layer < numArrayLayers; layer++)
        {
            uint32 readOffset = srcBlockStart + (layer * srcMipSize);

            ConstByteView srcView = data.imageData.ToByteView().Slice(readOffset, readOffset + srcMipSize);

            ByteBuffer tempBuffer;
            tempBuffer.SetSize(dstMipSize, false);

            int result = 0;
            const int numChannels = TextureUtils::NumComponents(desc.format);

            if (desc.format >= TF_R16F && desc.format <= TF_RGBA32F)
            {
                AssertDebug(srcView.Size() % sizeof(float) == 0 && tempBuffer.Size() % sizeof(float) == 0);

                result = stbir_resize_float(
                    reinterpret_cast<const float*>(srcView.Data()),
                    srcExtent.x, srcExtent.y, 0,
                    reinterpret_cast<float*>(tempBuffer.Data()),
                    dstExtent.x, dstExtent.y, 0,
                    numChannels);
            }
            else if (desc.IsSrgb())
            {
                result = stbir_resize_uint8_srgb(
                    srcView.Data(), srcExtent.x, srcExtent.y, 0,
                    tempBuffer.Data(), dstExtent.x, dstExtent.y, 0,
                    numChannels, numChannels == 4 ? 3 : -1, 0);
            }
            else
            {
                result = stbir_resize_uint8(
                    srcView.Data(), srcExtent.x, srcExtent.y, 0,
                    tempBuffer.Data(), dstExtent.x, dstExtent.y, 0,
                    numChannels);
            }

            if (result == 0)
            {
                HYP_LOG(Texture, Error, "Mip generation failed at level {} layer {}", dstMipLevel, layer);
                return;
            }

            data.imageData.Write(dstMipSize, dstWriteOffset, tempBuffer.Data());

            dstWriteOffset += dstMipSize;
        }

        srcBlockStart = currentBlockStart;
    }
}

void Texture::Readback(ByteBuffer& outByteBuffer)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

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
    AssertOnThread(g_renderThread);

    AssertReady();

    Frame* currentFrame = g_renderBackend->GetCurrentFrame();

    // No current frame, fallback to blocking Readback() call.
    if (!currentFrame)
    {
        ByteBuffer byteBuffer;
        Readback(byteBuffer);

        callback(std::move(byteBuffer));

        return;
    }

    RenderQueue& renderQueue = currentFrame->preRenderQueue;

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

    DelegateHandler* delegateHandle = new DelegateHandler();
    *delegateHandle = currentFrame->OnFrameEnd
                          .Bind([delegateHandle, name = GetName(), gpuImageRef = MakeStrongRef(m_gpuImage), /* hold a strong reference to our buffer to ensure it is kept alive */
                                    stagingBuffer = MakeStrongRef(stagingBuffer),
                                    callback = std::move(callback)](...) mutable
                              {
                                  HYP_LOG(Texture, Debug, "Finish readback for texture {}", name);

                                  ByteBuffer byteBuffer;
                                  byteBuffer.SetSize(stagingBuffer->Size());

                                  stagingBuffer->Read(byteBuffer.Size(), byteBuffer.Data());

                                  SafeDelete(std::move(stagingBuffer));
                                  SafeDelete(std::move(gpuImageRef));

                                  callback(std::move(byteBuffer));

                                  delete delegateHandle;
                              });

    SafeDelete(std::move(stagingBuffer));
}

Vec4f Texture::Sample(Vec3f uvw, uint32 faceIndex)
{
    if (!IsReady())
    {
        HYP_LOG_ONCE(Texture, Warning, "Texture is not ready, cannot sample");

        HYP_BREAKPOINT;

        return Vec4f::Zero();
    }

    if (faceIndex >= NumArrayLayers())
    {
        HYP_LOG_ONCE(Texture, Error, "Face index out of bounds: {} >= {}", faceIndex, NumArrayLayers());

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

    const TextureData* textureData = asset->GetTextureData();
    Assert(textureData != nullptr);

    const TextureDesc& textureDesc = asset->GetTextureDesc();

    Vec3u coord = {
        uint32(MathUtil::Abs(std::fmodf(uvw.x, 1.0f)) * float(textureDesc.extent.x - 1) + 0.5f),
        uint32(MathUtil::Abs(std::fmodf(uvw.y, 1.0f)) * float(textureDesc.extent.y - 1) + 0.5f),
        uint32(MathUtil::Abs(std::fmodf(uvw.z, 1.0f)) * float(textureDesc.extent.z - 1) + 0.5f)
    };

    const uint32 bytesPerComponent = TextureUtils::BytesPerComponent(textureDesc.format);

    if (bytesPerComponent != 1)
    {
        HYP_LOG_ONCE(Texture, Warning, "Unsupported bytes per component to use with Sample(): {}", bytesPerComponent);

        return Vec4f::Zero();
    }

    const uint32 numComponents = TextureUtils::NumComponents(textureDesc.format);

    const uint32 index = faceIndex * (textureDesc.extent.x * textureDesc.extent.y * textureDesc.extent.z * bytesPerComponent * numComponents)
        + coord.z * (textureDesc.extent.x * textureDesc.extent.y * bytesPerComponent * numComponents)
        + coord.y * (textureDesc.extent.x * bytesPerComponent * numComponents)
        + coord.x * bytesPerComponent * numComponents;

    const uint32 mip0Size = textureDesc.HasStoredMips()
        ? textureDesc.mipOffsets[0]
        : uint32(textureData->imageData.Size());

    if (index + (bytesPerComponent * numComponents) > mip0Size)
    {
        HYP_LOG_ONCE(Texture, Warning,
            "Sample() call would attempt to read out of bounds of data for Texture {} ({})!\n"
            "Texture format: {}, Texel index: {}, texture data buffer size: {}, coord: {}, dimensions: {}, num faces: {}, bytes per component: {}, num components: {}",
            GetName(), Id(),
            EnumToString(textureDesc.format),
            index, mip0Size,
            coord, textureDesc.extent, NumArrayLayers(),
            bytesPerComponent, numComponents);

        return Vec4f::Zero();
    }

    if ((textureDesc.format >= TF_R16F && textureDesc.format <= TF_RGBA32F) || textureDesc.format == TF_R11G11B10F)
    {
        // FP format
        switch (numComponents)
        {
        case 1:
            return ConstPixelReference<float, 1>(textureData->imageData.Data() + index).GetRGBA();
        case 2:
            return ConstPixelReference<float, 2>(textureData->imageData.Data() + index).GetRGBA();
        case 3:
            return ConstPixelReference<float, 3>(textureData->imageData.Data() + index).GetRGBA();
        case 4:
            return ConstPixelReference<float, 4>(textureData->imageData.Data() + index).GetRGBA();
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
            return ConstPixelReference<uint16, 1>(textureData->imageData.Data() + index).GetRGBA();
        case 2:
            return ConstPixelReference<uint16, 2>(textureData->imageData.Data() + index).GetRGBA();
        case 3:
            return ConstPixelReference<uint16, 3>(textureData->imageData.Data() + index).GetRGBA();
        case 4:
            return ConstPixelReference<uint16, 4>(textureData->imageData.Data() + index).GetRGBA();
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
            return ConstPixelReference<uint32, 1>(textureData->imageData.Data() + index).GetRGBA();
        case 2:
            return ConstPixelReference<uint32, 2>(textureData->imageData.Data() + index).GetRGBA();
        case 3:
            return ConstPixelReference<uint32, 3>(textureData->imageData.Data() + index).GetRGBA();
        case 4:
            return ConstPixelReference<uint32, 4>(textureData->imageData.Data() + index).GetRGBA();
        default:
            break;
        }
    }
    else
    {
        if (TextureUtils::IsSrgbFormat(textureDesc.format))
        {
            // convert from sRGB to linear
            switch (numComponents)
            {
            case 1:
                return ConstPixelReference<ubyte, 1, true>(textureData->imageData.Data() + index).GetRGBA();
            case 2:
                return ConstPixelReference<ubyte, 2, true>(textureData->imageData.Data() + index).GetRGBA();
            case 3:
                return ConstPixelReference<ubyte, 3, true>(textureData->imageData.Data() + index).GetRGBA();
            case 4:
                return ConstPixelReference<ubyte, 4, true>(textureData->imageData.Data() + index).GetRGBA();
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
                return ConstPixelReference<ubyte, 1>(textureData->imageData.Data() + index).GetRGBA();
            case 2:
                return ConstPixelReference<ubyte, 2>(textureData->imageData.Data() + index).GetRGBA();
            case 3:
                return ConstPixelReference<ubyte, 3>(textureData->imageData.Data() + index).GetRGBA();
            case 4:
                return ConstPixelReference<ubyte, 4>(textureData->imageData.Data() + index).GetRGBA();
            default:
                break;
            }
        }
    }

    HYP_LOG_ONCE(Texture, Error, "Unsupported texture format to read on CPU: {}", int(textureDesc.format));

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

} // namespace Hyperion
