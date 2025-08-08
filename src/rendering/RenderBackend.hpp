/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/Shared.hpp>
#include <rendering/RenderResult.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderConfig.hpp>

#include <core/functional/Proc.hpp>
#include <core/functional/Delegate.hpp>

#include <core/memory/ByteBuffer.hpp>

#include <core/utilities/Span.hpp>

#include <core/object/Handle.hpp>

#include <core/Defines.hpp>

namespace hyperion {

namespace sys {
class AppContextBase;
} // namespace sys

using sys::AppContextBase;

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

class DescriptorSetLayout;
struct DescriptorTableDeclaration;

enum class GpuBufferType : uint8;
enum class RenderPassStage : uint8;

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

    virtual RendererResult Initialize(AppContextBase& appContext) = 0;
    virtual RendererResult Destroy() = 0;

    virtual const IRenderConfig& GetRenderConfig() const = 0;

    // Will be moved to ApplicationWindow
    virtual SwapchainBase* GetSwapchain() const = 0;

    virtual AsyncComputeBase* GetAsyncCompute() const = 0;

    virtual FrameBase* GetCurrentFrame() const = 0;
    virtual FrameBase* PrepareNextFrame() = 0;
    virtual void PresentFrame(FrameBase* frame) = 0;

    virtual DescriptorSetRef MakeDescriptorSet(const DescriptorSetLayout& layout) = 0;

    virtual DescriptorTableRef MakeDescriptorTable(const DescriptorTableDeclaration* decl) = 0;

    virtual GraphicsPipelineRef MakeGraphicsPipeline(
        const ShaderRef& shader,
        const DescriptorTableRef& descriptorTable,
        Span<const FramebufferRef> framebuffers,
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
    virtual GpuImageViewRef MakeImageView(const GpuImageRef& image, uint32 mipIndex, uint32 numMips, uint32 faceIndex, uint32 numFaces) = 0;

    virtual SamplerRef MakeSampler(TextureFilterMode filterModeMin, TextureFilterMode filterModeMag, TextureWrapMode wrapMode) = 0;

    virtual FramebufferRef MakeFramebuffer(Vec2u extent, uint32 numViews = 1) = 0;
    virtual FramebufferRef MakeFramebuffer(Vec2u extent, RenderPassStage stage, uint32 numViews = 1) = 0;

    virtual FrameRef MakeFrame(uint32 frameIndex) = 0;

    virtual ShaderRef MakeShader(const RC<CompiledShader>& compiledShader) = 0;

    virtual BLASRef MakeBLAS(
        const GpuBufferRef& packedVerticesBuffer,
        const GpuBufferRef& packedIndicesBuffer,
        uint32 numVertices,
        uint32 numIndices,
        const Handle<Material>& material,
        const Matrix4& transform) = 0;
    virtual TLASRef MakeTLAS() = 0;

    virtual const GpuImageViewRef& GetTextureImageView(const Handle<Texture>& texture, uint32 mipIndex = 0, uint32 numMips = ~0u, uint32 faceIndex = 0, uint32 numFaces = ~0u) = 0;

    virtual void PopulateIndirectDrawCommandsBuffer(const GpuBufferRef& vertexBuffer, const GpuBufferRef& indexBuffer, uint32 instanceOffset, ByteBuffer& outByteBuffer) = 0;

    virtual TextureFormat GetDefaultFormat(DefaultImageFormat type) const = 0;

    virtual bool IsSupportedFormat(TextureFormat format, ImageSupport supportType) const = 0;
    virtual TextureFormat FindSupportedFormat(Span<TextureFormat> possibleFormats, ImageSupport supportType) const = 0;

    virtual QueryImageCapabilitiesResult QueryImageCapabilities(const TextureDesc& textureDesc) const = 0;

    virtual UniquePtr<SingleTimeCommands> GetSingleTimeCommands() = 0;

    virtual Delegate<void, SwapchainBase*>& GetOnSwapchainRecreatedDelegate() = 0;
};

} // namespace hyperion
