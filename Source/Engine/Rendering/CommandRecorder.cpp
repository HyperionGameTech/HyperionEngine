/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <Rendering/CommandRecorder.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/GraphicsPipelineCache.hpp>
#include <Rendering/DescriptorSetCache.hpp>
#include <Rendering/DescriptorSet.hpp>
#include <Rendering/GraphicsPipeline.hpp>
#include <Rendering/ComputePipeline.hpp>
#include <Rendering/ShaderInstance.hpp>
#include <Rendering/Shader.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/RayTracingPipeline.hpp>
#include <Rendering/AccelerationStructure.hpp>
#include <Rendering/ScratchImageAllocator.hpp>
#include <Rendering/GpuImageView.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/SamplerCache.hpp>
#include <Rendering/Sampler.hpp>
#include <Rendering/TextureViewCache.hpp>

#include <Rendering/Util/ShaderCompiler.hpp>

#include <Rendering/GpuTimerBackend.hpp>

#include <Core/Reflection/Enum.hpp>

#include <Scene/View.hpp>

#include <Rendering/Util/MeshBuilder.hpp>

namespace Hyperion {

#pragma region CommandRecorderBase


#pragma endregion CommandRecorderBase

#pragma region TCommandRecorder

template <>
void TCommandRecorder<RenderAllocator>::Prepare(Frame* frame)
{
    Assert(frame != nullptr);

    for (size_t i = 0; i < m_headerCount; i++)
    {
        CmdHeader& header = m_headersPtr[i];

        CmdBase* cmdDataPtr = reinterpret_cast<CmdBase*>(m_buffer.Data() + header.offset);
        AssertDebug(header.offset < m_buffer.Size());

        if (header.prepareFnPtr != nullptr)
        {
            header.prepareFnPtr(cmdDataPtr, frame);
        }
    }
}

template <>
void TCommandRecorder<RenderAllocator>::Execute(CommandBuffer* commandBuffer)
{
    AssertDebug(commandBuffer != nullptr);

    for (size_t i = 0; i < m_headerCount; i++)
    {
        CmdHeader& header = m_headersPtr[i];

        CmdBase* cmdDataPtr = reinterpret_cast<CmdBase*>(m_buffer.Data() + header.offset);

        InvokeCmdFnPtr invokeFnPtr = header.invokeFnPtr;
        invokeFnPtr(cmdDataPtr, commandBuffer);
    }

    m_headerCount = 0;
    m_offset = 0;

    m_writableState.Release();
}

template <>
void TCommandRecorder<RenderAllocator>::Submit()
{
    Done();

    Assert(writeCount == 0);

    { // Submit to transient command buffer
        CommandBuffer& commandBuffer = RI.GetTransientCommandBuffer();

        Execute(&commandBuffer);

        RI.SubmitTransientCommandBuffer(commandBuffer);
    }

    // Reset offset and header count
    Reset(/* freeMemory */ false);
}

#pragma endregion TCommandRecorder

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

void Blit::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    Blit* cmdCasted = static_cast<Blit*>(cmd);

    Texture* src = cmdCasted->m_src;
    Texture* dst = cmdCasted->m_dst;

    /* Resolve defaults */
    ImageSubResource srcSubResource = cmdCasted->m_srcSubResource;
    ImageSubResource dstSubResource = cmdCasted->m_dstSubResource;

    Rect<uint32> srcRect = cmdCasted->m_srcRect;
    Rect<uint32> dstRect = cmdCasted->m_dstRect;

    if (!cmdCasted->m_hasRect)
    {
        /* Default rects = full extent */
        const Vec3u srcExtent = src->GetExtent();
        const Vec3u dstExtent = dst->GetExtent();

        srcRect = Rect<uint32> { 0, 0, srcExtent.x, srcExtent.y };
        dstRect = Rect<uint32> { 0, 0, dstExtent.x, dstExtent.y };
    }

    const TextureDesc& srcDesc = src->GetTextureDesc();
    const TextureDesc& dstDesc = dst->GetTextureDesc();

