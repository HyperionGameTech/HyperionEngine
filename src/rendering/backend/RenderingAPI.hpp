/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERING_API_HPP
#define HYPERION_BACKEND_RENDERING_API_HPP

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RenderConfig.hpp>

#include <core/functional/Proc.hpp>
#include <core/functional/Delegate.hpp>

#include <core/Handle.hpp>

#include <core/Defines.hpp>

namespace hyperion {

namespace sys {
class AppContext;
} // namespace sys

using sys::AppContext;

class RenderableAttributeSet;
struct CompiledShader;
class Material;

namespace renderer {

class IRenderingAPI;
class FrameBase;
class SwapchainBase;
class AsyncComputeBase;
struct TextureDesc;

class DescriptorSetLayout;
class DescriptorTableDeclaration;

enum class GPUBufferType : uint8;
enum class RenderPassStage : uint8;

struct QueryImageCapabilitiesResult
{
    bool    supports_2d = false;
    bool    supports_3d = false;
    bool    supports_cubemap = false;
    bool    supports_array = false;
    bool    supports_mipmaps = false;
    bool    supports_storage = false;
};

class IDescriptorSetManager
{
public:
    virtual ~IDescriptorSetManager() = default;
};

class IRenderingAPI
{
public:
    virtual ~IRenderingAPI() = default;

    virtual RendererResult Initialize(AppContext &app_context) = 0;
    virtual RendererResult Destroy() = 0;

    virtual const IRenderConfig &GetRenderConfig() const = 0;

    // Will be moved to ApplicationWindow
    virtual SwapchainBase *GetSwapchain() const = 0;

    virtual AsyncComputeBase *GetAsyncCompute() const = 0;

    virtual FrameBase *GetCurrentFrame() const = 0;
    virtual FrameBase *PrepareNextFrame() = 0;
    virtual void PresentFrame(FrameBase *frame) = 0;

    virtual DescriptorSetRef MakeDescriptorSet(const DescriptorSetLayout &layout) = 0;

    virtual DescriptorTableRef MakeDescriptorTable(const DescriptorTableDeclaration &decl) = 0;

    virtual GraphicsPipelineRef MakeGraphicsPipeline(
        const ShaderRef &shader,
        const DescriptorTableRef &descriptor_table,
        const Array<FramebufferRef> &framebuffers,
        const RenderableAttributeSet &attributes
    ) = 0;

    virtual ComputePipelineRef MakeComputePipeline(
        const ShaderRef &shader,
        const DescriptorTableRef &descriptor_table
    ) = 0;

    virtual RaytracingPipelineRef MakeRaytracingPipeline(
        const ShaderRef &shader,
        const DescriptorTableRef &descriptor_table
    ) = 0;

    virtual GPUBufferRef MakeGPUBuffer(GPUBufferType buffer_type, SizeType size, SizeType alignment = 0) = 0;

    virtual ImageRef MakeImage(const TextureDesc &texture_desc) = 0;

    virtual ImageViewRef MakeImageView(const ImageRef &image) = 0;
    virtual ImageViewRef MakeImageView(const ImageRef &image, uint32 mip_index, uint32 num_mips, uint32 face_index, uint32 num_faces) = 0;

    virtual SamplerRef MakeSampler(FilterMode filter_mode_min, FilterMode filter_mode_mag, WrapMode wrap_mode) = 0;

    virtual FramebufferRef MakeFramebuffer(Vec2u extent, uint32 num_views = 1) = 0;
    virtual FramebufferRef MakeFramebuffer(Vec2u extent, RenderPassStage stage, uint32 num_views = 1) = 0;

    virtual FrameRef MakeFrame(uint32 frame_index) = 0;

    virtual ShaderRef MakeShader(const RC<CompiledShader> &compiled_shader) = 0;
    
    virtual BLASRef MakeBLAS(
        const GPUBufferRef &packed_vertices_buffer,
        const GPUBufferRef &packed_indices_buffer,
        const Handle<Material> &material,
        const Matrix4 &transform
    ) = 0;
    virtual TLASRef MakeTLAS() = 0;

    virtual InternalFormat GetDefaultFormat(DefaultImageFormatType type) const = 0;

    virtual bool IsSupportedFormat(InternalFormat format, ImageSupportType support_type) const = 0;
    virtual InternalFormat FindSupportedFormat(Span<InternalFormat> possible_formats, ImageSupportType support_type) const = 0;

    virtual QueryImageCapabilitiesResult QueryImageCapabilities(const TextureDesc &texture_desc) const = 0;

    virtual Delegate<void, SwapchainBase *> &GetOnSwapchainRecreatedDelegate() = 0;
};

} // namespace renderer

using renderer::IRenderingAPI;

} // namespace hyperion

#endif