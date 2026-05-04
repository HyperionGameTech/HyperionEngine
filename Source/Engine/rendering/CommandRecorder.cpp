/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/CommandRecorder.hpp>
#include <rendering/Frame.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/DescriptorSetCache.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/GraphicsPipeline.hpp>
#include <rendering/ComputePipeline.hpp>
#include <rendering/ShaderInstance.hpp>
#include <rendering/Shader.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/CBufferAllocator.hpp>
#include <rendering/RayTracingPipeline.hpp>
#include <rendering/AccelerationStructure.hpp>
#include <rendering/ScratchImageAllocator.hpp>
#include <rendering/GpuImageView.hpp>
#include <rendering/GpuBuffer.hpp>
#include <rendering/SamplerCache.hpp>
#include <rendering/Sampler.hpp>

#include <rendering/util/ShaderCompiler.hpp>

#include <Core/reflection/Enum.hpp>

#include <scene/View.hpp>

#include <util/MeshBuilder.hpp>

namespace Hyperion {

#pragma region CommandRecorder

template <>
void TCommandRecorder<RenderAllocator>::Prepare(Frame* frame)
{
    Assert(frame != nullptr);

    for (CmdHeader& cmdHeader : m_cmdHeaders)
    {
        CmdBase* cmdDataPtr = reinterpret_cast<CmdBase*>(m_buffer.Data() + cmdHeader.offset);
        AssertDebug(cmdHeader.offset < m_buffer.Size());

        if (cmdHeader.prepareFnPtr != nullptr)
        {
            cmdHeader.prepareFnPtr(cmdDataPtr, frame);
        }
    }
}

template <>
void TCommandRecorder<RenderAllocator>::Execute(CommandBuffer* commandBuffer)
{
    AssertDebug(commandBuffer != nullptr);

    const size_t max = m_cmdHeaders.Size();

    CmdHeader* headersBegin = m_cmdHeaders.Data();
    CmdHeader* headersEnd = headersBegin + max;

    CmdHeader* curr = headersBegin;

    ubyte* data = m_buffer.Data();

    while (curr != headersEnd)
    {
        CmdBase* cmdDataPtr = HYP_ALIGN_PTR_AS(data + curr->offset, CmdBase);

        InvokeCmdFnPtr invokeFnPtr = curr->invokeFnPtr;
        invokeFnPtr(cmdDataPtr, commandBuffer);

        ++curr;
    }

    m_cmdHeaders.Clear();
    m_offset = 0;

    m_writableState.Release();
}

#pragma endregion CommandRecorder

#pragma region BindVertexBuffer

void BindVertexBuffer::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    BindVertexBuffer* cmdCasted = static_cast<BindVertexBuffer*>(cmd);
    commandBuffer->BindVertexBuffer(cmdCasted->m_buffer);
}

#pragma endregion BindVertexBuffer

#pragma region BindIndexBuffer

void BindIndexBuffer::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    BindIndexBuffer* cmdCasted = static_cast<BindIndexBuffer*>(cmd);
    commandBuffer->BindIndexBuffer(cmdCasted->m_buffer);
}

#pragma endregion BindIndexBuffer

#pragma region DrawIndexed

void DrawIndexed::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    DrawIndexed* cmdCasted = static_cast<DrawIndexed*>(cmd);
    commandBuffer->DrawIndexed(cmdCasted->m_numIndices, cmdCasted->m_numInstances, cmdCasted->m_instanceIndex);
}

#pragma endregion DrawIndexed

#pragma region DrawIndexedIndirect

void DrawIndexedIndirect::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    DrawIndexedIndirect* cmdCasted = static_cast<DrawIndexedIndirect*>(cmd);

#if HYP_VULKAN
    AssertDebug(cmdCasted->m_bufferOffset + 20 <= cmdCasted->m_buffer->Size());
#endif

    commandBuffer->DrawIndexedIndirect(cmdCasted->m_buffer, cmdCasted->m_bufferOffset);
}

#pragma endregion DrawIndexedIndirect

#pragma region Blit

#ifndef HYP_VULKAN