    if (!cmdCasted->m_hasSubResource)
    {
        /* Default subresource = all mips and layers */
        srcSubResource.baseMipLevel = 0;
        srcSubResource.numLevels = srcDesc.NumMips();
        srcSubResource.baseArrayLayer = 0;
        srcSubResource.numLayers = srcDesc.NumArrayLayers();

        dstSubResource.baseMipLevel = 0;
        dstSubResource.numLevels = dstDesc.NumMips();
        dstSubResource.baseArrayLayer = 0;
        dstSubResource.numLayers = dstDesc.NumArrayLayers();
    }

#ifdef HYP_VULKAN
    src->GetGpuImage()->InsertBarrier(commandBuffer, srcSubResource, RS_COPY_SRC, ShaderModuleType::None);
    dst->GetGpuImage()->InsertBarrier(commandBuffer, dstSubResource, RS_COPY_DST, ShaderModuleType::None);

    dst->GetGpuImage()->Blit(commandBuffer, src->GetGpuImage(), srcRect, dstRect, srcSubResource, dstSubResource);
#else
    if (!RI.IsSupportedFormat(dstDesc.format, ImageSupport::UnorderedAccess))
    {
        HYP_LOG(RenderingBackend, Warning, "Blit: Destination format {} does not support UnorderedAccess, "
                                           "cannot blit as it requires using a compute shader!",
            EnumToString(dstDesc.format));

        return;
    }

    /* Resolve subresource ranges (UINT8_MAX / UINT16_MAX sentinels mean "all remaining") */
    const uint8 srcNumLevels = (srcSubResource.numLevels == UINT8_MAX)
        ? uint8(srcDesc.NumMips() - srcSubResource.baseMipLevel)
        : srcSubResource.numLevels;
    const uint8 dstNumLevels = (dstSubResource.numLevels == UINT8_MAX)
        ? uint8(dstDesc.NumMips() - dstSubResource.baseMipLevel)
        : dstSubResource.numLevels;
    const uint16 srcNumLayers = (srcSubResource.numLayers == UINT16_MAX)
        ? uint16(srcDesc.NumArrayLayers() - srcSubResource.baseArrayLayer)
        : srcSubResource.numLayers;
    const uint16 dstNumLayers = (dstSubResource.numLayers == UINT16_MAX)
        ? uint16(dstDesc.NumArrayLayers() - dstSubResource.baseArrayLayer)
        : dstSubResource.numLayers;

    const uint8 numLevelsToCopy = MathUtil::Min(srcNumLevels, dstNumLevels);
    const uint16 numLayersToCopy = MathUtil::Min(srcNumLayers, dstNumLayers);

    RenderInterface::State& state = RI.state;
    state.attributes.SetShaderName(NAME("GenerateMipmap"));
    state.attributes.SetShaderProperties(ShaderPropertySet {});

    Sampler* linearSampler = RI.samplerCache->GetOrCreate(SamplerDesc { TFM_LINEAR, TFM_LINEAR, TWM_REPEAT });

    struct BlitUniforms
    {
        Vec2u srcDimensions;
        Vec2u dstDimensions;
        uint32 srcMipLevel;
    };

    for (uint8 mipLevel = 0; mipLevel < numLevelsToCopy; mipLevel++)
    {
        const uint8 actualSrcMip = srcSubResource.baseMipLevel + mipLevel;
        const uint8 actualDstMip = dstSubResource.baseMipLevel + mipLevel;

        const Vec3u srcExtent = srcDesc.GetMipExtent(actualSrcMip);
        const Vec3u dstExtent = dstDesc.GetMipExtent(actualDstMip);

        for (uint16 layerIndex = 0; layerIndex < numLayersToCopy; layerIndex++)
        {
            const uint16 actualSrcLayer = srcSubResource.baseArrayLayer + layerIndex;
            const uint16 actualDstLayer = dstSubResource.baseArrayLayer + layerIndex;

            /* Acquire a temporary 2D image with IU_STORAGE | IU_SAMPLED so we
               can perform the blit via compute dispatch without requiring the
               destination image to have IU_STORAGE. */
            Handle<Texture> tempImage = RI.scratchImageAllocator->AcquireScratchImage(TextureType::Texture2D, dstDesc.format, dstExtent);

            if (!tempImage.IsValid())
            {
                HYP_LOG(RenderingBackend, Error, "Blit: Failed to acquire scratch image");

                continue;
            }

            const ImageSubResource srcViewSubResource {
                .baseMipLevel = actualSrcMip,
                .numLevels = 1,
                .baseArrayLayer = actualSrcLayer,
                .numLayers = 1
            };

            const ImageSubResource dstViewSubResource {
                .baseMipLevel = actualDstMip,
                .numLevels = 1,
                .baseArrayLayer = actualDstLayer,
                .numLayers = 1
            };

            /* Create image views for source (SRV) and scratch destination (UAV) */
            const GpuImageViewRef& inputView = RI.textureViewCache->GetOrCreate(src, srcViewSubResource, TextureType::Texture2D);
            const GpuImageViewRef& outputView = RI.textureViewCache->GetOrCreate(tempImage, dstViewSubResource, TextureType::Texture2D);

            /* Barrier source to shader resource, scratch to unordered access */
            src->GetGpuImage()->InsertBarrier(
                commandBuffer,
                srcViewSubResource,
                RS_SHADER_RESOURCE,
                ShaderModuleType::None);

            tempImage->GetGpuImage()->InsertBarrier(
                commandBuffer,
                RS_UNORDERED_ACCESS,
                ShaderModuleType::None);

            /* Set up uniform data */
            BlitUniforms uniforms;
            uniforms.srcDimensions = { srcExtent.x, srcExtent.y };
            uniforms.dstDimensions = { dstExtent.x, dstExtent.y };
            uniforms.srcMipLevel = 0;

            RI.cbufferAllocator->Write(&uniforms);

            GpuBuffer* blitCBuffer = nullptr;
            size_t blitCBufferOffset = 0;
            size_t blitCBufferSize = 0;
            RI.cbufferAllocator->Commit(blitCBuffer, blitCBufferOffset, blitCBufferSize);

            ShaderUniform& inputUniform = state.shaderUniforms[0];
            inputUniform = ShaderUniform("InputTexture"_sh, inputView.Get());
            state.dirtyUniforms |= 1u << 0;

            ShaderUniform& outputUniform = state.shaderUniforms[1];
            outputUniform = ShaderUniform("OutputTexture"_sh, outputView.Get());
            state.dirtyUniforms |= 1u << 1;

            ShaderUniform& ubUniform = state.shaderUniforms[2];
            ubUniform = ShaderUniform("Constants"_sh, blitCBuffer);
            state.shaderUniformBufferOffsets[2] = uint32(blitCBufferOffset);
            state.shaderUniformBufferStrides[2] = uint32(blitCBufferSize);
            state.dirtyUniforms |= 1u << 2;
            state.dirtyBufferOffsets |= 1u << 2;

            ShaderUniform& samplerUniform = state.shaderUniforms[3];
            samplerUniform = ShaderUniform("SamplerLinear"_sh, linearSampler);
            state.dirtyUniforms |= 1u << 3;

            RI.CommitPipelineState(PSO_Compute, commandBuffer);

            ComputePipeline* pipeline = state.boundComputePipeline;
            if (!pipeline)
            {
                HYP_LOG(RenderingBackend, Error, "Blit: Failed to get compute pipeline. "
                    "Shader 'GenerateMipmap' may not be compiled or available.");

                continue;
            }

            pipeline->Dispatch(commandBuffer, {
                (dstExtent.x + 7) / 8,
                (dstExtent.y + 7) / 8,
                1
            });

#ifdef HYP_DX12
            tempImage->GetGpuImage()->InsertUAVBarrier(commandBuffer);
#endif
            /* Barrier scratch to copy source, destination to copy dest */
            tempImage->GetGpuImage()->InsertBarrier(
                commandBuffer,
                RS_COPY_SRC,
                ShaderModuleType::None);

            dst->GetGpuImage()->InsertBarrier(
                commandBuffer,
                dstViewSubResource,
                RS_COPY_DST,
                ShaderModuleType::None);

            /* Copy the computed result from scratch back to the destination */
            dst->GetGpuImage()->CopyFrom(
                commandBuffer,
                tempImage->GetGpuImage().Get(),
                Vec3u::Zero(),
                Vec3u::Zero(),
                dstExtent,
                ImageSubResource { 0, 1, 0, 1 },
                dstViewSubResource);

            /* Barrier destination back to shader resource so subsequent passes can sample it */
            dst->GetGpuImage()->InsertBarrier(
                commandBuffer,
                dstViewSubResource,
                RS_SHADER_RESOURCE,
                ShaderModuleType::None);
        }
    }
#endif
}
#pragma endregion Blit

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

GenerateMipmaps::GenerateMipmaps(Texture* inTexture)
    : inTexture(inTexture)
{
    AssertDebug(inTexture != nullptr && inTexture->IsCreated());
}

void GenerateMipmaps::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    GenerateMipmaps* cmdCasted = static_cast<GenerateMipmaps*>(cmd);

    Texture* inTexture = cmdCasted->inTexture;

#ifdef HYP_VULKAN
    inTexture->GetGpuImage()->GenerateMipmaps(commandBuffer);
#else
    const TextureDesc& desc = inTexture->GetTextureDesc();

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

    /* Allocate per-mip views and cbuffers for the temp image */
    Array<GpuImageView*, RenderAllocator> inputViews;
    Array<GpuImageView*, RenderAllocator> outputViews;

    Array<GpuBuffer*, RenderAllocator> cbuffers;
    Array<size_t, RenderAllocator> cbufferOffsets;
    Array<size_t, RenderAllocator> cbufferSizes;

    inputViews.Reserve(numMips - 1);
    outputViews.Reserve(numMips - 1);
    cbuffers.Reserve(numMips - 1);
    cbufferOffsets.Reserve(numMips - 1);
    cbufferSizes.Reserve(numMips - 1);

    for (uint16 layer = 0; layer < numLayers; layer++)
    {
        inputViews.Resize(0);
        outputViews.Resize(0);

        cbuffers.Resize(0);
        cbufferOffsets.Resize(0);
        cbufferSizes.Resize(0);

        /* Acquire a temporary 2D image with IU_STORAGE | IU_SAMPLED so we
           can generate mips via compute dispatch without requiring the
           source image to have IU_STORAGE. */
        Handle<Texture> tempImage = RI.scratchImageAllocator->AcquireScratchImage(TextureType::Texture2D, desc.format, desc.extent);

        if (!tempImage.IsValid())
        {
            HYP_LOG(RenderingBackend, Error, "GenerateMipmaps: Failed to acquire scratch image");
            continue;
        }

        inTexture->GetGpuImage()->InsertBarrier(
            commandBuffer,
            ImageSubResource { .baseMipLevel = 0, .numLevels = 1, .baseArrayLayer = layer, .numLayers = 1 },
            RS_COPY_SRC,
            ShaderModuleType::None);

        tempImage->GetGpuImage()->InsertBarrier(
            commandBuffer,
            RS_COPY_DST,
            ShaderModuleType::None);

        tempImage->GetGpuImage()->CopyFrom(
            commandBuffer,
            inTexture->GetGpuImage(),
            Vec3u::Zero(),
            Vec3u::Zero(),
            desc.extent,
            ImageSubResource { .baseMipLevel = 0, .numLevels = 1, .baseArrayLayer = layer, .numLayers = 1 },
            ImageSubResource { .baseMipLevel = 0, .numLevels = 1, .baseArrayLayer = 0, .numLayers = 1 });

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

            const GpuImageViewRef& inputView = RI.textureViewCache->GetOrCreate(
                tempImage, srcMip, 1, 0, 1);

            inputViews.PushBack(inputView);

            const GpuImageViewRef& outputView = RI.textureViewCache->GetOrCreate(
                tempImage, mip, 1, 0, 1);

            outputViews.PushBack(outputView);

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

            tempImage->GetGpuImage()->InsertBarrier(
                commandBuffer,
                ImageSubResource { .baseMipLevel = srcMip, .numLevels = 1, .baseArrayLayer = 0, .numLayers = 1 },
                RS_SHADER_RESOURCE,
                ShaderModuleType::None);

            tempImage->GetGpuImage()->InsertBarrier(
                commandBuffer,
                ImageSubResource { .baseMipLevel = mip, .numLevels = 1, .baseArrayLayer = 0, .numLayers = 1 },
                RS_UNORDERED_ACCESS,
                ShaderModuleType::None);

            ShaderUniform& inputUniform = state.shaderUniforms[0];
            inputUniform = ShaderUniform("InputTexture"_sh, inputViews[srcMip]);
            state.dirtyUniforms |= 1u << 0;

            ShaderUniform& outputUniform = state.shaderUniforms[1];
            outputUniform = ShaderUniform("OutputTexture"_sh, outputViews[srcMip]);
            state.dirtyUniforms |= 1u << 1;

            // Use this mip's own cbuffer with dynamic offset
            ShaderUniform& ubUniform = state.shaderUniforms[2];
            ubUniform = ShaderUniform("Constants"_sh, cbuffers[srcMip]);
            state.shaderUniformBufferOffsets[2] = uint32(cbufferOffsets[srcMip]);
            state.shaderUniformBufferStrides[2] = uint32(cbufferSizes[srcMip]);
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
            tempImage->GetGpuImage()->InsertUAVBarrier(commandBuffer);
#endif
        }

