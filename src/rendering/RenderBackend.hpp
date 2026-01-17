/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/utilities/Span.hpp>

#include <rendering/Shared.hpp>
#include <rendering/RenderResult.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/RenderMemory.hpp>

#include <core/functional/Delegate.hpp>

#include <core/memory/ByteBuffer.hpp>
#include <core/memory/RefCountedPtr.hpp>

namespace Hyperion {

class RenderableAttributeSet;
struct CompiledShader;
class Material;

class IRenderBackend;
class FrameBase;
class SwapchainBase;
class AsyncComputeBase;
struct TextureDesc;
class SingleTimeCommands;
class Texture;
class ApplicationWindow;

class DescriptorSetLayout;
struct DescriptorTableDeclaration;

enum class GpuBufferType : uint8;
enum RenderTargetType : uint8;

template <class T>
struct Handle;

struct QueryImageCapabilitiesResult
{
    bool supports2d = false;
    bool supports3d = false;
    bool supportsCubemap = false;
    bool supportsArray = false;
    bool supportsMipmaps = false;
    bool supportsStorage = false;
};

class IDescriptorSetManager
{
public:
    virtual ~IDescriptorSetManager() = default;
};

class IRenderBackend
{
public:
    virtual ~IRenderBackend() = default;

    virtual RendererResult Initialize() = 0;
    virtual RendererResult Destroy() = 0;

    virtual const IRenderConfig& GetRenderConfig() const = 0;

    virtual AsyncComputeBase* GetAsyncCompute() const = 0;

    virtual Frame* GetCurrentFrame() const = 0;

    virtual Frame* PrepareNextFrame() = 0;

    virtual SwapchainRef CreateSwapchain(ApplicationWindow* window) = 0;

    virtual void PrepareSwapchain(Swapchain* swapchain) = 0;
    virtual void SubmitCommandBuffers(Swapchain* swapchain) = 0;
    virtual void PresentToSwapchain(Swapchain* swapchain) = 0;

    virtual CommandBuffer* GetCurrentCommandBuffer() const = 0;

    virtual DescriptorSetRef MakeDescriptorSet(const DescriptorSetLayout& layout) = 0;

    virtual DescriptorTableRef MakeDescriptorTable(const DescriptorTableDeclaration* decl) = 0;

    virtual GraphicsPipelineRef MakeGraphicsPipeline(
        const ShaderRef& shader,
        const RenderTargetDesc& renderTargetDesc,
        const RenderableAttributeSet& attributes) = 0;

    virtual ComputePipelineRef MakeComputePipeline(
        const ShaderRef& shader,
        const DescriptorTableRef& descriptorTable) = 0;

    virtual RaytracingPipelineRef MakeRaytracingPipeline(
        const ShaderRef& shader,
        const DescriptorTableRef& descriptorTable) = 0;

    virtual GpuBufferRef MakeGpuBuffer(GpuBufferType bufferType, SizeType size, SizeType alignment = 0) = 0;

    virtual GpuImageRef MakeImage(const TextureDesc& textureDesc) = 0;

    virtual GpuImageViewRef MakeImageView(const GpuImageRef& image) = 0;
    virtual GpuImageViewRef MakeImageView(const GpuImageRef& image, uint32 mipIndex, uint32 numMips, uint32 layerIndex, uint32 numLayers) = 0;

    virtual SamplerRef MakeSampler(TextureFilterMode filterModeMin, TextureFilterMode filterModeMag, TextureWrapMode wrapMode) = 0;

    virtual FramebufferRef MakeFramebuffer(const RenderTargetDesc& renderTargetDesc) = 0;

    virtual FrameRef MakeFrame(uint32 frameIndex) = 0;

    virtual ShaderRef MakeShader(const RC<CompiledShader>& compiledShader) = 0;

    virtual GpuBlasRef MakeGpuBlas(
        const GpuBufferRef& packedVerticesBuffer,
        const GpuBufferRef& packedIndicesBuffer,
        uint32 numVertices,
        uint32 numIndices,
        const Handle<Material>& material,
        const Mat4f& transform) = 0;
    virtual GpuTlasRef MakeTLAS() = 0;

    virtual const GpuImageViewRef& GetTextureImageView(const Handle<Texture>& texture, uint32 mipIndex = 0, uint32 numMips = ~0u, uint32 layerIndex = 0, uint32 numLayers = ~0u) = 0;

    virtual void PopulateIndirectDrawCommandsBuffer(const GpuBufferRef& vertexBuffer, const GpuBufferRef& indexBuffer, uint32 instanceOffset, TByteBuffer<RenderAllocator>& outByteBuffer) = 0;

    virtual TextureFormat GetDefaultFormat(DefaultImageFormat type) const = 0;

    virtual bool IsSupportedFormat(TextureFormat format, ImageSupport supportType) const = 0;
    virtual TextureFormat FindSupportedFormat(Span<TextureFormat> possibleFormats, ImageSupport supportType) const = 0;

    virtual QueryImageCapabilitiesResult QueryImageCapabilities(const TextureDesc& textureDesc) const = 0;

    virtual UniquePtr<SingleTimeCommands> GetSingleTimeCommands() = 0;

    virtual void ReleaseTransientMemory() = 0;

    virtual void NextFrame() = 0;
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <rendering/vulkan/VulkanRenderBackend.hpp>
#elif HYP_DX12
#include <rendering/dx12/DX12RenderBackend.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif