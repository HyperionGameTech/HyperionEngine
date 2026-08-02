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

#include <Rendering/GpuTimerBackend.hpp>

#include <Core/Reflection/Enum.hpp>

#include <Scene/View.hpp>

#include <Rendering/Util/MeshBuilder.hpp>

namespace Hyperion {

static Handle<Mesh> g_quadMesh;

void MergeGlobalShaderProperties(ShaderPropertySet& out);

#pragma region TCommandRecorder

template <>
void TCommandRecorder<RenderAllocator>::Execute(CommandBuffer* commandBuffer)
{
    AssertDebug(commandBuffer != nullptr);

    for (size_t i = 0; i < m_headerCount; i++)
    {
        CmdHeader& header = m_headersPtr[i];

        CmdBase* cmdDataPtr = reinterpret_cast<CmdBase*>(m_buffer.Data() + header.offset);

        AssertDebug(header.offset < m_buffer.Size(), "Header has offset {} which is greater or equal to the buffer's size ({})",
                    uint64(header.offset), m_buffer.Size());

        if (!header.IsCustom())
        {
            const CommandType type = header.GetCommandType();

            switch (type)
            {
            case CommandType::BindVertexBuffer:
            {
                auto* cmd = static_cast<BindVertexBuffer*>(cmdDataPtr);
                commandBuffer->BindVertexBuffer(cmd->m_buffer);
            }
            break;
            case CommandType::BindIndexBuffer:
            {
                auto* cmd = static_cast<BindIndexBuffer*>(cmdDataPtr);
                commandBuffer->BindIndexBuffer(cmd->m_buffer);
            }
            break;
            case CommandType::DrawIndexed:
            {
                auto* cmd = static_cast<DrawIndexed*>(cmdDataPtr);
                commandBuffer->DrawIndexed(cmd->m_numIndices, cmd->m_numInstances, cmd->m_instanceIndex);
            }
            break;
            case CommandType::DrawIndexedIndirect:
            {
                auto* cmd = static_cast<DrawIndexedIndirect*>(cmdDataPtr);
#if HYP_VULKAN
                AssertDebug(cmd->m_bufferOffset + 20 <= cmd->m_buffer->Size());
#endif
                commandBuffer->DrawIndexedIndirect(cmd->m_buffer, cmd->m_bufferOffset);
            }
            break;
            case CommandType::DrawQuad:
            {
                if (HYP_UNLIKELY(!g_quadMesh))
                {
                    g_quadMesh = MeshBuilder::Quad();
                    g_quadMesh->SetFlags(MeshFlags::ViewIndependent);
                    g_quadMesh->UploadGpuData();

                    CurrentThreadObject()->AddOnExitCallback(
                        []()
                        {
                            g_quadMesh.Reset();
                        });
                }

                commandBuffer->BindIndexBuffer(g_quadMesh->GetIndexBuffer());
                commandBuffer->BindVertexBuffer(g_quadMesh->GetVertexBuffer());
                commandBuffer->DrawIndexed(6);

                static_assert(std::is_trivially_destructible_v<DrawQuad>);
            }
            break;
            case CommandType::SetCurrentFramebuffer:
            {
                auto* cmd = static_cast<SetCurrentFramebuffer*>(cmdDataPtr);
                RenderInterface::State& state = RI.state;

                if (cmd->m_framebuffer != state.boundFramebuffer)
                {
                    if (state.boundFramebuffer != nullptr)
                    {
                        state.boundFramebuffer->EndCapture(commandBuffer);
                        state.boundFramebuffer = nullptr;
                    }
                    state.boundGraphicsPipeline = nullptr;
                }
                else if (cmd->m_framebuffer == nullptr && state.boundFramebuffer != nullptr)
                {
                    state.boundFramebuffer->EndCapture(commandBuffer);
                    state.boundFramebuffer = nullptr;
                }

                state.framebuffer = cmd->m_framebuffer;
                state.validUniforms = 0;
                state.dirtyUniforms = 0;
                state.dirtyBufferOffsets = 0;

                static_assert(std::is_trivially_destructible_v<SetCurrentFramebuffer>);
            }
            break;
            case CommandType::ClearFramebuffer:
            {
                auto* cmd = static_cast<ClearFramebuffer*>(cmdDataPtr);
                AssertDebug(cmd->framebuffer != nullptr);

                RenderInterface::State& state = RI.state;
                state.framebuffer = cmd->framebuffer;

                if (state.boundFramebuffer != cmd->framebuffer)
                {
                    if (state.boundFramebuffer != nullptr)
                    {
                        state.boundFramebuffer->EndCapture(commandBuffer);
                        state.boundFramebuffer = nullptr;
                    }

                    state.boundFramebuffer = cmd->framebuffer;

                    cmd->framebuffer->BeginCapture(commandBuffer);
                }

                if (int(cmd->rect.x1) - int(cmd->rect.x0) == 0
                    && int(cmd->rect.y1) - int(cmd->rect.y0) == 0)
                {
                    cmd->framebuffer->Clear(commandBuffer, cmd->attachmentsMask);
                }
                else
                {
                    cmd->framebuffer->Clear(commandBuffer, cmd->rect, cmd->attachmentsMask);
                }

                static_assert(std::is_trivially_destructible_v<ClearFramebuffer>);
            }
            break;
            case CommandType::BindGraphicsPipeline:
            {
                auto* cmd = static_cast<BindGraphicsPipeline*>(cmdDataPtr);

                RenderInterface::State& state = RI.state;
                cmd->m_pipeline->lastFrame = GetFrameCounter();

                if (cmd->m_viewport.position != Vec2i(0, 0) || cmd->m_viewport.extent != Vec2u(0, 0))
                {
                    cmd->m_pipeline->Bind(commandBuffer, cmd->m_viewport.position, cmd->m_viewport.extent);
                }
                else
                {
                    cmd->m_pipeline->Bind(commandBuffer);
                }

                state.boundGraphicsPipeline = cmd->m_pipeline;

                static_assert(std::is_trivially_destructible_v<BindGraphicsPipeline>);
            }
            break;
            case CommandType::BindComputePipeline:
            {
                auto* cmd = static_cast<BindComputePipeline*>(cmdDataPtr);

                RenderInterface::State& state = RI.state;
                cmd->m_pipeline->Bind(commandBuffer);

                state.boundComputePipeline = cmd->m_pipeline;

                static_assert(std::is_trivially_destructible_v<BindComputePipeline>);
            }
            break;
            case CommandType::BindRayTracingPipeline:
            {
                auto* cmd = static_cast<BindRayTracingPipeline*>(cmdDataPtr);

                RenderInterface::State& state = RI.state;
                cmd->m_pipeline->Bind(commandBuffer);

                state.boundRayTracingPipeline = cmd->m_pipeline;

                static_assert(std::is_trivially_destructible_v<BindRayTracingPipeline>);
            }
            break;
            case CommandType::BindDescriptorSet:
            {
                auto* cmd = static_cast<BindDescriptorSet*>(cmdDataPtr);

                switch (cmd->m_pipelineType)
                {
                case 0:
                    cmd->m_descriptorSet->Bind(commandBuffer, cmd->m_graphicsPipeline, cmd->m_offsets, cmd->m_bindIndex);
                    break;
                case 1:
                    cmd->m_descriptorSet->Bind(commandBuffer, cmd->m_computePipeline, cmd->m_offsets, cmd->m_bindIndex);
                    break;
                case 2:
                    cmd->m_descriptorSet->Bind(commandBuffer, cmd->m_rayTracingPipeline, cmd->m_offsets, cmd->m_bindIndex);
                    break;
                default:
                    HYP_UNREACHABLE();
                }

                static_assert(std::is_trivially_destructible_v<BindDescriptorSet>);
            }
            break;
            case CommandType::InsertBarrier:
            {
                auto* cmd = static_cast<InsertBarrier*>(cmdDataPtr);
#if defined(HYP_VULKAN) && defined(HYP_DEBUG_MODE)
                cmd->CheckNotInRenderPass(commandBuffer);
#endif
                if (cmd->m_buffer)
                {
                    cmd->m_buffer->InsertBarrier(commandBuffer, cmd->m_state, cmd->m_shaderModuleType);
                }
                else if (cmd->m_image)
                {
                    if (cmd->m_hasSubResource)
                    {
                        cmd->m_image->InsertBarrier(commandBuffer, cmd->m_subResource, cmd->m_state, cmd->m_shaderModuleType, cmd->m_onlyDepth, cmd->m_onlyStencil);
                    }
                    else
                    {
                        cmd->m_image->InsertBarrier(commandBuffer, cmd->m_state, cmd->m_shaderModuleType, cmd->m_onlyDepth, cmd->m_onlyStencil);
                    }
                }
            }

            break;

            case CommandType::InsertUAVBarrier:
            {
                auto* cmd = static_cast<InsertUAVBarrier*>(cmdDataPtr);
                AssertDebug(cmd->m_image != nullptr);
                cmd->m_image->InsertUAVBarrier(commandBuffer);
            }
            break;
            case CommandType::Blit:
            {
                auto* cmd = static_cast<Blit*>(cmdDataPtr);

                Texture* src = cmd->m_src;
                Texture* dst = cmd->m_dst;

                ImageSubResource srcSubResource = cmd->m_srcSubResource;
                ImageSubResource dstSubResource = cmd->m_dstSubResource;

                Rect<uint32> srcRect = cmd->m_srcRect;
                Rect<uint32> dstRect = cmd->m_dstRect;

                if (!cmd->m_hasRect)
                {
                    const Vec3u srcExtent = src->GetExtent();
                    const Vec3u dstExtent = dst->GetExtent();

                    srcRect = Rect<uint32> { 0, 0, srcExtent.x, srcExtent.y };
                    dstRect = Rect<uint32> { 0, 0, dstExtent.x, dstExtent.y };
                }

                const TextureDesc& srcDesc = src->GetTextureDesc();
                const TextureDesc& dstDesc = dst->GetTextureDesc();

                if (!cmd->m_hasSubResource)
                {
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

                    break;
                }

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

                {
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

                            const GpuImageViewRef& inputView = RI.textureViewCache->GetOrCreate(src, srcViewSubResource, TextureType::Texture2D);
                            const GpuImageViewRef& outputView = RI.textureViewCache->GetOrCreate(tempImage, dstViewSubResource, TextureType::Texture2D);

                            src->GetGpuImage()->InsertBarrier(commandBuffer, srcViewSubResource, RS_SHADER_RESOURCE, ShaderModuleType::None);
                            tempImage->GetGpuImage()->InsertBarrier(commandBuffer, RS_UNORDERED_ACCESS, ShaderModuleType::None);

                            BlitUniforms uniforms;
                            uniforms.srcDimensions = { srcExtent.x, srcExtent.y };
                            uniforms.dstDimensions = { dstExtent.x, dstExtent.y };
                            uniforms.srcMipLevel = 0;

                            RI.cbufferAllocator->Write(&uniforms);

                            GpuBuffer* blitCBuffer = nullptr;
                            size_t blitCBufferOffset = 0;
                            size_t blitCBufferSize = 0;
                            RI.cbufferAllocator->Commit(blitCBuffer, blitCBufferOffset, blitCBufferSize);

                            state.shaderUniforms[0] = ShaderUniform("InputTexture"_sh, inputView.Get());
                            state.dirtyUniforms |= 1u << 0;

                            state.shaderUniforms[1] = ShaderUniform("OutputTexture"_sh, outputView.Get());
                            state.dirtyUniforms |= 1u << 1;

                            state.shaderUniforms[2] = ShaderUniform("Constants"_sh, blitCBuffer);
                            state.shaderUniformBufferOffsets[2] = uint32(blitCBufferOffset);
                            state.shaderUniformBufferStrides[2] = uint32(blitCBufferSize);
                            state.dirtyUniforms |= 1u << 2;
                            state.dirtyBufferOffsets |= 1u << 2;

                            state.shaderUniforms[3] = ShaderUniform("SamplerLinear"_sh, linearSampler);
                            state.dirtyUniforms |= 1u << 3;

                            RI.CommitPipelineState(PSO_Compute, commandBuffer);

                            ComputePipeline* pipeline = state.boundComputePipeline;
                            if (!pipeline)
                            {
                                HYP_LOG(RenderingBackend, Error, "Blit: Failed to get compute pipeline. "
                                                                 "Shader 'GenerateMipmap' may not be compiled or available.");
                                continue;
                            }

                            pipeline->Dispatch(commandBuffer, { (dstExtent.x + 7) / 8, (dstExtent.y + 7) / 8, 1 });

#ifdef HYP_DX12
                            tempImage->GetGpuImage()->InsertUAVBarrier(commandBuffer);
#endif
                            tempImage->GetGpuImage()->InsertBarrier(commandBuffer, RS_COPY_SRC, ShaderModuleType::None);

                            dst->GetGpuImage()->InsertBarrier(commandBuffer, dstViewSubResource, RS_COPY_DST, ShaderModuleType::None);

                            dst->GetGpuImage()->CopyFrom(commandBuffer, tempImage->GetGpuImage().Get(),
                                                         Vec3u::Zero(), Vec3u::Zero(), dstExtent,
                                                         ImageSubResource { 0, 1, 0, 1 }, dstViewSubResource);

                            dst->GetGpuImage()->InsertBarrier(commandBuffer, dstViewSubResource, RS_SHADER_RESOURCE, ShaderModuleType::None);
                        }
                    }
                }
#endif
            }
            break;
            case CommandType::CopyImage:
            {
                auto* cmd = static_cast<CopyImage*>(cmdDataPtr);
                cmd->dstImage->CopyFrom(commandBuffer, cmd->srcImage, cmd->srcOffset, cmd->dstOffset, cmd->extent, cmd->srcSubResource, cmd->dstSubResource);
            }
            break;
            case CommandType::FillImage:
            {
                auto* cmd = static_cast<FillImage*>(cmdDataPtr);
                cmd->m_image->Fill(commandBuffer, cmd->m_value, cmd->m_subResource, cmd->m_offset, cmd->m_extent);
                static_assert(std::is_trivially_destructible_v<FillImage>);
            }
            break;
            case CommandType::CopyImageToBuffer:
            {
                auto* cmd = static_cast<CopyImageToBuffer*>(cmdDataPtr);
                cmd->m_image->CopyToBuffer(commandBuffer, cmd->m_buffer, cmd->m_subResource);
            }
            break;
            case CommandType::CopyBufferToImage:
            {
                auto* cmd = static_cast<CopyBufferToImage*>(cmdDataPtr);
                cmd->m_dstImage->CopyFromBuffer(commandBuffer, cmd->m_srcBuffer, cmd->m_srcBufferOffset, cmd->m_dstMipIndex, cmd->m_dstArrayLayer);
            }
            break;
            case CommandType::CopyBuffer:
            {
                auto* cmd = static_cast<CopyBuffer*>(cmdDataPtr);
                cmd->m_dstBuffer->CopyFrom(commandBuffer, cmd->m_srcBuffer, cmd->m_srcOffset, cmd->m_dstOffset, cmd->m_count);
                static_assert(std::is_trivially_destructible_v<CopyBuffer>);
            }
            break;
            case CommandType::GenerateMipmaps:
            {
                auto* cmd = static_cast<GenerateMipmaps*>(cmdDataPtr);

                Texture* inTexture = cmd->inTexture;

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

                    break;
                }

                if (numMips < 2)
                {
                    break;
                }

                RenderInterface::State& state = RI.state;
                state.attributes.SetShaderName(NAME("GenerateMipmap"));
                state.attributes.SetShaderProperties(ShaderPropertySet {});

                Sampler* linearSampler = RI.samplerCache->GetOrCreate(SamplerDesc { TFM_LINEAR, TFM_LINEAR, TWM_REPEAT });

                if (!linearSampler)
                {
                    HYP_LOG(RenderingBackend, Error, "GenerateMipmaps: Failed to get linear sampler");
                    break;
                }

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

                    Handle<Texture> tempImage = RI.scratchImageAllocator->AcquireScratchImage(TextureType::Texture2D, desc.format, desc.extent);

                    if (!tempImage.IsValid())
                    {
                        HYP_LOG(RenderingBackend, Error, "GenerateMipmaps: Failed to acquire scratch image");
                        continue;
                    }

                    inTexture->GetGpuImage()->InsertBarrier(
                        commandBuffer,
                        ImageSubResource { .baseMipLevel = 0, .numLevels = 1, .baseArrayLayer = layer, .numLayers = 1 },
                        RS_COPY_SRC, ShaderModuleType::None);

                    tempImage->GetGpuImage()->InsertBarrier(commandBuffer, RS_COPY_DST, ShaderModuleType::None);

                    tempImage->GetGpuImage()->CopyFrom(
                        commandBuffer, inTexture->GetGpuImage(),
                        Vec3u::Zero(), Vec3u::Zero(), desc.extent,
                        ImageSubResource { .baseMipLevel = 0, .numLevels = 1, .baseArrayLayer = layer, .numLayers = 1 },
                        ImageSubResource { .baseMipLevel = 0, .numLevels = 1, .baseArrayLayer = 0, .numLayers = 1 });

                    struct MipGenUniforms
                    {
                        Vec2u srcDimensions;
                        Vec2u dstDimensions;
                        uint32 srcMipLevel;
                    };

                    for (uint8 mip = 1; mip < numMips; mip++)
                    {
                        const uint8 srcMip = mip - 1;

                        const GpuImageViewRef& inputView = RI.textureViewCache->GetOrCreate(tempImage, srcMip, 1, 0, 1);
                        inputViews.PushBack(inputView);

                        const GpuImageViewRef& outputView = RI.textureViewCache->GetOrCreate(tempImage, mip, 1, 0, 1);
                        outputViews.PushBack(outputView);

                        const Vec3u srcExtent = desc.GetMipExtent(srcMip);
                        const Vec3u dstExtent = desc.GetMipExtent(mip);

                        MipGenUniforms uniforms;
                        uniforms.srcDimensions = { srcExtent.x, srcExtent.y };
                        uniforms.dstDimensions = { dstExtent.x, dstExtent.y };
                        uniforms.srcMipLevel = srcMip;

                        RI.cbufferAllocator->Write(&uniforms);

                        GpuBuffer* mipCBuffer = nullptr;
                        size_t mipCBufferOffset = 0;
                        size_t mipCBufferSize = 0;
                        RI.cbufferAllocator->Commit(mipCBuffer, mipCBufferOffset, mipCBufferSize);

                        cbuffers.PushBack(mipCBuffer);
                        cbufferOffsets.PushBack(mipCBufferOffset);
                        cbufferSizes.PushBack(mipCBufferSize);
                    }

                    for (uint8 mip = 1; mip < numMips; mip++)
                    {
                        const uint8 srcMip = mip - 1;
                        const Vec3u dstExtent = desc.GetMipExtent(mip);

                        tempImage->GetGpuImage()->InsertBarrier(
                            commandBuffer,
                            ImageSubResource { .baseMipLevel = srcMip, .numLevels = 1, .baseArrayLayer = 0, .numLayers = 1 },
                            RS_SHADER_RESOURCE, ShaderModuleType::None);

                        tempImage->GetGpuImage()->InsertBarrier(
                            commandBuffer,
                            ImageSubResource { .baseMipLevel = mip, .numLevels = 1, .baseArrayLayer = 0, .numLayers = 1 },
                            RS_UNORDERED_ACCESS, ShaderModuleType::None);

                        state.shaderUniforms[0] = ShaderUniform("InputTexture"_sh, inputViews[srcMip]);
                        state.dirtyUniforms |= 1u << 0;

                        state.shaderUniforms[1] = ShaderUniform("OutputTexture"_sh, outputViews[srcMip]);
                        state.dirtyUniforms |= 1u << 1;

                        state.shaderUniforms[2] = ShaderUniform("Constants"_sh, cbuffers[srcMip]);
                        state.shaderUniformBufferOffsets[2] = uint32(cbufferOffsets[srcMip]);
                        state.shaderUniformBufferStrides[2] = uint32(cbufferSizes[srcMip]);
                        state.dirtyUniforms |= 1u << 2;
                        state.dirtyBufferOffsets |= 1u << 2;

                        state.shaderUniforms[3] = ShaderUniform("SamplerLinear"_sh, linearSampler);
                        state.dirtyUniforms |= 1u << 3;

                        RI.CommitPipelineState(PSO_Compute, commandBuffer);

                        ComputePipeline* pipeline = state.boundComputePipeline;
                        if (!pipeline)
                        {
                            HYP_LOG(RenderingBackend, Error, "GenerateMipmaps: Failed to get compute pipeline for mip {}. "
                                                             "Shader 'GenerateMipmap' may not be compiled or available.",
                                    mip);
                            continue;
                        }

                        pipeline->Dispatch(commandBuffer, { (dstExtent.x + 7) / 8, (dstExtent.y + 7) / 8, 1 });

#ifdef HYP_DX12
                        tempImage->GetGpuImage()->InsertUAVBarrier(commandBuffer);
#endif
                    }

#ifdef HYP_DX12
                    tempImage->GetGpuImage()->InsertUAVBarrier(commandBuffer);
#endif

                    for (uint8 mip = 1; mip < numMips; mip++)
                    {
                        const Vec3u mipExtent = desc.GetMipExtent(mip);

                        tempImage->GetGpuImage()->InsertBarrier(
                            commandBuffer,
                            ImageSubResource { .baseMipLevel = mip, .numLevels = 1, .baseArrayLayer = 0, .numLayers = 1 },
                            RS_COPY_SRC, ShaderModuleType::None);

                        inTexture->GetGpuImage()->InsertBarrier(
                            commandBuffer,
                            ImageSubResource { .baseMipLevel = mip, .numLevels = 1, .baseArrayLayer = layer, .numLayers = 1 },
                            RS_COPY_DST, ShaderModuleType::None);

                        inTexture->GetGpuImage()->CopyFrom(
                            commandBuffer, tempImage->GetGpuImage().Get(),
                            Vec3u::Zero(), Vec3u::Zero(), mipExtent,
                            ImageSubResource { .baseMipLevel = mip, .numLevels = 1, .baseArrayLayer = 0, .numLayers = 1 },
                            ImageSubResource { .baseMipLevel = mip, .numLevels = 1, .baseArrayLayer = layer, .numLayers = 1 });

                        inTexture->GetGpuImage()->InsertBarrier(
                            commandBuffer,
                            ImageSubResource { .baseMipLevel = mip, .numLevels = 1, .baseArrayLayer = layer, .numLayers = 1 },
                            RS_SHADER_RESOURCE, ShaderModuleType::None);
                    }
                }
#endif

                inTexture->GetGpuImage()->InsertBarrier(commandBuffer, RS_SHADER_RESOURCE, ShaderModuleType::None);
            }
            break;
            case CommandType::DispatchCompute:
            {
                auto* cmd = static_cast<DispatchCompute*>(cmdDataPtr);

                ComputePipeline* pipeline = cmd->m_pipeline;

                if (pipeline == nullptr)
                {
                    RI.CommitPipelineState(PSO_Compute, commandBuffer);
                    pipeline = RI.state.boundComputePipeline;
                    AssertDebug(pipeline != nullptr);
                }

                pipeline->Dispatch(commandBuffer, cmd->m_workgroupCount);

                static_assert(std::is_trivially_destructible_v<DispatchCompute>);
            }
            break;
            case CommandType::TraceRays:
            {
                auto* cmd = static_cast<TraceRays*>(cmdDataPtr);

                RayTracingPipeline* pipeline = cmd->m_pipeline;

                if (pipeline == nullptr)
                {
                    RI.CommitPipelineState(PSO_RayTracing, commandBuffer);
                    pipeline = RI.state.boundRayTracingPipeline;
                    AssertDebug(pipeline != nullptr);
                }

                pipeline->TraceRays(commandBuffer, cmd->m_workgroupCount);

                static_assert(std::is_trivially_destructible_v<TraceRays>);
            }
            break;
            case CommandType::SetStencilState:
            {
                auto* cmd = static_cast<SetStencilState*>(cmdDataPtr);

                RenderInterface::State& state = RI.state;

                if (state.stencilReference != cmd->m_referenceValue
                    || state.stencilCompareMask != cmd->m_compareMask
                    || state.stencilWriteMask != cmd->m_writeMask)
                {
                    state.stencilReference = cmd->m_referenceValue;
                    state.stencilCompareMask = cmd->m_compareMask;
                    state.stencilWriteMask = cmd->m_writeMask;
                    state.boundGraphicsPipeline = nullptr;
                    state.dirtyUniforms |= (state.validUniforms | state.dirtyBufferOffsets);
                    state.validUniforms = 0;

                    Memory::Zero(state.prevBoundDescriptorSets, sizeof(state.prevBoundDescriptorSets));
                }

                static_assert(std::is_trivially_destructible_v<SetStencilState>);
            }
            break;
            case CommandType::SetCurrentShader:
            {
                auto* cmd = static_cast<SetCurrentShader*>(cmdDataPtr);

                RenderInterface::State& state = RI.state;

                ShaderDesc& shaderDesc = cmd->shaderDesc;

                MergeGlobalShaderProperties(shaderDesc.properties);

                state.attributes.SetShaderName(shaderDesc.name);
                state.attributes.SetShaderProperties(shaderDesc.properties);

                static_assert(std::is_trivially_destructible_v<SetCurrentShader>);
            }
            break;
            case CommandType::SetCurrentViewport:
            {
                auto* cmd = static_cast<SetCurrentViewport*>(cmdDataPtr);

                RI.state.viewport = cmd->viewport;

                static_assert(std::is_trivially_destructible_v<SetCurrentViewport>);
            }
            break;
            case CommandType::SetTopology:
            {
                auto* cmd = static_cast<SetTopology*>(cmdDataPtr);

                if (RI.state.attributes.GetMeshAttributes().topology == cmd->topology)
                {
                    break;
                }

                RI.state.attributes.GetMeshAttributes().topology = cmd->topology;

                static_assert(std::is_trivially_destructible_v<SetTopology>);
            }
            break;
            case CommandType::SetInputLayout:
            {
                auto* cmd = static_cast<SetInputLayout*>(cmdDataPtr);
                RenderInterface::State& state = RI.state;

                if (state.attributes.GetMeshAttributes().inputLayout == cmd->inputLayout)
                {
                    break;
                }

                state.attributes.GetMeshAttributes().inputLayout = cmd->inputLayout;

                static_assert(std::is_trivially_destructible_v<SetInputLayout>);
            }
            break;
            case CommandType::SetCurrentBlendFunction:
            {
                auto* cmd = static_cast<SetCurrentBlendFunction*>(cmdDataPtr);

                if (RI.state.attributes.GetMaterialAttributes().blendFunction == cmd->blendFunction)
                {
                    break;
                }

                RI.state.attributes.GetMaterialAttributes().blendFunction = cmd->blendFunction;

                static_assert(std::is_trivially_destructible_v<SetCurrentBlendFunction>);
            }
            break;
            case CommandType::SetDepthWrite:
            {
                auto* cmd = static_cast<SetDepthWrite*>(cmdDataPtr);

                if (cmd->depthWrite)
                {
                    if (RI.state.attributes.GetMaterialAttributes().flags & MAF_DEPTH_WRITE)
                    {
                        break;
                    }

                    RI.state.attributes.GetMaterialAttributes().flags |= MAF_DEPTH_WRITE;
                }
                else
                {
                    if (!(RI.state.attributes.GetMaterialAttributes().flags & MAF_DEPTH_WRITE))
                    {
                        break;
                    }

                    RI.state.attributes.GetMaterialAttributes().flags &= ~MAF_DEPTH_WRITE;
                }

                static_assert(std::is_trivially_destructible_v<SetDepthWrite>);
            }
            break;
            case CommandType::SetDepthTest:
            {
                auto* cmd = static_cast<SetDepthTest*>(cmdDataPtr);

                if (cmd->depthTest)
                {
                    if (RI.state.attributes.GetMaterialAttributes().flags & MAF_DEPTH_TEST)
                    {
                        break;
                    }

                    RI.state.attributes.GetMaterialAttributes().flags |= MAF_DEPTH_TEST;
                }
                else
                {
                    if (!(RI.state.attributes.GetMaterialAttributes().flags & MAF_DEPTH_TEST))
                    {
                        break;
                    }

                    RI.state.attributes.GetMaterialAttributes().flags &= ~MAF_DEPTH_TEST;
                }

                static_assert(std::is_trivially_destructible_v<SetDepthTest>);
            }
            break;
            case CommandType::SetDepthCompareOp:
            {
                auto* cmd = static_cast<SetDepthCompareOp*>(cmdDataPtr);

                if (RI.state.attributes.GetMaterialAttributes().depthCompareOp == cmd->compareOp)
                {
                    break;
                }

                RI.state.attributes.GetMaterialAttributes().depthCompareOp = cmd->compareOp;

                static_assert(std::is_trivially_destructible_v<SetDepthCompareOp>);
            }
            break;
            case CommandType::SetDepthBias:
            {
                auto* cmd = static_cast<SetDepthBias*>(cmdDataPtr);

                RenderInterface::State& state = RI.state;
                const bool enableDepthBias = cmd->depthBias != 0;

                if (enableDepthBias)
                {
                    if ((state.attributes.GetMaterialAttributes().flags & MAF_DEPTH_BIAS)
                        && state.attributes.GetMaterialAttributes().depthBias == cmd->depthBias
                        && MathUtil::ApproxEqual(state.attributes.GetMaterialAttributes().depthBiasSlope, cmd->depthBiasSlope))
                    {
                        break;
                    }

                    state.attributes.GetMaterialAttributes().flags |= MAF_DEPTH_BIAS;
                    state.attributes.GetMaterialAttributes().depthBias = cmd->depthBias;
                    state.attributes.GetMaterialAttributes().depthBiasSlope = cmd->depthBiasSlope;
                }
                else
                {
                    if (!(state.attributes.GetMaterialAttributes().flags & MAF_DEPTH_BIAS))
                    {
                        break;
                    }

                    state.attributes.GetMaterialAttributes().flags &= ~MAF_DEPTH_BIAS;
                }

                static_assert(std::is_trivially_destructible_v<SetDepthBias>);
            }
            break;
            case CommandType::SetDepthClamp:
            {
                auto* cmd = static_cast<SetDepthClamp*>(cmdDataPtr);

                if (cmd->depthClamp)
                {
                    if (RI.state.attributes.GetMaterialAttributes().flags & MAF_DEPTH_CLAMP)
                    {
                        break;
                    }

                    RI.state.attributes.GetMaterialAttributes().flags |= MAF_DEPTH_CLAMP;
                }
                else
                {
                    if (!(RI.state.attributes.GetMaterialAttributes().flags & MAF_DEPTH_CLAMP))
                    {
                        break;
                    }

                    RI.state.attributes.GetMaterialAttributes().flags &= ~MAF_DEPTH_CLAMP;
                }

                static_assert(std::is_trivially_destructible_v<SetDepthClamp>);
            }
            break;
            case CommandType::SetStencilTest:
            {
                auto* cmd = static_cast<SetStencilTest*>(cmdDataPtr);

                if (cmd->stencilTest)
                {
                    if (RI.state.attributes.GetMaterialAttributes().flags & MAF_STENCIL_TEST)
                    {
                        break;
                    }

                    RI.state.attributes.GetMaterialAttributes().flags |= MAF_STENCIL_TEST;
                }
                else
                {
                    if (!(RI.state.attributes.GetMaterialAttributes().flags & MAF_STENCIL_TEST))
                    {
                        break;
                    }

                    RI.state.attributes.GetMaterialAttributes().flags &= ~MAF_STENCIL_TEST;
                }

                static_assert(std::is_trivially_destructible_v<SetStencilTest>);
            }
            break;
            case CommandType::SetStencilFunction:
            {
                auto* cmd = static_cast<SetStencilFunction*>(cmdDataPtr);

                if (RI.state.attributes.GetMaterialAttributes().stencilFunction == cmd->stencilFunction)
                {
                    break;
                }

                RI.state.attributes.GetMaterialAttributes().stencilFunction = cmd->stencilFunction;

                static_assert(std::is_trivially_destructible_v<SetStencilFunction>);
            }
            break;
            case CommandType::SetFillMode:
            {
                auto* cmd = static_cast<SetFillMode*>(cmdDataPtr);

                if (RI.state.attributes.GetMaterialAttributes().fillMode == cmd->fillMode)
                {
                    break;
                }

                RI.state.attributes.GetMaterialAttributes().fillMode = cmd->fillMode;

                static_assert(std::is_trivially_destructible_v<SetFillMode>);
            }
            break;
            case CommandType::SetFaceCullMode:
            {
                auto* cmd = static_cast<SetFaceCullMode*>(cmdDataPtr);

                if (RI.state.attributes.GetMaterialAttributes().cullFaces == cmd->faceCullMode)
                {
                    break;
                }

                RI.state.attributes.GetMaterialAttributes().cullFaces = cmd->faceCullMode;

                static_assert(std::is_trivially_destructible_v<SetFaceCullMode>);
            }
            break;
            case CommandType::SetShaderUniform:
            {
                auto* cmd = static_cast<SetShaderUniform*>(cmdDataPtr);

                RenderInterface::State& state = RI.state;

                AssertDebug(cmd->uniformIndex < state.MaxShaderUniforms,
                            "SetShaderUniform: uniformIndex {} is out of bounds (MaxShaderUniforms = {})",
                            cmd->uniformIndex, state.MaxShaderUniforms);

                ShaderUniform& uniform = state.shaderUniforms[cmd->uniformIndex];

                if (uniform != cmd->uniform || !(state.validUniforms & (1u << cmd->uniformIndex)))
                {
                    uniform = cmd->uniform;
                    state.dirtyUniforms |= (1u << cmd->uniformIndex);
                }

                if (cmd->uniform.type == ShaderUniform::UT_Buffer)
                {
                    if (state.shaderUniformBufferStrides[cmd->uniformIndex] != cmd->shaderDataOffset.stride)
                    {
                        state.dirtyUniforms |= (1u << cmd->uniformIndex);
                    }

                    state.shaderUniformBufferOffsets[cmd->uniformIndex] = cmd->shaderDataOffset.offset;
                    state.shaderUniformBufferStrides[cmd->uniformIndex] = cmd->shaderDataOffset.stride;

                    state.dirtyBufferOffsets |= (1u << cmd->uniformIndex);
                }
                else
                {
                    state.dirtyBufferOffsets &= ~(1u << cmd->uniformIndex);
                }

                static_assert(std::is_trivially_destructible_v<SetShaderUniform>);
            }
            break;
            case CommandType::SetShaderUniforms:
            {
                auto* cmd = static_cast<SetShaderUniforms*>(cmdDataPtr);

                RenderInterface::State& state = RI.state;

                const ShaderUniforms& srcUniforms = cmd->shaderUniforms;
                AssertDebug(cmd->startIndex + srcUniforms.count <= state.MaxShaderUniforms);

                for (uint32 i = cmd->startIndex; i < cmd->startIndex + srcUniforms.count; i++)
                {
                    const ShaderUniform& srcUniform = srcUniforms.uniforms[i - cmd->startIndex];
                    ShaderUniform& dstUniform = state.shaderUniforms[i];

                    if (dstUniform != srcUniform || !(state.validUniforms & (1u << i)))
                    {
                        dstUniform = srcUniform;
                        state.dirtyUniforms |= (1u << i);
                    }

                    if (srcUniform.type == ShaderUniform::UT_Buffer)
                    {
                        const size_t bufferStride = srcUniforms.bufferStrides[i - cmd->startIndex];
                        const size_t bufferOffset = srcUniforms.bufferOffsets[i - cmd->startIndex];

                        if (state.shaderUniformBufferStrides[i] != bufferStride)
                        {
                            state.dirtyUniforms |= (1u << i);
                        }

                        state.shaderUniformBufferOffsets[i] = bufferOffset;
                        state.shaderUniformBufferStrides[i] = bufferStride;
                        state.dirtyBufferOffsets |= (1u << i);
                    }
                    else
                    {
                        state.dirtyBufferOffsets &= ~(1u << i);
                    }
                }

                static_assert(std::is_trivially_destructible_v<SetShaderUniforms>);
            }
            break;
            case CommandType::RecordGpuTimestamp:
            {
                auto* cmd = static_cast<RecordGpuTimestamp*>(cmdDataPtr);

                if (cmd->m_isStart)
                {
                    RI.RecordStartTimestamp(commandBuffer, cmd->m_timer);
                }
                else
                {
                    RI.RecordStopTimestamp(commandBuffer, cmd->m_timer);
                }

                static_assert(std::is_trivially_destructible_v<RecordGpuTimestamp>);
            }
            break;
            case CommandType::CommitDrawState:
                RI.CommitDrawState(commandBuffer);
                static_assert(std::is_trivially_destructible_v<CommitDrawState>);
                break;
            default:
                HYP_FAIL("Unexpected command type {}", header.GetCommandType());
            }
        }
        else // IsCustom
        {
            // Read next header which holds fnptr directly

            i++;

            InvokeCmdFnPtr fnPtr = reinterpret_cast<InvokeCmdFnPtr>(m_headersPtr[i].address);
            fnPtr(cmdDataPtr, commandBuffer);

            continue;
        }
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

#pragma region GenerateMipmaps

GenerateMipmaps::GenerateMipmaps(Texture* inTexture)
    : inTexture(inTexture)
{
    AssertDebug(inTexture != nullptr && inTexture->IsCreated());
}

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

#pragma endregion BindDescriptorSet

#pragma region InsertBarrier

#if defined(HYP_VULKAN) && defined(HYP_DEBUG_MODE)
void InsertBarrier::CheckNotInRenderPass(CommandBuffer* commandBuffer) const
{
    Assert(!commandBuffer->IsInRenderPass(), "InsertBarrier() used while renderpass active!");
}
#endif

#pragma endregion InsertBarrier

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

#pragma endregion SetShaderUniform

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

#pragma endregion FillImage

} // namespace Hyperion