/// Generic blit pass for D3D since it doesn't have something analogous to vkCmdBlitImage().
static void BlitImages(
    GpuImage* srcImage,
    GpuImage* dstImage,
    const Rect<uint32>& srcRect,
    const Rect<uint32>& dstRect,
    const ImageSubResource& srcSubResource,
    const ImageSubResource& dstSubResource,
    CommandBuffer* commandBuffer)
{
    if (!srcImage || !dstImage || !srcImage->IsCreated() || !dstImage->IsCreated())
    {
        HYP_LOG(RenderingBackend, Warning, "BlitImages: source or destination image is null or not created");

        return;
    }

    const TextureDesc& srcDesc = srcImage->GetTextureDesc();
    const TextureDesc& dstDesc = dstImage->GetTextureDesc();

    const uint8 srcMipCount = MathUtil::Min(srcSubResource.numLevels, uint8(srcDesc.NumMips() - srcSubResource.baseMipLevel));
    const uint16 srcLayerCount = MathUtil::Min(srcSubResource.numLayers, uint16(srcDesc.NumArrayLayers() - srcSubResource.baseArrayLayer));

    const uint8 dstMipCount = MathUtil::Min(dstSubResource.numLevels, uint8(dstDesc.NumMips() - dstSubResource.baseMipLevel));
    const uint16 dstLayerCount = MathUtil::Min(dstSubResource.numLayers, uint16(dstDesc.NumArrayLayers() - dstSubResource.baseArrayLayer));

    const uint8 mipIterCount = MathUtil::Min(srcMipCount, dstMipCount);
    const uint16 layerIterCount = MathUtil::Min(srcLayerCount, dstLayerCount);

    if (mipIterCount == 0 || layerIterCount == 0)
    {
        return;
    }

    const uint32 dstW = dstRect.x1 - dstRect.x0;
    const uint32 dstH = dstRect.y1 - dstRect.y0;

    if (dstW == 0 || dstH == 0)
    {
        return;
    }

    /* Create a temporary image with IU_STORAGE so we can write to it via compute.
       The temp image has the destination format and is sized to the destination rect.
       After the compute blit, we issue a CopyFrom (1:1, no scaling) to the real destination. */
    GpuImageRef tempImage = RI.MakeImage(TextureDesc {
        TextureType::Texture2D,
        dstDesc.format,
        Vec3u { dstW, dstH, 1 },
        TFM_LINEAR,
        TFM_LINEAR,
        TWM_CLAMP_TO_EDGE,
        1, // numLayers
        IU_SAMPLED | IU_STORAGE
    });
    tempImage->Create();

    /* Set the shader */
    RenderInterface::State& state = RI.state;
    state.attributes.SetShaderName(NAME("BlitCompute"));
    state.attributes.SetShaderProperties(ShaderPropertySet {});

    /* Get a linear sampler */
    Sampler* linearSampler = RI.samplerCache->GetOrCreate(
        SamplerDesc { TFM_LINEAR, TFM_LINEAR, TWM_CLAMP_TO_EDGE });

    for (uint16 layerIndex = 0; layerIndex < layerIterCount; layerIndex++)
    {
        for (uint8 mipOffset = 0; mipOffset < mipIterCount; mipOffset++)
        {
            const uint8 srcMip = uint8(srcSubResource.baseMipLevel + mipOffset);
            const uint8 dstMip = uint8(dstSubResource.baseMipLevel + mipOffset);
            const uint16 srcLayer = uint16(srcSubResource.baseArrayLayer + layerIndex);
            const uint16 dstLayer = uint16(dstSubResource.baseArrayLayer + layerIndex);

            /* Create SRV view for the source subresource */
            GpuImageViewRef srcView = RI.MakeImageView(
                MakeStrongRef(srcImage), srcMip, 1, srcLayer, 1);
            srcView->Create();

            /* Create UAV view for the temp image (single mip 0, layer 0) */
            GpuImageViewRef tempView = RI.MakeImageView(
                tempImage, 0, 1, 0, 1);
            tempView->Create();

            /* Transition source to shader-readable state */
            srcImage->InsertBarrier(
                commandBuffer,
                ImageSubResource { .baseMipLevel = srcMip, .numLevels = 1, .baseArrayLayer = srcLayer, .numLayers = 1 },
                RS_SHADER_RESOURCE,
                ShaderModuleType::None);

            /* Transition temp to UAV state */
            tempImage->InsertBarrier(
                commandBuffer,
                RS_UNORDERED_ACCESS,
                ShaderModuleType::None);

            struct BlitUniformData
            {
                uint32 srcRectMin[2];
                uint32 srcRectMax[2];
                uint32 dstRectMin[2];
                uint32 dstRectMax[2];
                uint32 srcDimensions[2];
                uint32 srcMipLevel;
                uint32 _padding;
            } uniforms;

            uniforms.srcRectMin[0] = srcRect.x0;
            uniforms.srcRectMin[1] = srcRect.y0;
            uniforms.srcRectMax[0] = srcRect.x1;
            uniforms.srcRectMax[1] = srcRect.y1;
            uniforms.dstRectMin[0] = 0;
            uniforms.dstRectMin[1] = 0;
            uniforms.dstRectMax[0] = dstW;
            uniforms.dstRectMax[1] = dstH;

            const Vec3u srcExtent = srcDesc.GetMipExtent(srcMip);
            uniforms.srcDimensions[0] = srcExtent.x;
            uniforms.srcDimensions[1] = srcExtent.y;
            uniforms.srcMipLevel = srcMip;

            /* Create and fill uniform buffer */
            GpuBuffer* cbuffer = nullptr;
            size_t cbufferOffset = 0;
            size_t cbufferSize = 0;

            RI.cbufferAllocator->Write(&uniforms);
            RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

            /* Bind SRV input */
            ShaderUniform& inputUniform = state.shaderUniforms[0];
            inputUniform = ShaderUniform("InputTexture"_sh, srcView.Get());
            state.dirtyUniforms |= 1u << 0;

            /* Bind UAV output (temp image) */
            ShaderUniform& outputUniform = state.shaderUniforms[1];
            outputUniform = ShaderUniform("OutputTexture"_sh, tempView.Get());
            state.dirtyUniforms |= 1u << 1;

            /* Bind uniform buffer */
            ShaderUniform& ubUniform = state.shaderUniforms[2];
            ubUniform = ShaderUniform("BlitConstants"_sh, cbuffer);
            state.shaderUniformBufferOffsets[2] = uint32(cbufferOffset);
            state.shaderUniformBufferOffsetStrides[2] = uint32(cbufferSize);
            state.dirtyUniforms |= 1u << 2;
            state.dirtyBufferOffsets |= 1u << 2;

            /* Bind sampler */
            ShaderUniform& samplerUniform = state.shaderUniforms[3];
            samplerUniform = ShaderUniform("SamplerLinear"_sh, linearSampler);
            state.dirtyUniforms |= 1u << 3;

            /* Commit compute pipeline and dispatch */
            RI.CommitPipelineState(PSO_Compute, commandBuffer);

            ComputePipeline* pipeline = state.boundComputePipeline;
            AssertDebug(pipeline != nullptr);

            pipeline->Dispatch(commandBuffer, {
                (dstW + 7) / 8,
                (dstH + 7) / 8,
                1
            });

            /* Transition temp to copy-source */
            tempImage->InsertBarrier(
                commandBuffer,
                RS_COPY_SRC,
                ShaderModuleType::None);

            /* Transition destination subresource to copy-dest */
            dstImage->InsertBarrier(
                commandBuffer,
                ImageSubResource { .baseMipLevel = dstMip, .numLevels = 1, .baseArrayLayer = dstLayer, .numLayers = 1 },
                RS_COPY_DST,
                ShaderModuleType::None);

            /* Copy from temp to the actual destination (1:1, same format & size) */
            dstImage->CopyFrom(
                commandBuffer,
                tempImage.Get(),
                Vec3u::Zero(),
                Vec3u { dstRect.x0, dstRect.y0, 0 },
                Vec3u { dstW, dstH, 1 },
                ImageSubResource { .baseMipLevel = 0, .numLevels = 1, .baseArrayLayer = 0, .numLayers = 1 },
                ImageSubResource { .baseMipLevel = dstMip, .numLevels = 1, .baseArrayLayer = dstLayer, .numLayers = 1 });

            /* Transition destination back to shader-readable */
            dstImage->InsertBarrier(
                commandBuffer,
                ImageSubResource { .baseMipLevel = dstMip, .numLevels = 1, .baseArrayLayer = dstLayer, .numLayers = 1 },
                RS_SHADER_RESOURCE,
                ShaderModuleType::None);

            /* Transfer transient resources to the deletion queue */
            EnqueueDeletion(std::move(srcView));
            EnqueueDeletion(std::move(tempView));
        }
    }

    /* Transition any remaining source mips to shader-readable */
    srcImage->InsertBarrier(
        commandBuffer,
        RS_SHADER_RESOURCE,
        ShaderModuleType::None);

    /* Clean up the temp image */
    EnqueueDeletion(std::move(tempImage));
}
#endif

void Blit::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    Blit* cmdCasted = static_cast<Blit*>(cmd);

    GpuImage* srcImage = cmdCasted->m_srcImage;
    GpuImage* dstImage = cmdCasted->m_dstImage;

    /* Resolve defaults */
    ImageSubResource srcSubResource = cmdCasted->m_srcSubResource;
    ImageSubResource dstSubResource = cmdCasted->m_dstSubResource;

    Rect<uint32> srcRect = cmdCasted->m_srcRect;
    Rect<uint32> dstRect = cmdCasted->m_dstRect;

    if (!cmdCasted->m_hasRect)
    {
        /* Default rects = full extent */
        const Vec3u srcExtent = srcImage->GetExtent();
        const Vec3u dstExtent = dstImage->GetExtent();

        srcRect = Rect<uint32> { 0, 0, srcExtent.x, srcExtent.y };
        dstRect = Rect<uint32> { 0, 0, dstExtent.x, dstExtent.y };
    }

    if (!cmdCasted->m_hasSubResource)
    {
        /* Default subresource = all mips and layers */
        srcSubResource.baseMipLevel = 0;
        srcSubResource.numLevels = uint8(srcImage->NumMips());
        srcSubResource.baseArrayLayer = 0;
        srcSubResource.numLayers = srcImage->NumArrayLayers();

        dstSubResource.baseMipLevel = 0;
        dstSubResource.numLevels = uint8(dstImage->NumMips());
        dstSubResource.baseArrayLayer = 0;
        dstSubResource.numLayers = dstImage->NumArrayLayers();
    }

#ifdef HYP_VULKAN
    dstImage->Blit(commandBuffer, srcImage, srcRect, dstRect, srcSubResource, dstSubResource);
#else
    //BlitImages(srcImage, dstImage, srcRect, dstRect, srcSubResource, dstSubResource, commandBuffer);
#endif
}

#pragma endregion Blit

#pragma region BlitRect

void BlitRect::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    BlitRect* cmdCasted = static_cast<BlitRect*>(cmd);

    GpuImage* srcImage = cmdCasted->m_srcImage;
    GpuImage* dstImage = cmdCasted->m_dstImage;

    // Validate inputs
    if (!srcImage || !dstImage)
    {
        HYP_LOG(RenderingBackend, Warning, "BlitRect: Source or destination image is null");
        return;
    }

    if (!srcImage->IsCreated() || !dstImage->IsCreated())
    {
        HYP_LOG(RenderingBackend, Warning, "BlitRect: Source or destination image is not created");
        return;
    }

    // Check for format compatibility
    if (srcImage->GetTextureFormat() != dstImage->GetTextureFormat())
    {
        HYP_LOG(RenderingBackend, Warning, "BlitRect: Source and destination formats do not match ({} != {})",
            EnumToString(srcImage->GetTextureFormat()),
            EnumToString(dstImage->GetTextureFormat()));
        return;
    }

    Rect<uint32> srcRect = cmdCasted->m_srcRect;
    Rect<uint32> dstRect = cmdCasted->m_dstRect;

    ImageSubResource srcSubResource;
    srcSubResource.baseMipLevel = 0;
    srcSubResource.numLevels = 1; // Only copy mip 0 for BlitRect
    srcSubResource.baseArrayLayer = 0;
    srcSubResource.numLayers = 1; // Only copy layer 0 for BlitRect

    ImageSubResource dstSubResource;
    dstSubResource.baseMipLevel = 0;
    dstSubResource.numLevels = 1; // Only copy mip 0 for BlitRect
    dstSubResource.baseArrayLayer = 0;
    dstSubResource.numLayers = 1; // Only copy layer 0 for BlitRect

#ifdef HYP_VULKAN
    dstImage->Blit(commandBuffer, srcImage, srcRect, dstRect, srcSubResource, dstSubResource);
#endif
}

#pragma endregion BlitRect

#pragma region CopyImage

void CopyImage::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    CopyImage* cmdCasted = static_cast<CopyImage*>(cmd);

    cmdCasted->dstImage->CopyFrom(
        commandBuffer,
        cmdCasted->srcImage,
        cmdCasted->srcOffset,
        cmdCasted->dstOffset,
        cmdCasted->extent,
        cmdCasted->srcSubResource,
        cmdCasted->dstSubResource);
}

#pragma endregion CopyImage

#pragma region CopyImageToBuffer

void CopyImageToBuffer::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    CopyImageToBuffer* cmdCasted = static_cast<CopyImageToBuffer*>(cmd);

    cmdCasted->m_image->CopyToBuffer(commandBuffer, cmdCasted->m_buffer, cmdCasted->m_subResource);
}

#pragma endregion CopyImageToBuffer

#pragma region CopyBufferToImage

void CopyBufferToImage::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    CopyBufferToImage* cmdCasted = static_cast<CopyBufferToImage*>(cmd);

    cmdCasted->m_dstImage->CopyFromBuffer(
        commandBuffer,
        cmdCasted->m_srcBuffer,
        cmdCasted->m_srcBufferOffset,
        cmdCasted->m_dstMipIndex,
        cmdCasted->m_dstArrayLayer);
}

#pragma endregion CopyBufferToImage

#pragma region GenerateMipmaps

GenerateMipmaps::GenerateMipmaps(GpuImage* image)
    : m_image(image)
{
    AssertDebug(image && image->IsCreated());
}

void GenerateMipmaps::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    GenerateMipmaps* cmdCasted = static_cast<GenerateMipmaps*>(cmd);

    GpuImage* image = cmdCasted->m_image;

    if (!image || !image->IsCreated())
    {
        HYP_LOG(RenderingBackend, Warning, "GenerateMipmaps::InvokeStatic: Image is null or not created");

        return;
    }

    const TextureDesc& desc = image->GetTextureDesc();
    const uint8 numMips = uint8(desc.NumMips());
    const uint16 numLayers = desc.NumArrayLayers();

    if (!RI.IsSupportedFormat(desc.format, ImageSupport::UnorderedAccess))
    {
        HYP_LOG(RenderingBackend, Warning, "Image format {} does not support UnorderedAccess, cannot generate mipmaps as it requires using a compute shader!",
            EnumToString(desc.format));

        return;
    }

    if (numMips < 2)
    {
        return;
    }

    /* Set the shader */
    RenderInterface::State& state = RI.state;
    state.attributes.SetShaderName(NAME("GenerateMipmap"));
    state.attributes.SetShaderProperties(ShaderPropertySet {});

    /* Get a linear sampler */
    Sampler* linearSampler = RI.samplerCache->GetOrCreate(
        SamplerDesc { TFM_LINEAR, TFM_LINEAR, TWM_REPEAT });

    if (!linearSampler)
    {
        HYP_LOG(RenderingBackend, Error, "GenerateMipmaps: Failed to get linear sampler");
        return;
    }

    for (uint16 layer = 0; layer < numLayers; layer++)
    {
        /* Acquire a temporary 2D image with IU_STORAGE | IU_SAMPLED so we
           can generate mips via compute dispatch without requiring the
           source image to have IU_STORAGE. */
        GpuImageRef tempImage = RI.scratchImageAllocator->AcquireScratchImage(desc.format, desc.extent);

        if (!tempImage)
        {
            HYP_LOG(RenderingBackend, Error, "GenerateMipmaps: Failed to acquire scratch image");
            continue;
        }

        image->InsertBarrier(
            commandBuffer,
            ImageSubResource { .baseMipLevel = 0, .numLevels = 1, .baseArrayLayer = layer, .numLayers = 1 },
            RS_COPY_SRC,
            ShaderModuleType::None);

        tempImage->InsertBarrier(
            commandBuffer,
            RS_COPY_DST,
            ShaderModuleType::None);

        tempImage->CopyFrom(
            commandBuffer,
            image,
            Vec3u::Zero(),
            Vec3u::Zero(),
            desc.extent,
            ImageSubResource { .baseMipLevel = 0, .numLevels = 1, .baseArrayLayer = layer, .numLayers = 1 },
            ImageSubResource { .baseMipLevel = 0, .numLevels = 1, .baseArrayLayer = 0, .numLayers = 1 });

        /* Allocate per-mip views and cbuffers for the temp image */
        Array<GpuImageViewRef> inputViews;
        Array<GpuImageViewRef> outputViews;
        Array<GpuBuffer*> cbuffers;
        Array<size_t> cbufferOffsets;
        Array<size_t> cbufferSizes;

        inputViews.Reserve(numMips - 1);
        outputViews.Reserve(numMips - 1);
        cbuffers.Reserve(numMips - 1);
        cbufferOffsets.Reserve(numMips - 1);
        cbufferSizes.Reserve(numMips - 1);

        struct MipGenUniforms
        {
            Vec2u srcDimensions;
            Vec2u dstDimensions;
            uint32 srcMipLevel;
        };

        // Allocate each mip's uniforms separately - each gets its own cbuffer
        for (uint8 mip = 1; mip < numMips; mip++)
        {
            const uint8 srcMip = mip - 1;

            GpuImageViewRef inputView = RI.MakeImageView(
                tempImage, srcMip, 1, 0, 1);
            RendererResult inputViewResult = inputView->Create();
            if (!inputViewResult)
            {
                HYP_LOG(RenderingBackend, Error, "GenerateMipmaps: Failed to create input view for mip {}: {}",
                    srcMip, inputViewResult.HasError() ? inputViewResult.GetError().GetMessage() : "Unknown error");
                // Continue anyway, will check validity before use
            }
            inputViews.PushBack(std::move(inputView));

            GpuImageViewRef outputView = RI.MakeImageView(
                tempImage, mip, 1, 0, 1);
            RendererResult outputViewResult = outputView->Create();
            if (!outputViewResult)
            {
                HYP_LOG(RenderingBackend, Error, "GenerateMipmaps: Failed to create output view for mip {}: {}",
                    mip, outputViewResult.HasError() ? outputViewResult.GetError().GetMessage() : "Unknown error");
                // Continue anyway, will check validity before use
            }
            outputViews.PushBack(std::move(outputView));

            const Vec3u srcExtent = desc.GetMipExtent(srcMip);
            const Vec3u dstExtent = desc.GetMipExtent(mip);

            MipGenUniforms uniforms;
            uniforms.srcDimensions = { srcExtent.x, srcExtent.y };
            uniforms.dstDimensions = { dstExtent.x, dstExtent.y };
            uniforms.srcMipLevel = srcMip;

            // Write uniform data to cbuffer allocator and get this mip's cbuffer
            RI.cbufferAllocator->Write(&uniforms);

            GpuBuffer* mipCBuffer = nullptr;
            size_t mipCBufferOffset = 0;
            size_t mipCBufferSize = 0;
            RI.cbufferAllocator->Commit(mipCBuffer, mipCBufferOffset, mipCBufferSize);

            cbuffers.PushBack(mipCBuffer);
            cbufferOffsets.PushBack(mipCBufferOffset);
            cbufferSizes.PushBack(mipCBufferSize);
        }

        /* Dispatch compute mip generation on the temp image */
        for (uint8 mip = 1; mip < numMips; mip++)
        {
            const uint8 srcMip = mip - 1;
            const Vec3u dstExtent = desc.GetMipExtent(mip);

            tempImage->InsertBarrier(
                commandBuffer,
                ImageSubResource { .baseMipLevel = srcMip, .numLevels = 1, .baseArrayLayer = 0, .numLayers = 1 },
                RS_SHADER_RESOURCE,
                ShaderModuleType::None);

            tempImage->InsertBarrier(
                commandBuffer,
                ImageSubResource { .baseMipLevel = mip, .numLevels = 1, .baseArrayLayer = 0, .numLayers = 1 },
                RS_UNORDERED_ACCESS,
                ShaderModuleType::None);

            // Validate that the image view and cbuffer are valid before using
            if (!inputViews[srcMip].IsValid() || !outputViews[srcMip].IsValid() || cbuffers[srcMip] == nullptr)
            {
                HYP_LOG(RenderingBackend, Error, "GenerateMipmaps: Invalid view or cbuffer at mip {}", mip);
                continue;
            }

            ShaderUniform& inputUniform = state.shaderUniforms[0];
            inputUniform = ShaderUniform("InputTexture"_sh, inputViews[srcMip].Get());
            state.dirtyUniforms |= 1u << 0;

            ShaderUniform& outputUniform = state.shaderUniforms[1];
            outputUniform = ShaderUniform("OutputTexture"_sh, outputViews[srcMip].Get());
            state.dirtyUniforms |= 1u << 1;

            // Use this mip's own cbuffer with dynamic offset
            ShaderUniform& ubUniform = state.shaderUniforms[2];
            ubUniform = ShaderUniform("Constants"_sh, cbuffers[srcMip]);
            state.shaderUniformBufferOffsets[2] = uint32(cbufferOffsets[srcMip]);
            state.shaderUniformBufferOffsetStrides[2] = uint32(cbufferSizes[srcMip]);
            state.dirtyUniforms |= 1u << 2;
            state.dirtyBufferOffsets |= 1u << 2;

            ShaderUniform& samplerUniform = state.shaderUniforms[3];
            samplerUniform = ShaderUniform("SamplerLinear"_sh, linearSampler);
            state.dirtyUniforms |= 1u << 3;

            RI.CommitPipelineState(PSO_Compute, commandBuffer);

            ComputePipeline* pipeline = state.boundComputePipeline;
            if (!pipeline)
            {
                HYP_LOG(RenderingBackend, Error, "GenerateMipmaps: Failed to get compute pipeline for mip {}. "
                    "Shader 'GenerateMipmap' may not be compiled or available.", mip);
                continue;
            }

            pipeline->Dispatch(commandBuffer, {
                (dstExtent.x + 7) / 8,
                (dstExtent.y + 7) / 8,
                1
            });

#ifdef HYP_DX12
            tempImage->InsertUAVBarrier(commandBuffer);
#endif
        }

#ifdef HYP_DX12
        tempImage->InsertUAVBarrier(commandBuffer);
#endif

        /* Copy each generated mip from the temp image back to the source */
        for (uint8 mip = 1; mip < numMips; mip++)
        {
            const Vec3u mipExtent = desc.GetMipExtent(mip);

            tempImage->InsertBarrier(
                commandBuffer,
                ImageSubResource { .baseMipLevel = mip, .numLevels = 1, .baseArrayLayer = 0, .numLayers = 1 },
                RS_COPY_SRC,
                ShaderModuleType::None);

            image->InsertBarrier(
                commandBuffer,
                ImageSubResource { .baseMipLevel = mip, .numLevels = 1, .baseArrayLayer = layer, .numLayers = 1 },
                RS_COPY_DST,
                ShaderModuleType::None);

            image->CopyFrom(
                commandBuffer,
                tempImage.Get(),
                Vec3u::Zero(),
                Vec3u::Zero(),
                mipExtent,
                ImageSubResource { .baseMipLevel = mip, .numLevels = 1, .baseArrayLayer = 0, .numLayers = 1 },
                ImageSubResource { .baseMipLevel = mip, .numLevels = 1, .baseArrayLayer = layer, .numLayers = 1 });

            image->InsertBarrier(
                commandBuffer,
                ImageSubResource { .baseMipLevel = mip, .numLevels = 1, .baseArrayLayer = layer, .numLayers = 1 },
                RS_SHADER_RESOURCE,
                ShaderModuleType::None);
        }

        EnqueueDeletion(std::move(inputViews));
        EnqueueDeletion(std::move(outputViews));
    }

    image->InsertBarrier(
        commandBuffer,
        RS_SHADER_RESOURCE,
        ShaderModuleType::None);
}

#pragma endregion GenerateMipmaps

#pragma region BindDescriptorSet

BindDescriptorSet::BindDescriptorSet(DescriptorSet* descriptorSet, GraphicsPipeline* pipeline, const DescriptorSetOffsetMap& offsets)
    : m_descriptorSet(descriptorSet),
      m_graphicsPipeline(pipeline),
      m_offsets(offsets),
      m_pipelineType(0) // 0 = Graphics
{
    AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
    AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");

    AssertDebug(pipeline && pipeline->GetShader());

    m_bindIndex = pipeline->GetShader()->GetShader()->GetDescriptorTableDeclaration()->GetDescriptorSetIndex(descriptorSet->GetLayout().GetName());
    AssertDebug(m_bindIndex != ~0u, "Invalid bind index for descriptor set {}", descriptorSet->GetLayout().GetName());
}

BindDescriptorSet::BindDescriptorSet(DescriptorSet* descriptorSet, GraphicsPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex)
    : m_descriptorSet(descriptorSet),
      m_graphicsPipeline(pipeline),
      m_offsets(offsets),
      m_bindIndex(bindIndex),
      m_pipelineType(0) // 0 = Graphics
{
    AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
    AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");
    AssertDebug(m_bindIndex != ~0u, "Invalid bind index");
}

