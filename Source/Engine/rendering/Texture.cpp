/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/CommandRecorder.hpp>
#include <rendering/Texture.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/GpuImage.hpp>
#include <rendering/Sampler.hpp>
#include <rendering/Frame.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderHelpers.hpp>
#include <rendering/TextureViewCache.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/BlobStorage.hpp>

#include <Core/utilities/DeferredScope.hpp>

#include <Core/threading/ThreadSignal.hpp>

// for EnumToString
#include <Core/reflection/Enum.hpp>

#include <util/img/Bitmap.hpp>

#include <engine/EngineDriver.hpp>

#include <Texture.generated.inl>

#include <stb_image_resize.h>

namespace Hyperion {

class Texture;

extern ThreadSignal g_renderInitSignal;

const FixedArray<Pair<Vec3f, Vec3f>, 6> Texture::s_cubemapDirections = {
    Pair<Vec3f, Vec3f> { Vec3f { 1, 0, 0 }, Vec3f { 0, 1, 0 } },
    Pair<Vec3f, Vec3f> { Vec3f { -1, 0, 0 }, Vec3f { 0, 1, 0 } },
    Pair<Vec3f, Vec3f> { Vec3f { 0, 1, 0 }, Vec3f { 0, 0, -1 } },
    Pair<Vec3f, Vec3f> { Vec3f { 0, -1, 0 }, Vec3f { 0, 0, 1 } },
    Pair<Vec3f, Vec3f> { Vec3f { 0, 0, 1 }, Vec3f { 0, 1, 0 } },
    Pair<Vec3f, Vec3f> { Vec3f { 0, 0, -1 }, Vec3f { 0, 1, 0 } }
};

static const Name s_nameTextureDefault = NAME("<unnamed texture>");

static bool CheckImageData(Texture& texture, GpuImage& image)
{
    ConstByteView imageData = texture.GetImageData();

    if (!imageData.Data())
    {
        HYP_LOG(Streaming, Error, "No image data for texture");

        return false;
    }

    const TextureDesc& textureDesc = texture.GetTextureDesc();

    const uint32 largestMipSize = textureDesc.HasStoredMips()
        ? textureDesc.mipOffsets[0]
        : uint32(textureDesc.GetByteSize());

    ConstByteView largestMipData = ConstByteView(imageData.Data(), largestMipSize);

    if (textureDesc != image.GetTextureDesc())
    {
        HYP_LOG(Streaming, Warning, "Streamed texture data TextureDesc not equal to Image's TextureDesc!");
    }

    if (largestMipData.Size() != image.GetByteSize())
    {
        HYP_LOG(Streaming, Warning, "Streamed texture data buffer size mismatch for texture asset {}! Expected: {}, Got: {}",
            texture.GetName(), image.GetByteSize(), largestMipData.Size());

        return false;
    }

    return true;
}

static RendererResult CreateGpuImage(Texture& texture, GpuImage& image, ResourceState initialState, bool uploadTextureData)
{
    if (!IsOnThread(g_renderThread))
    {
        // we need the renderer to be ready before we can create the gpu image
        g_renderInitSignal.Wait();
    }

    CheckResultOrReturn(image.Create());

    CommandRecorder& cr = RI.commandRecorderAllocator.GetCommandRecorder();

    // Cannot be in a render pass while transitioning images
    cr << SetCurrentFramebuffer(nullptr);

    if (uploadTextureData)
    {
        ConstByteView imageData = texture.GetImageData();

        const TextureDesc& textureDesc = texture.GetTextureDesc();

        Span<const uint32> mipOffsets = textureDesc.mipOffsets.ToSpan();

        Optional<ByteBuffer> placeholderBuffer;

        if (!CheckImageData(texture, image))
        {
            // throw an error in debug mode
            AssertDebug(false, "Image contains invalid data!");

            static const uint32 s_placeholderMipOffsets[TextureDesc::MaxMips] { 0 };
            mipOffsets = { s_placeholderMipOffsets, TextureDesc::MaxMips };

            // fill some placeholder data with zeros so we don't crash
            imageData = placeholderBuffer.Emplace().ToByteView();
            placeholderBuffer->SetSize(image.GetByteSize());

            const TextureFormat nonSrgbFormat = TextureUtils::ChangeFormatSRGB(image.GetTextureFormat(), false);

            switch (texture.GetTextureDesc().type)
            {
            case TextureType::Texture2D:
                switch (nonSrgbFormat)
                {
                case TextureFormat::R8:
                    FillPlaceholderBuffer_Tex2D<TextureFormat::R8>(image.GetExtent().GetXY(), *placeholderBuffer);
                    break;
                case TextureFormat::RGBA8:
                    FillPlaceholderBuffer_Tex2D<TextureFormat::RGBA8>(image.GetExtent().GetXY(), *placeholderBuffer);
                    break;
                case TextureFormat::RGBA16F:
                    FillPlaceholderBuffer_Tex2D<TextureFormat::RGBA16F>(image.GetExtent().GetXY(), *placeholderBuffer);
                    break;
                case TextureFormat::RGBA32F:
                    FillPlaceholderBuffer_Tex2D<TextureFormat::RGBA32F>(image.GetExtent().GetXY(), *placeholderBuffer);
                    break;
                default:
                    // no FillPlaceholderBuffer method defined
                    break;
                }
                break;
            case TextureType::Cubemap:
                switch (nonSrgbFormat)
                {
                case TextureFormat::R8:
                    FillPlaceholderBuffer_Cubemap<TextureFormat::R8>(image.GetExtent().GetXY(), *placeholderBuffer);
                    break;
                case TextureFormat::RGBA8:
                    FillPlaceholderBuffer_Cubemap<TextureFormat::RGBA8>(image.GetExtent().GetXY(), *placeholderBuffer);
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

        // @TODO use staging buffer pool. change staging buffer pool to use GetFrameCounter(), make it thread-safe.
        // should recycle same way constants allocator recycles blocks etc.
        GpuBufferRef stagingBuffer = RI.MakeGpuBuffer(GpuBufferType::StagingBuffer, imageData.Size());
#if HYP_DEBUG_MODE
        stagingBuffer->SetDebugName(NAME_FMT("Texture_StagingBuffer_{}", texture.GetName().IsValid() ? texture.GetName() : NAME("Invalid")));
#endif

        CheckResultOrReturn(stagingBuffer->Create());
        stagingBuffer->Copy(imageData.Size(), imageData.Data());

        HYP_DEFER({ EnqueueDeletion(std::move(stagingBuffer)); });

        cr << InsertBarrier(stagingBuffer, RS_COPY_SRC);
        cr << InsertBarrier(&image, RS_COPY_DST);

        bool hasMips = textureDesc.HasMipMaps() && !placeholderBuffer.HasValue();

        if (hasMips && !textureDesc.mipOffsets[0])
        {
            HYP_LOG(Assets, Warning, "Mip data missing for texture {}", texture.GetName());

            hasMips = false;
        }

        if (hasMips)
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

                for (uint32 layerIndex = 0; layerIndex < numArrayLayers; layerIndex++)
                {
                    uint32 finalOffset = mipBlockStart + (mipSize * layerIndex);

                    AssertDebug(finalOffset + mipSize <= stagingBuffer->Size());

                    cr << CopyBufferToImage(
                        stagingBuffer,
                        &image,
                        /* byteOffset */ finalOffset,
                        /* dstMipIndex */ mipIndex,
                        /* dstArrayLayer */ layerIndex);
                }
            }
        }
        else
        {
            // No mips, just base level
            cr << CopyBufferToImage(stagingBuffer, &image);
        }

        cr << InsertBarrier(&image, initialState);
    }
    else if (initialState != RS_UNDEFINED)
    {
        // Transition to initial state
        cr << InsertBarrier(&image, initialState);
    }

    cr.Done();

    return {};
}

#pragma region Texture

Texture::Texture()
    : Texture(TextureDesc {
          TextureType::Texture2D,
          TextureFormat::RGBA8,
          Vec3u { 1, 1, 1 },
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE
      })
{
}

Texture::Texture(const TextureDesc& textureDesc)
    : AssetObject(s_nameTextureDefault),
      m_textureDesc(textureDesc)
{
}

Texture::Texture(const TextureDesc& textureDesc, ConstByteView imageData)
    : AssetObject(s_nameTextureDefault),
      m_textureDesc(textureDesc)
{
    AllocateBlobData(m_imageData, imageData.Data(), imageData.Size(), 1);
}

Texture::~Texture()
{
    LockWriter(/* doInitialize */ false);

    if (m_gpuImage.IsValid())
    {
        if (RI.textureViewCache != nullptr)
        {
            RI.textureViewCache->RemoveTexture(this);
        }

        EnqueueDeletion(std::move(m_gpuImage));
    }

    FreeBlobData(m_imageData);
}

RendererResult Texture::Create()
{
    auto readScope = GetReadScope();

    const bool uploadTextureData = GetImageData().Size() > 0;

    if (!m_gpuImage.IsValid())
    {
        Assert(m_textureDesc.extent.Volume() > 0);

        GpuImageRef gpuImage = RI.MakeImage(m_textureDesc);

#if HYP_DEBUG_MODE
        Name assetName = GetName();
        if (assetName.IsValid())
        {
            gpuImage->SetDebugName(assetName);
        }
#endif

        CheckResultOrReturn(CreateGpuImage(*this, *gpuImage, RS_SHADER_RESOURCE, uploadTextureData));

        // done with image data
        readScope.Reset();

        auto writeScope = GetWriteScope();

        if (m_gpuImage.IsValid())
        {
            // another thread set the gpu image
            gpuImage.Reset();
            return {};
        }

        m_gpuImage = std::move(gpuImage);

        return {};
    }

    readScope.Reset();

    auto writeScope = GetWriteScope();

    if (!m_gpuImage->IsCreated())
    {
        CheckResultOrReturn(m_gpuImage->Create());
    }

    return {};
}

bool Texture::IsCreated() const
{
    return m_gpuImage.IsValid()
        && m_gpuImage->IsCreated();
}

Result Texture::Rename(Name name)
{
    return AssetObject::Rename(name);
}

void Texture::SetImageData(ConstByteView imageData)
{
    FreeBlobData(m_imageData);
    AllocateBlobData(m_imageData, imageData.Data(), imageData.Size(), 1);

    MarkDirty();
}

void Texture::PageBlobData()
{
    if (m_imageData.raw == nullptr
        && m_imageData.key
        && m_imageData.size != 0)
    {
        Handle<AssetRegistry> registry = GetAssetRegistry();
        AssertDebug(registry.IsValid());

        if (!registry.IsValid())
        {
            return;
        }

        BlobStorage* blobStorage = registry->HasBlobStorage() ? &registry->GetBlobStorage() : nullptr;

        if (!blobStorage || !blobStorage->GetData(m_imageData.key, m_imageData.size, m_imageData.raw))
        {
#if HYP_EDITOR || HYP_ALLOW_INLINE_BLOBS
            // check if failed; if so, try to import from raw data blob in project directory
            FileByteReader stream { registry->GetRootPath() / AssetBuckets::Textures.GetName() / (String(*GetName()) + ".TEX.raw.blob") };
            if (!stream.Eof())
            {
                ByteBuffer buffer = stream.Read(stream.Max());
                AssertDebug(buffer.Size() == stream.Max());

                AllocateBlobData(m_imageData, buffer.Data(), buffer.Size(), 1);

#if HYP_EDITOR
                Result saveBlobDataResult = SaveBlobData(blobStorage);
                if (saveBlobDataResult.HasError())
                {
                    HYP_LOG(Assets, Error, "Failed to save local blob data: {}", saveBlobDataResult.GetError().GetMessage());
                }

                MarkDirty();
#endif

                return;
            }
#endif

            HYP_FAIL("Blob data missing! Data corruption detected.");
        }
        else
        {
            m_imageData.readOnly = true;
        }
    }
}

void Texture::UnpageBlobData()
{
    if (m_imageData.readOnly)
    {
        m_imageData.raw = nullptr;
    }
}

void Texture::GenerateMipmaps(TextureDesc& desc, ByteBuffer& imageData)
{
    const uint32 numMipLevels = desc.NumMips();
    const uint32 numArrayLayers = desc.NumArrayLayers();

    AssertDebug(imageData.Size() == desc.GetByteSize());

    if (numMipLevels <= 1)
    {
        return;
    }

    const bool canGenerateMips = (desc.format >= TextureFormat::R16F && desc.format <= TextureFormat::RGBA32F)
        || (desc.format >= TextureFormat::R8 && desc.format <= TextureFormat::RGBA8)
        || (desc.format >= TextureFormat::R8_SRGB && desc.format <= TextureFormat::RGBA8_SRGB);

    if (!canGenerateMips)
    {
        return;
    }

    // base mip size
    const uint32 baseMipSize = desc.GetMipByteSize(0, /* includeArrayLayers */ true);
    AssertDebug(imageData.Size() == baseMipSize);

    uint32 totalSize = baseMipSize;

    for (uint32 mip = 1; mip < numMipLevels; mip++)
    {
        totalSize += desc.GetMipByteSize(mip, /* includeArrayLayers */ true);
    }

    imageData.SetSize(totalSize);

    uint32 srcBlockStart = 0;
    uint32 dstWriteOffset = baseMipSize;

    ubyte* intermediateBuffer = nullptr;

    ubyte* scratchBuffer = nullptr; // used for converting f16 -> f32 and back

    HYP_DEFER({
        if (intermediateBuffer != nullptr)
        {
            Memory::Free(intermediateBuffer);
            intermediateBuffer = nullptr;
        }

        if (scratchBuffer != nullptr)
        {
            Memory::Free(scratchBuffer);
            scratchBuffer = nullptr;
        }
    });

    for (uint32 dstMipLevel = 1; dstMipLevel < numMipLevels; dstMipLevel++)
    {
        uint32 srcMipLevel = dstMipLevel - 1;

        const uint32 srcMipSize = desc.GetMipByteSize(srcMipLevel);
        const uint32 dstMipSize = desc.GetMipByteSize(dstMipLevel);

        const Vec3u srcExtent = desc.GetMipExtent(srcMipLevel);
        const Vec3u dstExtent = desc.GetMipExtent(dstMipLevel);

        if (!intermediateBuffer)
        {
            size_t intermediateBufferSize = dstMipSize;

            if (desc.format >= TextureFormat::R16F && desc.format <= TextureFormat::RGBA16F)
            {
                intermediateBufferSize *= 2; // increase buffer size to convert between 16 and 32 bit float
            }

            intermediateBuffer = (ubyte*)Memory::Allocate(intermediateBufferSize);
        }

        // mipOffsets stores the start of the block for given mip level but skips the first elem
        // so we can check if we have pregenerated mips by doing mipOffsets[0] != 0
        desc.mipOffsets[dstMipLevel - 1] = dstWriteOffset;

        uint32 currentBlockStart = dstWriteOffset;

        for (uint32 layer = 0; layer < numArrayLayers; layer++)
        {
            uint32 readOffset = srcBlockStart + (layer * srcMipSize);

            ConstByteView srcView = imageData.ToByteView().Slice(readOffset, readOffset + srcMipSize);

            int result = 0;
            const int numChannels = TextureUtils::NumComponents(desc.format);

            if (desc.format >= TextureFormat::R32F && desc.format <= TextureFormat::RGBA32F)
            {
                AssertDebug(srcView.Size() % sizeof(float32) == 0);

                result = stbir_resize_float(
                    reinterpret_cast<const float32*>(srcView.Data()),
                    srcExtent.x, srcExtent.y, 0,
                    reinterpret_cast<float32*>(intermediateBuffer),
                    dstExtent.x, dstExtent.y, 0,
                    numChannels);
            }
            else if (desc.format >= TextureFormat::R16F && desc.format <= TextureFormat::RGBA16F)
            {
                if (!scratchBuffer)
                {
                    // allocate enough memory to be used by all proceeding mips
                    scratchBuffer = (ubyte*)Memory::Allocate(srcMipSize * 2);
                }

                const Float16* float16Data = reinterpret_cast<const Float16*>(srcView.Data());

                // initialize f32 data from f16
                for (size_t byteIndex = 0; byteIndex < srcMipSize; byteIndex += sizeof(Float16))
                {
                    float32& f32Value = (*(reinterpret_cast<float32*>(scratchBuffer + (byteIndex * 2))) = *(float16Data + (byteIndex / sizeof(Float16))));

                    if (f32Value < -FLT16_MAX)
                    {
                        f32Value = -FLT16_MAX;
                    }
                    else if (f32Value > FLT16_MAX)
                    {
                        f32Value = FLT16_MAX;
                    }
                    else if (f32Value == static_cast<float32>(Float16::FromRaw(65504))) // nan
                    {
                        f32Value = 0.0f;
                    }
                }

                result = stbir_resize_float(
                    reinterpret_cast<const float32*>(scratchBuffer),
                    srcExtent.x, srcExtent.y, 0,
                    reinterpret_cast<float32*>(intermediateBuffer),
                    dstExtent.x, dstExtent.y, 0,
                    numChannels);

                // scratchData now used to store result converted to f16
                for (size_t byteIndex = 0; byteIndex < dstMipSize * 2; byteIndex += sizeof(float32))
                {
                    *reinterpret_cast<Float16*>(scratchBuffer + (byteIndex / 2)) = *(reinterpret_cast<const float32*>(intermediateBuffer + byteIndex));
                }

                Memory::Copy(intermediateBuffer, scratchBuffer, dstMipSize);
            }
            else if (desc.IsSrgb())
            {
                result = stbir_resize_uint8_srgb(
                    srcView.Data(), srcExtent.x, srcExtent.y, 0,
                    intermediateBuffer, dstExtent.x, dstExtent.y, 0,
                    numChannels, numChannels == 4 ? 3 : -1, 0);
            }
            else if (desc.format >= TextureFormat::R8 && desc.format <= TextureFormat::RGBA8)
            {
                result = stbir_resize_uint8(
                    srcView.Data(), srcExtent.x, srcExtent.y, 0,
                    intermediateBuffer, dstExtent.x, dstExtent.y, 0,
                    numChannels);
            }
            else
            {
                Assert(false, "Unsupported texture format for mipmap generation: {}", desc.format);
            }

            if (result == 0)
            {
                HYP_LOG(Texture, Error, "Mip generation failed at level {} layer {}", dstMipLevel, layer);
                return;
            }

            imageData.Write(dstMipSize, dstWriteOffset, intermediateBuffer);

            dstWriteOffset += dstMipSize;
        }

        srcBlockStart = currentBlockStart;
    }
}

void Texture::Readback(GpuBufferRef& outBuffer, bool allMips)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(IsCreated());

    if (!IsCreated())
    {
        return;
    }

    outBuffer = RI.MakeGpuBuffer(GpuBufferType::ReadbackBuffer, m_gpuImage->GetTextureDesc().GetByteSize(allMips));
    outBuffer->SetIsCpuAccessible(true);
#if HYP_DEBUG_MODE
    outBuffer->SetDebugName(NAME("Texture_ReadbackBuffer"));
#endif

    CheckResult(outBuffer->Create());

    UniquePtr<SingleTimeCommands> singleTimeCommands = RI.GetSingleTimeCommands();

    singleTimeCommands->Push([this, &outBuffer, allMips](CommandRecorder& cr)
        {
            const ResourceState previousResourceState = m_gpuImage->GetResourceState();

            cr << InsertBarrier(m_gpuImage, RS_COPY_SRC);
            cr << InsertBarrier(outBuffer, RS_COPY_DST);

            ImageSubResource sr;
            sr.baseArrayLayer = 0;
            sr.numLayers = UINT16_MAX;
            sr.baseMipLevel = 0;
            sr.numLevels = UINT8_MAX;

            if (!allMips)
            {
                sr.numLevels = 1;
            }

            cr << CopyImageToBuffer(m_gpuImage, outBuffer, sr);

            if (previousResourceState != RS_UNDEFINED && previousResourceState != RS_PRE_INITIALIZED)
            {
                cr << InsertBarrier(m_gpuImage, previousResourceState);
            }
            else
            {
                cr << InsertBarrier(m_gpuImage, RS_SHADER_RESOURCE);
            }
        });

    RendererResult result = singleTimeCommands->Execute();

    if (result.HasError())
    {
        HYP_LOG(Rendering, Error, "Failed to readback texture data! {}", result.GetError().GetMessage());

        EnqueueDeletion(std::move(outBuffer));

        return;
    }
}

void Texture::EnqueueReadback(Proc<void(GpuBuffer&)>&& callback, bool allMips)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(IsCreated());