#ifdef HYP_DX12
        tempImage->GetGpuImage()->InsertUAVBarrier(commandBuffer);
#endif

        /* Copy each generated mip from the temp image back to the source */
        for (uint8 mip = 1; mip < numMips; mip++)
        {
            const Vec3u mipExtent = desc.GetMipExtent(mip);

            tempImage->GetGpuImage()->InsertBarrier(
                commandBuffer,
                ImageSubResource { .baseMipLevel = mip, .numLevels = 1, .baseArrayLayer = 0, .numLayers = 1 },
                RS_COPY_SRC,
                ShaderModuleType::None);

            inTexture->GetGpuImage()->InsertBarrier(
                commandBuffer,
                ImageSubResource { .baseMipLevel = mip, .numLevels = 1, .baseArrayLayer = layer, .numLayers = 1 },
                RS_COPY_DST,
                ShaderModuleType::None);

            inTexture->GetGpuImage()->CopyFrom(
                commandBuffer,
                tempImage->GetGpuImage().Get(),
                Vec3u::Zero(),
                Vec3u::Zero(),
                mipExtent,
                ImageSubResource { .baseMipLevel = mip, .numLevels = 1, .baseArrayLayer = 0, .numLayers = 1 },
                ImageSubResource { .baseMipLevel = mip, .numLevels = 1, .baseArrayLayer = layer, .numLayers = 1 });

            inTexture->GetGpuImage()->InsertBarrier(
                commandBuffer,
                ImageSubResource { .baseMipLevel = mip, .numLevels = 1, .baseArrayLayer = layer, .numLayers = 1 },
                RS_SHADER_RESOURCE,
                ShaderModuleType::None);
        }
    }
#endif // !HYP_VULKAN

    inTexture->GetGpuImage()->InsertBarrier(
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

#pragma region InsertUAVBarrier

void InsertUAVBarrier::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    InsertUAVBarrier* cmdCasted = static_cast<InsertUAVBarrier*>(cmd);

    AssertDebug(cmdCasted->m_image != nullptr);
    cmdCasted->m_image->InsertUAVBarrier(commandBuffer);
}

#pragma endregion InsertUAVBarrier

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
        g_quadMesh->UploadGpuData();

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

#pragma region SetDepthCompareOp

void SetDepthCompareOp::InvokeStatic(CmdBase* cmd, CommandBuffer*)
{
    SetDepthCompareOp* cmdCasted = static_cast<SetDepthCompareOp*>(cmd);

    if (RI.state.attributes.GetMaterialAttributes().depthCompareOp == cmdCasted->compareOp)
        return;

    RI.state.attributes.GetMaterialAttributes().depthCompareOp = cmdCasted->compareOp;

    static_assert(std::is_trivially_destructible_v<SetDepthCompareOp>);
}

#pragma endregion SetDepthCompareOp

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
        if (state.shaderUniformBufferStrides[cmdCasted->uniformIndex] != cmdCasted->shaderDataOffset.stride)
        {
            state.dirtyUniforms |= (1u << cmdCasted->uniformIndex);
        }

        // buffer offset + stride updating
        state.shaderUniformBufferOffsets[cmdCasted->uniformIndex] = cmdCasted->shaderDataOffset.offset;
        state.shaderUniformBufferStrides[cmdCasted->uniformIndex] = cmdCasted->shaderDataOffset.stride;

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
            if (state.shaderUniformBufferStrides[i] != bufferStride)
            {
                state.dirtyUniforms |= (1u << i);
            }

            // buffer offset + stride updating
            state.shaderUniformBufferOffsets[i] = bufferOffset;
            state.shaderUniformBufferStrides[i] = bufferStride;

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

#pragma region RecordGpuTimestamp

void RecordGpuTimestamp::InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
{
    RecordGpuTimestamp* cmdCasted = static_cast<RecordGpuTimestamp*>(cmd);

    if (cmdCasted->m_isStart)
    {
        RI.RecordStartTimestamp(commandBuffer, cmdCasted->m_timer);
    }
    else
    {
        RI.RecordStopTimestamp(commandBuffer, cmdCasted->m_timer);
    }

    static_assert(std::is_trivially_destructible_v<RecordGpuTimestamp>);
}

#pragma endregion RecordGpuTimestamp

} // namespace Hyperion