BindDescriptorSet::BindDescriptorSet(DescriptorSet* descriptorSet, ComputePipeline* pipeline, const DescriptorSetOffsetMap& offsets)
    : m_descriptorSet(descriptorSet),
      m_computePipeline(pipeline),
      m_offsets(offsets),
      m_pipelineType(1) // 1 = Compute
{
    AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
    AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");

    AssertDebug(pipeline && pipeline->GetShader());

    m_bindIndex = pipeline->GetShader()->GetShader()->GetDescriptorTableDeclaration()->GetDescriptorSetIndex(descriptorSet->GetLayout().GetName());
    AssertDebug(m_bindIndex != ~0u, "Invalid bind index for descriptor set {}", descriptorSet->GetLayout().GetName());
}

BindDescriptorSet::BindDescriptorSet(DescriptorSet* descriptorSet, ComputePipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex)
    : m_descriptorSet(descriptorSet),
      m_computePipeline(pipeline),
      m_offsets(offsets),
      m_bindIndex(bindIndex),
      m_pipelineType(1) // 1 = Compute
{
    AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
    AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");
    AssertDebug(m_bindIndex != ~0u, "Invalid bind index");
}

BindDescriptorSet::BindDescriptorSet(DescriptorSet* descriptorSet, RayTracingPipeline* pipeline, const DescriptorSetOffsetMap& offsets)
    : m_descriptorSet(descriptorSet),
      m_rayTracingPipeline(pipeline),
      m_offsets(offsets),
      m_pipelineType(2) // 2 = RayTracing
{
    AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
    AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");

    AssertDebug(pipeline && pipeline->GetShader());

    m_bindIndex = pipeline->GetShader()->GetShader()->GetDescriptorTableDeclaration()->GetDescriptorSetIndex(descriptorSet->GetLayout().GetName());
    AssertDebug(m_bindIndex != ~0u, "Invalid bind index for descriptor set {}", descriptorSet->GetLayout().GetName());
}

BindDescriptorSet::BindDescriptorSet(DescriptorSet* descriptorSet, RayTracingPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex)
    : m_descriptorSet(descriptorSet),
      m_rayTracingPipeline(pipeline),
      m_offsets(offsets),
      m_bindIndex(bindIndex),
      m_pipelineType(2) // 2 = RayTracing
{
    AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
    AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");
    AssertDebug(m_bindIndex != ~0u, "Invalid bind index");
}

void BindDescriptorSet::PrepareStatic(CmdBase* cmd, Frame* frame)
{
    BindDescriptorSet* cmdCasted = static_cast<BindDescriptorSet*>(cmd);

    Assert(cmdCasted->m_descriptorSet->IsCreated());
}

void BindDescriptorSet::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    BindDescriptorSet* cmdCasted = static_cast<BindDescriptorSet*>(cmd);

    switch (cmdCasted->m_pipelineType)
    {
    case 0: // Graphics
        cmdCasted->m_descriptorSet->Bind(commandBuffer, cmdCasted->m_graphicsPipeline, cmdCasted->m_offsets, cmdCasted->m_bindIndex);
        break;
    case 1: // Compute
        cmdCasted->m_descriptorSet->Bind(commandBuffer, cmdCasted->m_computePipeline, cmdCasted->m_offsets, cmdCasted->m_bindIndex);
        break;
    case 2: // RayTracing
        cmdCasted->m_descriptorSet->Bind(commandBuffer, cmdCasted->m_rayTracingPipeline, cmdCasted->m_offsets, cmdCasted->m_bindIndex);
        break;
    default:
        HYP_UNREACHABLE();
    }

    static_assert(std::is_trivially_destructible_v<BindDescriptorSet>);
    // cmdCasted->~BindDescriptorSet();
}

#pragma endregion BindDescriptorSet

#pragma region InsertBarrier

#if defined(HYP_VULKAN) && defined(HYP_DEBUG_MODE)
void InsertBarrier::CheckNotInRenderPass(CommandBuffer* commandBuffer) const
{
    Assert(!commandBuffer->IsInRenderPass());
}
#endif

void InsertBarrier::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    InsertBarrier* cmdCasted = static_cast<InsertBarrier*>(cmd);

#if defined(HYP_VULKAN) && defined(HYP_DEBUG_MODE)
    cmdCasted->CheckNotInRenderPass(commandBuffer);
#endif

    if (cmdCasted->m_buffer)
    {
        cmdCasted->m_buffer->InsertBarrier(commandBuffer, cmdCasted->m_state, cmdCasted->m_shaderModuleType);
    }
    else if (cmdCasted->m_image)
    {
        if (cmdCasted->m_hasSubResource)
        {
            cmdCasted->m_image->InsertBarrier(
                commandBuffer,
                cmdCasted->m_subResource,
                cmdCasted->m_state,
                cmdCasted->m_shaderModuleType,
                cmdCasted->m_onlyDepth,
                cmdCasted->m_onlyStencil);
        }
        else
        {
            cmdCasted->m_image->InsertBarrier(
                commandBuffer,
                cmdCasted->m_state,
                cmdCasted->m_shaderModuleType,
                cmdCasted->m_onlyDepth,
                cmdCasted->m_onlyStencil);
        }
    }
}

#pragma endregion InsertBarrier

#pragma region SetCurrentFramebuffer

thread_local Framebuffer* s_currentFramebuffer;

SetCurrentFramebuffer::SetCurrentFramebuffer(Framebuffer* framebuffer)
    : m_framebuffer(framebuffer)
{
    if (m_framebuffer != nullptr)
    {
        m_framebuffer->SetIsDeferredRecording(true);
    }

    if (s_currentFramebuffer != nullptr && s_currentFramebuffer != m_framebuffer)
    {
        s_currentFramebuffer->SetIsDeferredRecording(false);
    }

    s_currentFramebuffer = m_framebuffer;
}

void SetCurrentFramebuffer::PrepareStatic(CmdBase* cmd, Frame* frame)
{
}

void SetCurrentFramebuffer::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    SetCurrentFramebuffer* cmdCasted = static_cast<SetCurrentFramebuffer*>(cmd);

    RenderInterface::State& state = RI.state;

    if (cmdCasted->m_framebuffer != state.boundFramebuffer)
    {
        if (state.boundFramebuffer != nullptr)
        {
            // end current render pass before switching to a new framebuffer
            state.boundFramebuffer->EndCapture(commandBuffer);
            state.boundFramebuffer = nullptr;
        }

        // @NOTE: Vulkan validation layers complain that there is no
        // graphics pipeline bound when drawing when we start a new render pass and
        // expect the bound graphics pipeline to stay active.
        // From what I'm seeing in the spec this should be fine, so requires more
        // investigation to figure out if it is a quirk of the validation layers or
        // an actual issue.
        // For now, just rebind the graphics pipeline when we change render passes. (setting it to null will do this)
        state.boundGraphicsPipeline = nullptr;
    }
    else if (cmdCasted->m_framebuffer == nullptr && state.boundFramebuffer != nullptr)
    {
        // end render pass if we are setting to nullptr and we are currently in a pass
        state.boundFramebuffer->EndCapture(commandBuffer);
        state.boundFramebuffer = nullptr;
    }

    state.framebuffer = cmdCasted->m_framebuffer;

    // Clear uniforms for new pass
    state.validUniforms = 0;
    state.dirtyUniforms = 0;
    state.dirtyBufferOffsets = 0;

    static_assert(std::is_trivially_destructible_v<SetCurrentFramebuffer>);
    // cmdCasted->~SetCurrentFramebuffer();
}

#pragma endregion SetCurrentFramebuffer

#pragma region ClearFramebuffer