    if (!IsCreated())
    {
        return;
    }

    const ResourceState previousResourceState = m_gpuImage->GetResourceState();

    GpuBufferRef readbackBuffer = RI.MakeGpuBuffer(GpuBufferType::ReadbackBuffer, m_gpuImage->GetTextureDesc().GetByteSize(allMips));
    readbackBuffer->SetIsCpuAccessible(true);
#if HYP_DEBUG_MODE
    readbackBuffer->SetDebugName(NAME("Texture_EnqueueReadbackBuffer"));
#endif

    CheckResult(readbackBuffer->Create());

    CommandRecorder& cr = RI.commandRecorderAllocator.GetCommandRecorder();
    HYP_DEFER({ cr.Done(); });

    cr << InsertBarrier(m_gpuImage, RS_COPY_SRC);
    cr << InsertBarrier(readbackBuffer, RS_COPY_DST);

    ImageSubResource sr;
    sr.baseArrayLayer = 0;
    sr.numLayers = UINT16_MAX;
    sr.baseMipLevel = 0;
    sr.numLevels = UINT8_MAX;

    if (!allMips)
    {
        sr.numLevels = 1;
    }

    cr << CopyImageToBuffer(m_gpuImage, readbackBuffer, sr);

    if (previousResourceState != RS_UNDEFINED && previousResourceState != RS_PRE_INITIALIZED)
    {
        cr << InsertBarrier(m_gpuImage, previousResourceState);
    }
    else
    {
        cr << InsertBarrier(m_gpuImage, RS_SHADER_RESOURCE);
    }