void ClearFramebuffer::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    ClearFramebuffer* cmdCasted = static_cast<ClearFramebuffer*>(cmd);

    AssertDebug(cmdCasted->framebuffer != nullptr);

    RenderInterface::State& state = RI.state;

    state.framebuffer = cmdCasted->framebuffer;

    if (state.boundFramebuffer != cmdCasted->framebuffer)
    {
        if (state.boundFramebuffer != nullptr)
        {
            // end render pass if we are in one and not same as the one we want to bind.
            state.boundFramebuffer->EndCapture(commandBuffer);
            state.boundFramebuffer = nullptr;
        }

        // begin pass
        state.boundFramebuffer = cmdCasted->framebuffer;

        cmdCasted->framebuffer->BeginCapture(commandBuffer);
    }

    if (int(cmdCasted->rect.x1) - int(cmdCasted->rect.x0) == 0
        && int(cmdCasted->rect.y1) - int(cmdCasted->rect.y0) == 0)
    {
        cmdCasted->framebuffer->Clear(commandBuffer, cmdCasted->attachmentsMask);
    }
    else
    {
        cmdCasted->framebuffer->Clear(commandBuffer, cmdCasted->rect, cmdCasted->attachmentsMask);
    }

    static_assert(std::is_trivially_destructible_v<ClearFramebuffer>);
    // cmdCasted->~ClearFramebuffer();
}

#pragma endregion ClearFramebuffer

#pragma region BindGraphicsPipeline

void BindGraphicsPipeline::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    RenderInterface::State& state = RI.state;

    BindGraphicsPipeline* cmdCasted = static_cast<BindGraphicsPipeline*>(cmd);

    cmdCasted->m_pipeline->lastFrame = GetFrameCounter();

    if (cmdCasted->m_viewport.position != Vec2i(0, 0) || cmdCasted->m_viewport.extent != Vec2u(0, 0))
    {
        cmdCasted->m_pipeline->Bind(commandBuffer, cmdCasted->m_viewport.position, cmdCasted->m_viewport.extent);
    }
    else
    {
        cmdCasted->m_pipeline->Bind(commandBuffer);
    }

    state.boundGraphicsPipeline = cmdCasted->m_pipeline;

    //// temporary, will be removed once everything operates through CommitDrawState().
    //RenderInterface::State& state = RI.state;
    //state.Reset();

    static_assert(std::is_trivially_destructible_v<BindGraphicsPipeline>);
    // cmdCasted->~BindGraphicsPipeline();
}

#pragma endregion BindGraphicsPipeline

#pragma region BindComputePipeline

void BindComputePipeline::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    RenderInterface::State& state = RI.state;

    BindComputePipeline* cmdCasted = static_cast<BindComputePipeline*>(cmd);

    cmdCasted->m_pipeline->Bind(commandBuffer);

    state.boundComputePipeline = cmdCasted->m_pipeline;

    static_assert(std::is_trivially_destructible_v<BindComputePipeline>);
    // cmdCasted->~BindComputePipeline();
}

#pragma endregion BindComputePipeline

#pragma region BindRayTracingPipeline

void BindRayTracingPipeline::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    RenderInterface::State& state = RI.state;

    BindRayTracingPipeline* cmdCasted = static_cast<BindRayTracingPipeline*>(cmd);

    cmdCasted->m_pipeline->Bind(commandBuffer);

    state.boundRayTracingPipeline = cmdCasted->m_pipeline;

    static_assert(std::is_trivially_destructible_v<BindRayTracingPipeline>);
    // cmdCasted->~BindRayTracingPipeline();
}

#pragma endregion BindRayTracingPipeline

#pragma region DispatchCompute

void DispatchCompute::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    DispatchCompute* cmdCasted = static_cast<DispatchCompute*>(cmd);

    ComputePipeline* pipeline = cmdCasted->m_pipeline;

    if (pipeline == nullptr)
    {
        RI.CommitPipelineState(PSO_Compute, commandBuffer);

        pipeline = RI.state.boundComputePipeline;
        AssertDebug(pipeline != nullptr, "No compute pipeline set, call SetCurrentShader before DispatchCompute() without pipeline passed");
    }

    pipeline->Dispatch(commandBuffer, cmdCasted->m_workgroupCount);

    static_assert(std::is_trivially_destructible_v<DispatchCompute>);
    // cmdCasted->~DispatchCompute();
}

#pragma endregion DispatchCompute

#pragma region TraceRays

void TraceRays::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    TraceRays* cmdCasted = static_cast<TraceRays*>(cmd);

    RayTracingPipeline* pipeline = cmdCasted->m_pipeline;

    if (pipeline == nullptr)
    {
        RI.CommitPipelineState(PSO_RayTracing, commandBuffer);

        pipeline = RI.state.boundRayTracingPipeline;
        AssertDebug(pipeline != nullptr, "No rayTracing pipeline set, call SetCurrentShader before TraceRays() without pipeline passed");
    }

    pipeline->TraceRays(commandBuffer, cmdCasted->m_workgroupCount);

    static_assert(std::is_trivially_destructible_v<TraceRays>);
    // cmdCasted->~TraceRays();
}

#pragma endregion TraceRays

#pragma region DrawQuad

static Handle<Mesh> g_quadMesh;

void DrawQuad::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    if (HYP_UNLIKELY(!g_quadMesh))
    {
        g_quadMesh = MeshBuilder::Quad();
        g_quadMesh->SetFlags(MeshFlags::ViewIndependent);
        InitObject(g_quadMesh);

        CurrentThreadObject()->AddOnExitCallback([]()
            {
                g_quadMesh.Reset();
            });
    }

    commandBuffer->BindIndexBuffer(g_quadMesh->GetIndexBuffer());
    commandBuffer->BindVertexBuffer(g_quadMesh->GetVertexBuffer());
    commandBuffer->DrawIndexed(6);

    static_assert(std::is_trivially_destructible_v<DrawQuad>);

    // reinterpret_cast<DrawQuad*>(cmd)->~DrawQuad();
}

#pragma endregion DrawQuad

#pragma region SetStencilState

void SetStencilState::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    SetStencilState* cmdCasted = static_cast<SetStencilState*>(cmd);

    RenderInterface::State& state = RI.state;

    if (state.stencilReference != cmdCasted->m_referenceValue
        || state.stencilCompareMask != cmdCasted->m_compareMask
        || state.stencilWriteMask != cmdCasted->m_writeMask)
    {
        // set stencil state
        state.stencilReference = cmdCasted->m_referenceValue;
        state.stencilCompareMask = cmdCasted->m_compareMask;
        state.stencilWriteMask = cmdCasted->m_writeMask;

        // invalidate pipeline state
        state.boundGraphicsPipeline = nullptr;

        state.dirtyUniforms |= (state.validUniforms | state.dirtyBufferOffsets);
        state.validUniforms = 0;

        Memory::Fill(state.prevBoundDescriptorSets, 0, sizeof(state.prevBoundDescriptorSets));
    }

    static_assert(std::is_trivially_destructible_v<SetStencilState>);
    // cmdCasted->~SetStencilState();
}

#pragma endregion SetStencilState

#pragma region SetCurrentShader

void SetCurrentShader::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetCurrentShader* cmdCasted = static_cast<SetCurrentShader*>(cmd);

    RenderInterface::State& state = RI.state;

    ShaderDesc& shaderDesc = cmdCasted->shaderDesc;

    // merge shared global properties with the one we're setting
    MergeGlobalShaderProperties(shaderDesc.properties);

    state.attributes.SetShaderName(shaderDesc.name);
    state.attributes.SetShaderProperties(shaderDesc.properties);

    static_assert(std::is_trivially_destructible_v<SetCurrentShader>);
    // cmdCasted->~SetCurrentShader();
}