    struct ReadbackPayload
    {
        GpuImageRef image;
        GpuBufferRef readbackBuffer;
        Proc<void(GpuBuffer&)> callback;
    };

    ReadbackPayload* payload = HYP_POOL_NEW(g_renderPool, ReadbackPayload);
    payload->image = MakeStrongRef(m_gpuImage);
    payload->readbackBuffer = std::move(readbackBuffer);
    payload->callback = std::move(callback);

    class ReadbackTextureCmd : public CmdBase
    {
    public:
        ReadbackPayload* payload;

        ReadbackTextureCmd(ReadbackPayload* payload)
            : payload(payload)
        {
        }

        static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
        {
            ReadbackTextureCmd* _this = static_cast<ReadbackTextureCmd*>(cmd);

            Frame* currentFrame = RI.GetCurrentFrame();
            Assert(currentFrame != nullptr);

            currentFrame->OnFrameEnd
                .Bind([payload = _this->payload](...)
                {
                    payload->callback(*payload->readbackBuffer);
                    payload->readbackBuffer.Reset();

                    EnqueueDeletion(std::move(payload->image));

                    PoolDelete(*g_renderPool, payload);
                })
                .Detach();
        }
    };

    cr << ReadbackTextureCmd(payload);
}

Vec4f Texture::Sample(Vec3f uvw, uint32 faceIndex)
{
    auto readScope = GetReadScope();

    if (faceIndex >= NumArrayLayers())
    {
        HYP_LOG_ONCE(Texture, Error, "Face index out of bounds: {} >= {}", faceIndex, NumArrayLayers());

        HYP_BREAKPOINT;

        return Vec4f::Zero();
    }

    ConstByteView imageData = GetImageData();
    const TextureDesc& textureDesc = GetTextureDesc();

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

    const uint32 largestMipSize = textureDesc.HasStoredMips()
        ? textureDesc.mipOffsets[0]
        : uint32(textureDesc.GetByteSize());

    if (index + (bytesPerComponent * numComponents) > largestMipSize)
    {
        HYP_LOG_ONCE(Texture, Warning,
            "Sample() call would attempt to read out of bounds of data for Texture {} ({})!\n"
            "Texture format: {}, Texel index: {}, texture data buffer size: {}, coord: {}, dimensions: {}, num faces: {}, bytes per component: {}, num components: {}",
            GetName(), Id(),
            EnumToString(textureDesc.format),
            index, largestMipSize,
            coord, textureDesc.extent, NumArrayLayers(),
            bytesPerComponent, numComponents);

        return Vec4f::Zero();
    }

    if ((textureDesc.format >= TextureFormat::R16F && textureDesc.format <= TextureFormat::RGBA32F) || textureDesc.format == TextureFormat::R11G11B10F)
    {
        // FP format
        switch (numComponents)
        {
        case 1:
            return ConstPixelReference<float, 1>(imageData.Data() + index).GetRGBA();
        case 2:
            return ConstPixelReference<float, 2>(imageData.Data() + index).GetRGBA();
        case 3:
            return ConstPixelReference<float, 3>(imageData.Data() + index).GetRGBA();
        case 4:
            return ConstPixelReference<float, 4>(imageData.Data() + index).GetRGBA();
        default:
            break;
        }
    }
    else if (textureDesc.format >= TextureFormat::R16 && textureDesc.format <= TextureFormat::RGBA16)
    {
        // 16 bit integer format
        switch (numComponents)
        {
        case 1:
            return ConstPixelReference<uint16, 1>(imageData.Data() + index).GetRGBA();
        case 2:
            return ConstPixelReference<uint16, 2>(imageData.Data() + index).GetRGBA();
        case 3:
            return ConstPixelReference<uint16, 3>(imageData.Data() + index).GetRGBA();
        case 4:
            return ConstPixelReference<uint16, 4>(imageData.Data() + index).GetRGBA();
        default:
            break;
        }
    }
    else if (textureDesc.format >= TextureFormat::R32 && textureDesc.format <= TextureFormat::RGBA32)
    {
        // 32 bit integer format
        switch (numComponents)
        {
        case 1:
            return ConstPixelReference<uint32, 1>(imageData.Data() + index).GetRGBA();
        case 2:
            return ConstPixelReference<uint32, 2>(imageData.Data() + index).GetRGBA();
        case 3:
            return ConstPixelReference<uint32, 3>(imageData.Data() + index).GetRGBA();
        case 4:
            return ConstPixelReference<uint32, 4>(imageData.Data() + index).GetRGBA();
        default:
            break;
        }
    }
    else
    {
        if (TextureUtils::IsSRGB(textureDesc.format))
        {
            // convert from sRGB to linear
            switch (numComponents)
            {
            case 1:
                return ConstPixelReference<ubyte, 1, true>(imageData.Data() + index).GetRGBA();
            case 2:
                return ConstPixelReference<ubyte, 2, true>(imageData.Data() + index).GetRGBA();
            case 3:
                return ConstPixelReference<ubyte, 3, true>(imageData.Data() + index).GetRGBA();
            case 4:
                return ConstPixelReference<ubyte, 4, true>(imageData.Data() + index).GetRGBA();
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
                return ConstPixelReference<ubyte, 1>(imageData.Data() + index).GetRGBA();
            case 2:
                return ConstPixelReference<ubyte, 2>(imageData.Data() + index).GetRGBA();
            case 3:
                return ConstPixelReference<ubyte, 3>(imageData.Data() + index).GetRGBA();
            case 4:
                return ConstPixelReference<ubyte, 4>(imageData.Data() + index).GetRGBA();
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
    if (GetType() != TextureType::Texture2D)
    {
        HYP_LOG_ONCE(Texture, Warning, "Unsupported texture type to use with Sample2D(): {}", GetType());

        return Vec4f::Zero();
    }

    return Sample(Vec3f { uv.x, uv.y, 0.0f }, 0);
}

/// https://www.gamedev.net/forums/topic/687535-implementing-a-cube-map-lookup-function/5337472/
Vec4f Texture::SampleCube(Vec3f direction)
{
    if (GetType() != TextureType::Cubemap)
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