#pragma endregion SetCurrentShader

#pragma region SetCurrentViewport

void SetCurrentViewport::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetCurrentViewport* cmdCasted = static_cast<SetCurrentViewport*>(cmd);

    Framebuffer* framebuffer = nullptr;

    RI.state.viewport = cmdCasted->viewport;

    static_assert(std::is_trivially_destructible_v<SetCurrentViewport>);
    // cmdCasted->~SetCurrentViewport();
}

#pragma endregion SetCurrentViewport

#pragma region SetTopology

void SetTopology::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetTopology* cmdCasted = static_cast<SetTopology*>(cmd);

    if (RI.state.attributes.GetMeshAttributes().topology == cmdCasted->topology)
        return;

    RI.state.attributes.GetMeshAttributes().topology = cmdCasted->topology;

    static_assert(std::is_trivially_destructible_v<SetTopology>);
    // cmdCasted->~SetTopology();
}

#pragma endregion SetTopology

#pragma region SetInputLayout

void SetInputLayout::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetInputLayout* cmdCasted = static_cast<SetInputLayout*>(cmd);

    RenderInterface::State& state = RI.state;

    if (state.attributes.GetMeshAttributes().inputLayout == cmdCasted->inputLayout)
        return;

    state.attributes.GetMeshAttributes().inputLayout = cmdCasted->inputLayout;

    static_assert(std::is_trivially_destructible_v<SetInputLayout>);
    // cmdCasted->~SetInputLayout();
}

#pragma endregion SetInputLayout

#pragma region SetCurrentBlendFunction

void SetCurrentBlendFunction::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetCurrentBlendFunction* cmdCasted = static_cast<SetCurrentBlendFunction*>(cmd);

    if (RI.state.attributes.GetMaterialAttributes().blendFunction == cmdCasted->blendFunction)
        return;

    RI.state.attributes.GetMaterialAttributes().blendFunction = cmdCasted->blendFunction;

    static_assert(std::is_trivially_destructible_v<SetCurrentBlendFunction>);
    // cmdCasted->~SetCurrentBlendFunction();
}

#pragma endregion SetCurrentBlendFunction

#pragma region SetDepthWrite

void SetDepthWrite::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetDepthWrite* cmdCasted = static_cast<SetDepthWrite*>(cmd);

    if (cmdCasted->depthWrite)
    {
        if (RI.state.attributes.GetMaterialAttributes().flags & MAF_DEPTH_WRITE)
            return;

        RI.state.attributes.GetMaterialAttributes().flags |= MAF_DEPTH_WRITE;
    }
    else
    {
        if (!(RI.state.attributes.GetMaterialAttributes().flags & MAF_DEPTH_WRITE))
            return;

        RI.state.attributes.GetMaterialAttributes().flags &= ~MAF_DEPTH_WRITE;
    }

    static_assert(std::is_trivially_destructible_v<SetDepthWrite>);
    // cmdCasted->~SetDepthWrite();
}

#pragma endregion SetDepthWrite

#pragma region SetDepthTest

void SetDepthTest::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetDepthTest* cmdCasted = static_cast<SetDepthTest*>(cmd);

    if (cmdCasted->depthTest)
    {
        if (RI.state.attributes.GetMaterialAttributes().flags & MAF_DEPTH_TEST)
            return;

        RI.state.attributes.GetMaterialAttributes().flags |= MAF_DEPTH_TEST;
    }
    else
    {
        if (!(RI.state.attributes.GetMaterialAttributes().flags & MAF_DEPTH_TEST))
            return;

        RI.state.attributes.GetMaterialAttributes().flags &= ~MAF_DEPTH_TEST;
    }

    static_assert(std::is_trivially_destructible_v<SetDepthTest>);
    // cmdCasted->~SetDepthTest();
}

#pragma endregion SetDepthTest

#pragma region SetDepthBias

void SetDepthBias::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetDepthBias* cmdCasted = static_cast<SetDepthBias*>(cmd);

    RenderInterface::State& state = RI.state;

    const bool enableDepthBias = cmdCasted->depthBias != 0;

    if (enableDepthBias)
    {
        if ((state.attributes.GetMaterialAttributes().flags & MAF_DEPTH_BIAS)
            && state.attributes.GetMaterialAttributes().depthBias == cmdCasted->depthBias
            && MathUtil::ApproxEqual(state.attributes.GetMaterialAttributes().depthBiasSlope, cmdCasted->depthBiasSlope))
        {
            return;
        }

        state.attributes.GetMaterialAttributes().flags |= MAF_DEPTH_BIAS;
        state.attributes.GetMaterialAttributes().depthBias = cmdCasted->depthBias;
        state.attributes.GetMaterialAttributes().depthBiasSlope = cmdCasted->depthBiasSlope;
    }
    else
    {
        if (!(state.attributes.GetMaterialAttributes().flags & MAF_DEPTH_BIAS))
            return;

        state.attributes.GetMaterialAttributes().flags &= ~MAF_DEPTH_BIAS;
    }

    static_assert(std::is_trivially_destructible_v<SetDepthBias>);
    // cmdCasted->~SetDepthBias();
}

#pragma endregion SetDepthBias

#pragma region SetDepthClamp

void SetDepthClamp::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetDepthClamp* cmdCasted = static_cast<SetDepthClamp*>(cmd);

    if (cmdCasted->depthClamp)
    {
        if (RI.state.attributes.GetMaterialAttributes().flags & MAF_DEPTH_CLAMP)
            return;

        RI.state.attributes.GetMaterialAttributes().flags |= MAF_DEPTH_CLAMP;
    }
    else
    {
        if (!(RI.state.attributes.GetMaterialAttributes().flags & MAF_DEPTH_CLAMP))
            return;

        RI.state.attributes.GetMaterialAttributes().flags &= ~MAF_DEPTH_CLAMP;
    }

    static_assert(std::is_trivially_destructible_v<SetDepthClamp>);
    // cmdCasted->~SetDepthClamp();
}

#pragma endregion SetDepthClamp

#pragma region SetStencilTest

void SetStencilTest::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetStencilTest* cmdCasted = static_cast<SetStencilTest*>(cmd);

    if (cmdCasted->stencilTest)
    {
        if (RI.state.attributes.GetMaterialAttributes().flags & MAF_STENCIL_TEST)
            return;

        RI.state.attributes.GetMaterialAttributes().flags |= MAF_STENCIL_TEST;
    }
    else
    {
        if (!(RI.state.attributes.GetMaterialAttributes().flags & MAF_STENCIL_TEST))
            return;

        RI.state.attributes.GetMaterialAttributes().flags &= ~MAF_STENCIL_TEST;
    }

    static_assert(std::is_trivially_destructible_v<SetStencilTest>);
    // cmdCasted->~SetStencilTest();
}

#pragma endregion SetStencilTest

#pragma region SetStencilFunction

void SetStencilFunction::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetStencilFunction* cmdCasted = static_cast<SetStencilFunction*>(cmd);

    if (RI.state.attributes.GetMaterialAttributes().stencilFunction == cmdCasted->stencilFunction)
        return;

    RI.state.attributes.GetMaterialAttributes().stencilFunction = cmdCasted->stencilFunction;

    static_assert(std::is_trivially_destructible_v<SetStencilFunction>);
    // cmdCasted->~SetStencilFunction();
}

#pragma endregion SetStencilFunction

#pragma region SetFillMode

void SetFillMode::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetFillMode* cmdCasted = static_cast<SetFillMode*>(cmd);

    if (RI.state.attributes.GetMaterialAttributes().fillMode == cmdCasted->fillMode)
        return;

    RI.state.attributes.GetMaterialAttributes().fillMode = cmdCasted->fillMode;

    static_assert(std::is_trivially_destructible_v<SetFillMode>);
    // cmdCasted->~SetFillMode();
}

#pragma endregion SetFillMode

#pragma region SetFaceCullMode

void SetFaceCullMode::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetFaceCullMode* cmdCasted = static_cast<SetFaceCullMode*>(cmd);

    if (RI.state.attributes.GetMaterialAttributes().cullFaces == cmdCasted->faceCullMode)
        return;

    RI.state.attributes.GetMaterialAttributes().cullFaces = cmdCasted->faceCullMode;

    static_assert(std::is_trivially_destructible_v<SetFaceCullMode>);
    // cmdCasted->~SetFaceCullMode();
}

#pragma endregion SetFaceCullMode

#pragma region SetShaderUniform

SetShaderUniform::SetShaderUniform(uint32 uniformIndex, StringHash name, const StructuredBuffer& structuredBuffer, uint32 elementOffset)
    : uniformIndex(uniformIndex),
      shaderDataOffset(elementOffset * structuredBuffer.elementSize, structuredBuffer.elementSize),
      uniform({ name, structuredBuffer.gpuBuffer })
{
    AssertDebug(structuredBuffer.gpuBuffer != nullptr);
}

SetShaderUniform::SetShaderUniform(uint32 uniformIndex, StringHash name, const RWStructuredBuffer& rwStructuredBuffer, uint32 elementOffset)
    : uniformIndex(uniformIndex),
      shaderDataOffset(elementOffset * rwStructuredBuffer.elementSize, rwStructuredBuffer.elementSize),
      uniform({ name, rwStructuredBuffer.gpuBuffer })
{
    AssertDebug(rwStructuredBuffer.gpuBuffer != nullptr);
}

SetShaderUniform::SetShaderUniform(uint32 uniformIndex, StringHash name, const ByteAddressBuffer& byteAddressBuffer, uint32 byteOffset)
    : uniformIndex(uniformIndex),
      shaderDataOffset(byteOffset, 0),
      uniform({ name, byteAddressBuffer.gpuBuffer })
{
    AssertDebug(byteAddressBuffer.gpuBuffer != nullptr);
}

void SetShaderUniform::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    SetShaderUniform* cmdCasted = static_cast<SetShaderUniform*>(cmd);

    RenderInterface::State& state = RI.state;

    ShaderUniform& uniform = state.shaderUniforms[cmdCasted->uniformIndex];

    if (uniform != cmdCasted->uniform                                   // uniform packet differs
        || !(state.validUniforms & (1u << cmdCasted->uniformIndex)))    // previous bound is invalid
    {
        uniform = cmdCasted->uniform;

        state.dirtyUniforms |= (1u << cmdCasted->uniformIndex);
    }

    if (cmdCasted->uniform.type == ShaderUniform::UT_Buffer)
    {
        // strides differ for buffer; needs rebind
        if (state.shaderUniformBufferOffsetStrides[cmdCasted->uniformIndex] != cmdCasted->shaderDataOffset.stride)
        {
            state.dirtyUniforms |= (1u << cmdCasted->uniformIndex);
        }

        // buffer offset + stride updating
        state.shaderUniformBufferOffsets[cmdCasted->uniformIndex] = cmdCasted->shaderDataOffset.offset;
        state.shaderUniformBufferOffsetStrides[cmdCasted->uniformIndex] = cmdCasted->shaderDataOffset.stride;

        state.dirtyBufferOffsets |= (1u << cmdCasted->uniformIndex);
    }
    else
    {
        // unset dirty buffer offset bit if it is not a buffer
        state.dirtyBufferOffsets &= ~(1u << cmdCasted->uniformIndex);
    }

    static_assert(std::is_trivially_destructible_v<SetShaderUniform>);
    // cmdCasted->~SetShaderUniform();
}

#pragma endregion SetShaderUniform

#pragma region SetShaderUniforms

void SetShaderUniforms::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    SetShaderUniforms* cmdCasted = static_cast<SetShaderUniforms*>(cmd);

    RenderInterface::State& state = RI.state;

    const ShaderUniforms& srcUniforms = cmdCasted->shaderUniforms;
    AssertDebug(cmdCasted->startIndex + srcUniforms.count <= state.MaxShaderUniforms);

    for (uint32 i = cmdCasted->startIndex; i < cmdCasted->startIndex + srcUniforms.count; i++)
    {
        const ShaderUniform& srcUniform = srcUniforms.uniforms[i - cmdCasted->startIndex];

        ShaderUniform& dstUniform = state.shaderUniforms[i];

        if (dstUniform != srcUniform                                   // uniform packet differs
            || !(state.validUniforms & (1u << i)))    // previous bound is invalid
        {
            dstUniform = srcUniform;

            state.dirtyUniforms |= (1u << i);
        }

        if (srcUniform.type == ShaderUniform::UT_Buffer)
        {
            const size_t bufferStride = srcUniforms.bufferStrides[i - cmdCasted->startIndex];
            const size_t bufferOffset = srcUniforms.bufferOffsets[i - cmdCasted->startIndex];

            // strides differ for buffer; needs rebind
            if (state.shaderUniformBufferOffsetStrides[i] != bufferStride)
            {
                state.dirtyUniforms |= (1u << i);
            }

            // buffer offset + stride updating
            state.shaderUniformBufferOffsets[i] = bufferOffset;
            state.shaderUniformBufferOffsetStrides[i] = bufferStride;

            state.dirtyBufferOffsets |= (1u << i);
        }
        else
        {
            // unset dirty buffer offset bit if it is not a buffer
            state.dirtyBufferOffsets &= ~(1u << i);
        }
    }

    static_assert(std::is_trivially_destructible_v<SetShaderUniforms>);
    // cmdCasted->~SetShaderUniforms();
}

#pragma endregion SetShaderUniforms

#pragma region CommitDrawState

void CommitDrawState::InvokeStatic(CmdBase*, CommandBuffer* commandBuffer)
{
    RI.CommitDrawState(commandBuffer);

    static_assert(std::is_trivially_destructible_v<CommitDrawState>);
    // cmdCasted->~CommitDrawState();
}

#pragma endregion CommitDrawState

#pragma region FillImage

FillImage::FillImage(GpuImage* image, float value)
    : m_image(image),
      m_value(value),
      m_offset(Vec3u::Zero()),
      m_extent(image ? image->GetTextureDesc().extent : Vec3u::One())
{
}

FillImage::FillImage(GpuImage* image, float value, const ImageSubResource& subResource)
    : m_image(image),
      m_value(value),
      m_subResource(subResource),
      m_offset(Vec3u::Zero()),
      m_extent(image ? image->GetTextureDesc().extent : Vec3u::One())
{
}

FillImage::FillImage(GpuImage* image, float value, const ImageSubResource& subResource, const Vec3u& offset, const Vec3u& extent)
    : m_image(image),
      m_value(value),
      m_subResource(subResource),
      m_offset(offset),
      m_extent(extent)
{
}

void FillImage::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    FillImage* cmdCasted = static_cast<FillImage*>(cmd);

    cmdCasted->m_image->Fill(
        commandBuffer,
        cmdCasted->m_value,
        cmdCasted->m_subResource,
        cmdCasted->m_offset,
        cmdCasted->m_extent);

    static_assert(std::is_trivially_destructible_v<FillImage>);
}

#pragma endregion FillImage

} // namespace Hyperion
