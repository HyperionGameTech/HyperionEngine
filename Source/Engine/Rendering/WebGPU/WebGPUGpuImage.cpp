/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <WebGPUPch.hpp>

#include <Rendering/WebGPU/WebGPUGpuImage.hpp>
#include <Rendering/WebGPU/WebGPUGpuImageView.hpp>
#include <Rendering/WebGPU/WebGPURenderInterface.hpp>
#include <Rendering/WebGPU/WebGPUHelpers.hpp>

#include <Core/Threading/Mutex.hpp>

#include <WebGPUGpuImage.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern WebGPURenderInterface RI;

static constexpr uint32 CopyRowPitchAlignment = 256;
static constexpr uint32 CopyPlacementAlignment = 512;

#pragma region Blit resources

// WebGPU has no blit command, so blits and mip generation draw a fullscreen triangle
static const char* const g_blitShaderSource = R"(
struct BlitParams
{
    uvOffset : vec2f,
    uvScale : vec2f,
}

@group(0) @binding(0) var sourceTexture : texture_2d<f32>;
@group(0) @binding(1) var sourceSampler : sampler;
@group(0) @binding(2) var<uniform> params : BlitParams;

struct VertexOutput
{
    @builtin(position) position : vec4f,
    @location(0) uv : vec2f,
}

@vertex
fn VSMain(@builtin(vertex_index) vertexIndex : u32) -> VertexOutput
{
    var output : VertexOutput;

    let corner = vec2f(f32((vertexIndex << 1u) & 2u), f32(vertexIndex & 2u));

    output.position = vec4f(corner * 2.0 - 1.0, 0.0, 1.0);
    output.uv = params.uvOffset + vec2f(corner.x, 1.0 - corner.y) * params.uvScale;

    return output;
}

@fragment
fn PSMain(input : VertexOutput) -> @location(0) vec4f
{
    return textureSampleLevel(sourceTexture, sourceSampler, input.uv, 0.0);
}
)";

struct BlitResources
{
    Mutex mutex;

    WGPUShaderModule shaderModule = nullptr;
    WGPUSampler sampler = nullptr;
    WGPUBindGroupLayout bindGroupLayout = nullptr;
    WGPUPipelineLayout pipelineLayout = nullptr;

    Array<Pair<WGPUTextureFormat, WGPURenderPipeline>, WebGPUAllocator> pipelines;

    void Initialize()
    {
        if (shaderModule != nullptr)
        {
            return;
        }

        WGPUShaderSourceWGSL shaderSource = WGPU_SHADER_SOURCE_WGSL_INIT;
        shaderSource.code = ToWGPUStringView(g_blitShaderSource);

        WGPUShaderModuleDescriptor moduleDescriptor = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
        moduleDescriptor.nextInChain = &shaderSource.chain;

        shaderModule = wgpuDeviceCreateShaderModule(RI.GetDevice(), &moduleDescriptor);

        WGPUSamplerDescriptor samplerDescriptor = WGPU_SAMPLER_DESCRIPTOR_INIT;
        samplerDescriptor.magFilter = WGPUFilterMode_Linear;
        samplerDescriptor.minFilter = WGPUFilterMode_Linear;

        sampler = wgpuDeviceCreateSampler(RI.GetDevice(), &samplerDescriptor);

        WGPUBindGroupLayoutEntry entries[3] = {
            WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT,
            WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT,
            WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT
        };

        entries[0].binding = 0;
        entries[0].visibility = WGPUShaderStage_Fragment;
        entries[0].texture.sampleType = WGPUTextureSampleType_Float;
        entries[0].texture.viewDimension = WGPUTextureViewDimension_2D;

        entries[1].binding = 1;
        entries[1].visibility = WGPUShaderStage_Fragment;
        entries[1].sampler.type = WGPUSamplerBindingType_Filtering;

        entries[2].binding = 2;
        entries[2].visibility = WGPUShaderStage_Vertex;
        entries[2].buffer.type = WGPUBufferBindingType_Uniform;

        WGPUBindGroupLayoutDescriptor layoutDescriptor = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
        layoutDescriptor.entryCount = 3;
        layoutDescriptor.entries = entries;

        bindGroupLayout = wgpuDeviceCreateBindGroupLayout(RI.GetDevice(), &layoutDescriptor);

        WGPUPipelineLayoutDescriptor pipelineLayoutDescriptor = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
        pipelineLayoutDescriptor.bindGroupLayoutCount = 1;
        pipelineLayoutDescriptor.bindGroupLayouts = &bindGroupLayout;

        pipelineLayout = wgpuDeviceCreatePipelineLayout(RI.GetDevice(), &pipelineLayoutDescriptor);
    }

    WGPURenderPipeline GetOrCreatePipeline(WGPUTextureFormat format)
    {
        for (const Pair<WGPUTextureFormat, WGPURenderPipeline>& entry : pipelines)
        {
            if (entry.first == format)
            {
                return entry.second;
            }
        }

        WGPUColorTargetState target = WGPU_COLOR_TARGET_STATE_INIT;
        target.format = format;

        WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
        fragment.module = shaderModule;
        fragment.entryPoint = ToWGPUStringView("PSMain");
        fragment.targetCount = 1;
        fragment.targets = &target;

        WGPURenderPipelineDescriptor descriptor = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
        descriptor.layout = pipelineLayout;
        descriptor.vertex.module = shaderModule;
        descriptor.vertex.entryPoint = ToWGPUStringView("VSMain");
        descriptor.fragment = &fragment;

        WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(RI.GetDevice(), &descriptor);

        pipelines.PushBack({ format, pipeline });

        return pipeline;
    }

    void Release()
    {
        for (Pair<WGPUTextureFormat, WGPURenderPipeline>& entry : pipelines)
        {
            if (entry.second != nullptr)
            {
                wgpuRenderPipelineRelease(entry.second);
            }
        }

        pipelines.Clear();

        if (pipelineLayout != nullptr)
        {
            wgpuPipelineLayoutRelease(pipelineLayout);
            pipelineLayout = nullptr;
        }

        if (bindGroupLayout != nullptr)
        {
            wgpuBindGroupLayoutRelease(bindGroupLayout);
            bindGroupLayout = nullptr;
        }

        if (sampler != nullptr)
        {
            wgpuSamplerRelease(sampler);
            sampler = nullptr;
        }

        if (shaderModule != nullptr)
        {
            wgpuShaderModuleRelease(shaderModule);
            shaderModule = nullptr;
        }
    }
};

static BlitResources g_blitResources;

#pragma endregion Blit resources

#pragma region Depth copy resources

static const char* const g_depthCopyShaderSource = R"(
struct DepthCopyParams
{
    sourceOffset : vec2i,
    destinationOffset : vec2i,
}

@group(0) @binding(0) var sourceTexture : texture_depth_2d;
@group(0) @binding(1) var<uniform> params : DepthCopyParams;

@vertex
fn VSMain(@builtin(vertex_index) vertexIndex : u32) -> @builtin(position) vec4f
{
    let corner = vec2f(f32((vertexIndex << 1u) & 2u), f32(vertexIndex & 2u));

    return vec4f(corner * 2.0 - 1.0, 0.0, 1.0);
}

@fragment
fn PSMain(@builtin(position) position : vec4f) -> @builtin(frag_depth) f32
{
    let coord = vec2i(position.xy) - params.destinationOffset + params.sourceOffset;

    return textureLoad(sourceTexture, coord, 0);
}
)";

struct DepthCopyResources
{
    Mutex mutex;

    WGPUShaderModule shaderModule = nullptr;
    WGPUBindGroupLayout bindGroupLayout = nullptr;
    WGPUPipelineLayout pipelineLayout = nullptr;

    Array<Pair<WGPUTextureFormat, WGPURenderPipeline>, WebGPUAllocator> pipelines;

    void Initialize()
    {
        if (shaderModule != nullptr)
        {
            return;
        }

        WGPUShaderSourceWGSL shaderSource = WGPU_SHADER_SOURCE_WGSL_INIT;
        shaderSource.code = ToWGPUStringView(g_depthCopyShaderSource);

        WGPUShaderModuleDescriptor moduleDescriptor = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
        moduleDescriptor.nextInChain = &shaderSource.chain;

        shaderModule = wgpuDeviceCreateShaderModule(RI.GetDevice(), &moduleDescriptor);

        WGPUBindGroupLayoutEntry entries[2] = {
            WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT,
            WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT
        };

        entries[0].binding = 0;
        entries[0].visibility = WGPUShaderStage_Fragment;
        entries[0].texture.sampleType = WGPUTextureSampleType_Depth;
        entries[0].texture.viewDimension = WGPUTextureViewDimension_2D;

        entries[1].binding = 1;
        entries[1].visibility = WGPUShaderStage_Fragment;
        entries[1].buffer.type = WGPUBufferBindingType_Uniform;

        WGPUBindGroupLayoutDescriptor layoutDescriptor = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
        layoutDescriptor.entryCount = 2;
        layoutDescriptor.entries = entries;

        bindGroupLayout = wgpuDeviceCreateBindGroupLayout(RI.GetDevice(), &layoutDescriptor);

        WGPUPipelineLayoutDescriptor pipelineLayoutDescriptor = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
        pipelineLayoutDescriptor.bindGroupLayoutCount = 1;
        pipelineLayoutDescriptor.bindGroupLayouts = &bindGroupLayout;

        pipelineLayout = wgpuDeviceCreatePipelineLayout(RI.GetDevice(), &pipelineLayoutDescriptor);
    }

    WGPURenderPipeline GetOrCreatePipeline(WGPUTextureFormat format)
    {
        for (const Pair<WGPUTextureFormat, WGPURenderPipeline>& entry : pipelines)
        {
            if (entry.first == format)
            {
                return entry.second;
            }
        }

        WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
        fragment.module = shaderModule;
        fragment.entryPoint = ToWGPUStringView("PSMain");

        WGPUDepthStencilState depthStencil = WGPU_DEPTH_STENCIL_STATE_INIT;
        depthStencil.format = format;
        depthStencil.depthWriteEnabled = WGPUOptionalBool_True;
        depthStencil.depthCompare = WGPUCompareFunction_Always;

        WGPURenderPipelineDescriptor descriptor = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
        descriptor.layout = pipelineLayout;
        descriptor.vertex.module = shaderModule;
        descriptor.vertex.entryPoint = ToWGPUStringView("VSMain");
        descriptor.fragment = &fragment;
        descriptor.depthStencil = &depthStencil;

        WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(RI.GetDevice(), &descriptor);

        pipelines.PushBack({ format, pipeline });

        return pipeline;
    }

    void Release()
    {
        for (Pair<WGPUTextureFormat, WGPURenderPipeline>& entry : pipelines)
        {
            if (entry.second != nullptr)
            {
                wgpuRenderPipelineRelease(entry.second);
            }
        }

        pipelines.Clear();

        if (pipelineLayout != nullptr)
        {
            wgpuPipelineLayoutRelease(pipelineLayout);
            pipelineLayout = nullptr;
        }

        if (bindGroupLayout != nullptr)
        {
            wgpuBindGroupLayoutRelease(bindGroupLayout);
            bindGroupLayout = nullptr;
        }

        if (shaderModule != nullptr)
        {
            wgpuShaderModuleRelease(shaderModule);
            shaderModule = nullptr;
        }
    }
};

static DepthCopyResources g_depthCopyResources;

#pragma endregion Depth copy resources

static WGPUTextureView CreateSingleSubresourceView(WGPUTexture texture, TextureFormat format, uint32 mipLevel, uint32 arrayLayer, WGPUTextureAspect aspect)
{
    WGPUTextureViewDescriptor descriptor = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    descriptor.format = ToWGPUTextureFormat(format);
    descriptor.dimension = WGPUTextureViewDimension_2D;
    descriptor.baseMipLevel = mipLevel;
    descriptor.mipLevelCount = 1;
    descriptor.baseArrayLayer = arrayLayer;
    descriptor.arrayLayerCount = 1;
    descriptor.aspect = aspect;

    return wgpuTextureCreateView(texture, &descriptor);
}

static uint32 GetBytesPerPixel(TextureFormat format)
{
    return TextureUtils::BytesPerComponent(format) * TextureUtils::NumComponents(format);
}

WebGPUGpuImage::WebGPUGpuImage(const TextureDesc& textureDesc, EnumFlags<GpuImageFlags> flags)
    : GpuImageBase(textureDesc, flags),
      m_texture(nullptr)
{
}

WebGPUGpuImage::~WebGPUGpuImage()
{
    DestroyTexture();
}

void WebGPUGpuImage::DestroyTexture()
{
    if (m_texture != nullptr)
    {
        wgpuTextureRelease(m_texture);
        m_texture = nullptr;
    }
}

void WebGPUGpuImage::ReleaseSharedResources()
{
    {
        Mutex::Guard guard(g_blitResources.mutex);

        g_blitResources.Release();
    }

    Mutex::Guard guard(g_depthCopyResources.mutex);

    g_depthCopyResources.Release();
}

bool WebGPUGpuImage::IsCreated() const
{
    return m_texture != nullptr;
}

bool WebGPUGpuImage::IsOwned() const
{
    return true;
}

RendererResult WebGPUGpuImage::Create()
{
    return Create(ResourceState::Undefined);
}

RendererResult WebGPUGpuImage::Create(ResourceState initialState)
{
    if (IsCreated())
    {
        return {};
    }

    const Vec3u extent = GetExtent();

    if (extent.Volume() == 0)
    {
        return HYP_MAKE_ERROR(RendererError, "Invalid image extent - width*height*depth cannot equal zero");
    }

    const WGPUTextureFormat format = ToWGPUTextureFormat(m_textureDesc.format);

    if (format == WGPUTextureFormat_Undefined)
    {
        return HYP_MAKE_ERROR(RendererError, "Texture format has no WebGPU equivalent");
    }

    const bool isVolume = m_textureDesc.type == TextureType::Texture3D;

    WGPUTextureUsage usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst;

    if (!isVolume || m_textureDesc.imageUsage[ImageUsage::Attachment])
    {
        usage |= WGPUTextureUsage_RenderAttachment;
    }

    if (m_textureDesc.imageUsage[ImageUsage::Storage] && !m_textureDesc.IsDepthStencil())
    {
        usage |= WGPUTextureUsage_StorageBinding;
    }

    WGPUTextureDescriptor descriptor = WGPU_TEXTURE_DESCRIPTOR_INIT;
    descriptor.usage = usage;
    descriptor.dimension = ToWGPUTextureDimension(m_textureDesc.type);
    descriptor.size.width = extent.x;
    descriptor.size.height = extent.y;
    descriptor.size.depthOrArrayLayers = isVolume ? extent.z : m_textureDesc.NumArrayLayers();
    descriptor.format = format;
    descriptor.mipLevelCount = m_textureDesc.HasMipMaps() ? m_textureDesc.NumMips() : 1;
    descriptor.sampleCount = 1;
#ifdef HYP_RHI_DEBUG_NAMES
    if (m_debugName.IsValid())
    {
        descriptor.label = ToWGPUStringView(*m_debugName);
    }
#endif

    m_texture = wgpuDeviceCreateTexture(RI.GetDevice(), &descriptor);

    if (m_texture == nullptr)
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to create WebGPU texture");
    }

    if (initialState != ResourceState::Undefined && initialState != ResourceState::PreInitialized)
    {
        SetResourceState(initialState);
    }
    else
    {
        SetResourceState(ResourceState::Common);
    }

    return {};
}

RendererResult WebGPUGpuImage::Resize(const Vec3u& extent)
{
    if (extent == m_textureDesc.extent)
    {
        return {};
    }

    if (extent.Volume() == 0)
    {
        return HYP_MAKE_ERROR(RendererError, "Invalid image extent - width*height*depth cannot equal zero");
    }

    m_textureDesc.extent = extent;

    if (IsCreated())
    {
        const ResourceState previousResourceState = m_resourceState;

        DestroyTexture();

        m_stencilState = ResourceState::Undefined;
        m_subResourceStates.Clear();

        CheckResultOrReturn(Create(previousResourceState));
    }

    return {};
}

void WebGPUGpuImage::InsertBarrier(
    WebGPUCommandBuffer* commandBuffer,
    ResourceState newState,
    ShaderModuleType shaderModuleType,
    bool onlyDepth,
    bool onlyStencil)
{
    // WebGPU tracks usage itself, only the engine-side bookkeeping moves
    if (onlyStencil && !onlyDepth)
    {
        SetStencilState(newState);

        return;
    }

    if (onlyDepth && !onlyStencil)
    {
        const ResourceState stencilState = m_stencilState;

        SetResourceState(newState);
        m_stencilState = stencilState;

        return;
    }

    SetResourceState(newState);
}

void WebGPUGpuImage::InsertBarrier(
    WebGPUCommandBuffer* commandBuffer,
    const ImageSubResource& subResource,
    ResourceState newState,
    ShaderModuleType shaderModuleType,
    bool onlyDepth,
    bool onlyStencil)
{
    if (IsFullSubResource(subResource))
    {
        InsertBarrier(commandBuffer, newState, shaderModuleType, onlyDepth, onlyStencil);

        return;
    }

    SetSubResourceState(subResource, newState);
}

void WebGPUGpuImage::Blit(WebGPUCommandBuffer* commandBuffer, const WebGPUGpuImage* srcImage)
{
    const Vec3u srcExtent = srcImage->GetExtent();
    const Vec3u dstExtent = GetExtent();

    Blit(commandBuffer, srcImage, Rect<uint32> { 0, 0, srcExtent.x, srcExtent.y }, Rect<uint32> { 0, 0, dstExtent.x, dstExtent.y });
}

void WebGPUGpuImage::Blit(
    WebGPUCommandBuffer* commandBuffer,
    const WebGPUGpuImage* srcImage,
    const Rect<uint32>& srcRect,
    const Rect<uint32>& dstRect)
{
    const uint32 numLayers = MathUtil::Min(uint32(srcImage->NumArrayLayers()), uint32(NumArrayLayers()));

    for (uint32 arrayLayer = 0; arrayLayer < numLayers; arrayLayer++)
    {
        BlitLevel(commandBuffer, srcImage, 0, arrayLayer, 0, arrayLayer, srcRect, dstRect);
    }
}

void WebGPUGpuImage::Blit(
    WebGPUCommandBuffer* commandBuffer,
    const WebGPUGpuImage* srcImage,
    const Rect<uint32>& srcRect,
    const Rect<uint32>& dstRect,
    const ImageSubResource& srcSubResource,
    const ImageSubResource& dstSubResource)
{
    const uint32 srcNumLevels = MathUtil::Min(uint32(srcSubResource.numLevels), srcImage->NumMips() - uint32(srcSubResource.baseMipLevel));
    const uint32 dstNumLevels = MathUtil::Min(uint32(dstSubResource.numLevels), NumMips() - uint32(dstSubResource.baseMipLevel));
    const uint32 srcNumLayers = MathUtil::Min(uint32(srcSubResource.numLayers), uint32(srcImage->NumArrayLayers()) - uint32(srcSubResource.baseArrayLayer));
    const uint32 dstNumLayers = MathUtil::Min(uint32(dstSubResource.numLayers), uint32(NumArrayLayers()) - uint32(dstSubResource.baseArrayLayer));

    const uint32 numLevels = MathUtil::Min(srcNumLevels, dstNumLevels);
    const uint32 numLayers = MathUtil::Min(srcNumLayers, dstNumLayers);

    for (uint32 levelIndex = 0; levelIndex < numLevels; levelIndex++)
    {
        const uint32 srcMipLevel = srcSubResource.baseMipLevel + levelIndex;
        const uint32 dstMipLevel = dstSubResource.baseMipLevel + levelIndex;

        const Rect<uint32> srcLevelRect {
            srcRect.x0 >> levelIndex, srcRect.y0 >> levelIndex,
            MathUtil::Max(srcRect.x1 >> levelIndex, (srcRect.x0 >> levelIndex) + 1), MathUtil::Max(srcRect.y1 >> levelIndex, (srcRect.y0 >> levelIndex) + 1)
        };

        const Rect<uint32> dstLevelRect {
            dstRect.x0 >> levelIndex, dstRect.y0 >> levelIndex,
            MathUtil::Max(dstRect.x1 >> levelIndex, (dstRect.x0 >> levelIndex) + 1), MathUtil::Max(dstRect.y1 >> levelIndex, (dstRect.y0 >> levelIndex) + 1)
        };

        for (uint32 layerIndex = 0; layerIndex < numLayers; layerIndex++)
        {
            BlitLevel(
                commandBuffer, srcImage,
                srcMipLevel, srcSubResource.baseArrayLayer + layerIndex,
                dstMipLevel, dstSubResource.baseArrayLayer + layerIndex,
                srcLevelRect, dstLevelRect);
        }
    }
}

void WebGPUGpuImage::BlitLevel(
    WebGPUCommandBuffer* commandBuffer,
    const WebGPUGpuImage* srcImage,
    uint32 srcMipLevel, uint32 srcArrayLayer,
    uint32 dstMipLevel, uint32 dstArrayLayer,
    const Rect<uint32>& srcRect,
    const Rect<uint32>& dstRect)
{
    if (!IsCreated() || srcImage == nullptr || !srcImage->IsCreated())
    {
        return;
    }

    const TextureDesc& srcDesc = srcImage->GetTextureDesc();

    const bool isSourceFilterable = GetDefaultSampleType(srcDesc.format, RI.GetDeviceFeatures().float32Filterable) == WGPUTextureSampleType_Float;

    if (!isSourceFilterable || m_textureDesc.IsDepthStencil() || m_textureDesc.type == TextureType::Texture3D || srcDesc.type == TextureType::Texture3D)
    {
        HYP_LOG(RenderingBackend, Warning, "Blit between formats {} and {} is not supported on WebGPU", EnumToString(srcDesc.format), EnumToString(m_textureDesc.format));

        return;
    }

    commandBuffer->EndActivePass();

    const Vec3u srcMipExtent = srcDesc.GetMipExtent(uint8(srcMipLevel));
    const Vec3u dstMipExtent = m_textureDesc.GetMipExtent(uint8(dstMipLevel));

    const float params[4] = {
        float(srcRect.x0) / float(srcMipExtent.x),
        float(srcRect.y0) / float(srcMipExtent.y),
        float(srcRect.x1 - srcRect.x0) / float(srcMipExtent.x),
        float(srcRect.y1 - srcRect.y0) / float(srcMipExtent.y)
    };

    WGPURenderPipeline pipeline;
    WGPUBindGroupLayout bindGroupLayout;
    WGPUSampler sampler;

    {
        Mutex::Guard guard(g_blitResources.mutex);

        g_blitResources.Initialize();

        pipeline = g_blitResources.GetOrCreatePipeline(ToWGPUTextureFormat(m_textureDesc.format));
        bindGroupLayout = g_blitResources.bindGroupLayout;
        sampler = g_blitResources.sampler;
    }

    if (pipeline == nullptr)
    {
        return;
    }

    WGPUBufferDescriptor paramsBufferDescriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    paramsBufferDescriptor.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    paramsBufferDescriptor.size = sizeof(params);

    WGPUBuffer paramsBuffer = wgpuDeviceCreateBuffer(RI.GetDevice(), &paramsBufferDescriptor);
    wgpuQueueWriteBuffer(RI.GetQueue(), paramsBuffer, 0, params, sizeof(params));

    WGPUTextureView srcView = CreateSingleSubresourceView(srcImage->GetWGPUTexture(), srcDesc.format, srcMipLevel, srcArrayLayer, WGPUTextureAspect_All);
    WGPUTextureView dstView = CreateSingleSubresourceView(m_texture, m_textureDesc.format, dstMipLevel, dstArrayLayer, WGPUTextureAspect_All);

    WGPUBindGroupEntry entries[3] = {
        WGPU_BIND_GROUP_ENTRY_INIT,
        WGPU_BIND_GROUP_ENTRY_INIT,
        WGPU_BIND_GROUP_ENTRY_INIT
    };

    entries[0].binding = 0;
    entries[0].textureView = srcView;

    entries[1].binding = 1;
    entries[1].sampler = sampler;

    entries[2].binding = 2;
    entries[2].buffer = paramsBuffer;
    entries[2].size = sizeof(params);

    WGPUBindGroupDescriptor bindGroupDescriptor = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    bindGroupDescriptor.layout = bindGroupLayout;
    bindGroupDescriptor.entryCount = 3;
    bindGroupDescriptor.entries = entries;

    WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(RI.GetDevice(), &bindGroupDescriptor);

    const bool coversWholeTarget = dstRect.x0 == 0 && dstRect.y0 == 0 && dstRect.x1 >= dstMipExtent.x && dstRect.y1 >= dstMipExtent.y;

    WGPURenderPassColorAttachment colorAttachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
    colorAttachment.view = dstView;
    colorAttachment.loadOp = coversWholeTarget ? WGPULoadOp_Clear : WGPULoadOp_Load;
    colorAttachment.storeOp = WGPUStoreOp_Store;

    WGPURenderPassDescriptor passDescriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
    passDescriptor.colorAttachmentCount = 1;
    passDescriptor.colorAttachments = &colorAttachment;

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(commandBuffer->GetEncoder(), &passDescriptor);

    const uint32 viewportWidth = MathUtil::Min(dstRect.x1, dstMipExtent.x) - dstRect.x0;
    const uint32 viewportHeight = MathUtil::Min(dstRect.y1, dstMipExtent.y) - dstRect.y0;

    wgpuRenderPassEncoderSetPipeline(pass, pipeline);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, bindGroup, 0, nullptr);
    wgpuRenderPassEncoderSetViewport(pass, float(dstRect.x0), float(dstRect.y0), float(viewportWidth), float(viewportHeight), 0.0f, 1.0f);
    wgpuRenderPassEncoderSetScissorRect(pass, dstRect.x0, dstRect.y0, viewportWidth, viewportHeight);
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    wgpuBindGroupRelease(bindGroup);
    wgpuTextureViewRelease(dstView);
    wgpuTextureViewRelease(srcView);
    wgpuBufferRelease(paramsBuffer);
}

RendererResult WebGPUGpuImage::GenerateMipmaps(WebGPUCommandBuffer* commandBuffer)
{
    if (!IsCreated())
    {
        return HYP_MAKE_ERROR(RendererError, "Cannot generate mipmaps on uninitialized image");
    }

    const uint32 numMips = m_textureDesc.HasMipMaps() ? m_textureDesc.NumMips() : 1;
    const uint32 numLayers = m_textureDesc.NumArrayLayers();

    for (uint32 arrayLayer = 0; arrayLayer < numLayers; arrayLayer++)
    {
        for (uint32 mipLevel = 1; mipLevel < numMips; mipLevel++)
        {
            const Vec3u srcMipExtent = m_textureDesc.GetMipExtent(uint8(mipLevel - 1));
            const Vec3u dstMipExtent = m_textureDesc.GetMipExtent(uint8(mipLevel));

            BlitLevel(
                commandBuffer, this,
                mipLevel - 1, arrayLayer,
                mipLevel, arrayLayer,
                Rect<uint32> { 0, 0, srcMipExtent.x, srcMipExtent.y },
                Rect<uint32> { 0, 0, dstMipExtent.x, dstMipExtent.y });
        }
    }

    return {};
}

void WebGPUGpuImage::CopyFromBuffer(
    WebGPUCommandBuffer* commandBuffer,
    const WebGPUGpuBuffer* srcBuffer,
    uint32 srcBufferOffset,
    uint8 dstMipIndex,
    uint16 dstArrayLayer) const
{
    AssertDebug(srcBuffer != nullptr && srcBuffer->IsCreated(), "Source buffer is not created");
    AssertDebug(IsCreated(), "Destination image is not created");

    const uint8 mipIndex = dstMipIndex != UINT8_MAX ? dstMipIndex : 0;

    const Vec3u mipExtent = m_textureDesc.GetMipExtent(mipIndex);
    const uint32 alignedRowPitch = ByteUtil::AlignAs(mipExtent.x * GetBytesPerPixel(m_textureDesc.format), CopyRowPitchAlignment);
    const size_t layerStep = ByteUtil::AlignAs(size_t(alignedRowPitch) * mipExtent.y * mipExtent.z, size_t(CopyPlacementAlignment));

    const bool isVolume = m_textureDesc.type == TextureType::Texture3D;
    const uint16 numLayersToCopy = (dstArrayLayer == UINT16_MAX && !isVolume) ? NumArrayLayers() : 1;

    commandBuffer->EndActivePass();

    for (uint16 layerIndex = 0; layerIndex < numLayersToCopy; layerIndex++)
    {
        const uint16 actualLayer = dstArrayLayer == UINT16_MAX ? layerIndex : dstArrayLayer;

        WGPUTexelCopyBufferInfo source = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
        source.buffer = srcBuffer->GetWGPUBuffer();
        source.layout.offset = uint64(srcBufferOffset) + uint64(layerIndex) * layerStep;
        source.layout.bytesPerRow = alignedRowPitch;
        source.layout.rowsPerImage = mipExtent.y;

        WGPUTexelCopyTextureInfo destination = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
        destination.texture = m_texture;
        destination.mipLevel = mipIndex;
        destination.origin.z = isVolume ? 0 : actualLayer;

        const WGPUExtent3D copyExtent { mipExtent.x, mipExtent.y, isVolume ? mipExtent.z : 1 };

        wgpuCommandEncoderCopyBufferToTexture(commandBuffer->GetEncoder(), &source, &destination, &copyExtent);
    }
}

void WebGPUGpuImage::CopyToBuffer(
    WebGPUCommandBuffer* commandBuffer,
    WebGPUGpuBuffer* dstBuffer,
    const ImageSubResource& subResource,
    size_t bufferOffset) const
{
    AssertDebug(IsCreated(), "Source image is not created");
    AssertDebug(dstBuffer != nullptr && dstBuffer->IsCreated(), "Destination buffer is not created");

    const uint32 numLevels = MathUtil::Min(uint32(subResource.numLevels), NumMips() - uint32(subResource.baseMipLevel));
    const uint32 numLayers = MathUtil::Min(uint32(subResource.numLayers), uint32(NumArrayLayers()) - uint32(subResource.baseArrayLayer));

    const bool isVolume = m_textureDesc.type == TextureType::Texture3D;

    commandBuffer->EndActivePass();

    for (uint32 mipIndex = subResource.baseMipLevel; mipIndex < subResource.baseMipLevel + numLevels; mipIndex++)
    {
        const Vec3u mipExtent = m_textureDesc.GetMipExtent(uint8(mipIndex));
        const uint32 alignedRowPitch = ByteUtil::AlignAs(mipExtent.x * GetBytesPerPixel(m_textureDesc.format), CopyRowPitchAlignment);
        const size_t layerStep = ByteUtil::AlignAs(size_t(alignedRowPitch) * mipExtent.y * mipExtent.z, size_t(CopyPlacementAlignment));

        for (uint32 layerIndex = 0; layerIndex < numLayers; layerIndex++)
        {
            WGPUTexelCopyTextureInfo source = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
            source.texture = m_texture;
            source.mipLevel = mipIndex;
            source.origin.z = isVolume ? 0 : subResource.baseArrayLayer + layerIndex;
            source.aspect = GetSampledAspect(m_textureDesc.format);

            WGPUTexelCopyBufferInfo destination = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
            destination.buffer = dstBuffer->GetWGPUBuffer();
            destination.layout.offset = uint64(bufferOffset) + uint64(layerIndex) * layerStep;
            destination.layout.bytesPerRow = alignedRowPitch;
            destination.layout.rowsPerImage = mipExtent.y;

            const WGPUExtent3D copyExtent { mipExtent.x, mipExtent.y, isVolume ? mipExtent.z : 1 };

            wgpuCommandEncoderCopyTextureToBuffer(commandBuffer->GetEncoder(), &source, &destination, &copyExtent);
        }

        bufferOffset += layerStep * numLayers;
    }

    if (dstBuffer->GetBufferType() == GpuBufferType::ReadbackBuffer)
    {
        commandBuffer->AddPendingReadback(dstBuffer);
    }
}

void WebGPUGpuImage::CopyFrom(
    WebGPUCommandBuffer* commandBuffer,
    const WebGPUGpuImage* srcImage,
    const Vec3u& srcOffset,
    const Vec3u& dstOffset,
    const Vec3u& extent,
    const ImageSubResource& srcSubResource,
    const ImageSubResource& dstSubResource)
{
    AssertDebug(IsCreated() && srcImage != nullptr && srcImage->IsCreated());

    const uint32 srcNumLevels = MathUtil::Min(uint32(srcSubResource.numLevels), srcImage->NumMips() - uint32(srcSubResource.baseMipLevel));
    const uint32 dstNumLevels = MathUtil::Min(uint32(dstSubResource.numLevels), NumMips() - uint32(dstSubResource.baseMipLevel));
    const uint32 srcNumLayers = MathUtil::Min(uint32(srcSubResource.numLayers), uint32(srcImage->NumArrayLayers()) - uint32(srcSubResource.baseArrayLayer));
    const uint32 dstNumLayers = MathUtil::Min(uint32(dstSubResource.numLayers), uint32(NumArrayLayers()) - uint32(dstSubResource.baseArrayLayer));

    const uint32 numLevels = MathUtil::Min(srcNumLevels, dstNumLevels);
    const uint32 numLayers = MathUtil::Min(srcNumLayers, dstNumLayers);

    const bool isSourceVolume = srcImage->GetType() == TextureType::Texture3D;
    const bool isDestinationVolume = m_textureDesc.type == TextureType::Texture3D;

    commandBuffer->EndActivePass();

    if (m_textureDesc.IsDepthStencil() && srcImage->GetTextureDesc().IsDepthStencil() && srcImage != this)
    {
        for (uint32 levelIndex = 0; levelIndex < numLevels; levelIndex++)
        {
            const uint32 srcMipLevel = srcSubResource.baseMipLevel + levelIndex;
            const uint32 dstMipLevel = dstSubResource.baseMipLevel + levelIndex;

            const Vec3u srcMipExtent = srcImage->GetTextureDesc().GetMipExtent(uint8(srcMipLevel));
            const Vec3u dstMipExtent = m_textureDesc.GetMipExtent(uint8(dstMipLevel));

            const Vec2u levelSrcOffset { srcOffset.x >> levelIndex, srcOffset.y >> levelIndex };
            const Vec2u levelDstOffset { dstOffset.x >> levelIndex, dstOffset.y >> levelIndex };
            const Vec2u levelExtent { MathUtil::Max(extent.x >> levelIndex, 1u), MathUtil::Max(extent.y >> levelIndex, 1u) };

            const bool coversSource = levelSrcOffset == Vec2u::Zero() && levelExtent == srcMipExtent.GetXY();
            const bool coversDestination = levelDstOffset == Vec2u::Zero() && levelExtent == dstMipExtent.GetXY();

            if (coversSource && coversDestination)
            {
                WGPUTexelCopyTextureInfo source = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
                source.texture = srcImage->GetWGPUTexture();
                source.mipLevel = srcMipLevel;
                source.origin.z = srcSubResource.baseArrayLayer;

                WGPUTexelCopyTextureInfo destination = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
                destination.texture = m_texture;
                destination.mipLevel = dstMipLevel;
                destination.origin.z = dstSubResource.baseArrayLayer;

                const WGPUExtent3D copyExtent { levelExtent.x, levelExtent.y, numLayers };

                wgpuCommandEncoderCopyTextureToTexture(commandBuffer->GetEncoder(), &source, &destination, &copyExtent);

                continue;
            }

            for (uint32 layerIndex = 0; layerIndex < numLayers; layerIndex++)
            {
                CopyDepthRegion(
                    commandBuffer, srcImage,
                    srcMipLevel, srcSubResource.baseArrayLayer + layerIndex,
                    dstMipLevel, dstSubResource.baseArrayLayer + layerIndex,
                    levelSrcOffset, levelDstOffset, levelExtent);
            }
        }

        return;
    }

    for (uint32 levelIndex = 0; levelIndex < numLevels; levelIndex++)
    {
        WGPUTexelCopyTextureInfo source = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
        source.texture = srcImage->GetWGPUTexture();
        source.mipLevel = srcSubResource.baseMipLevel + levelIndex;
        source.origin.x = srcOffset.x >> levelIndex;
        source.origin.y = srcOffset.y >> levelIndex;
        source.origin.z = isSourceVolume ? (srcOffset.z >> levelIndex) : srcSubResource.baseArrayLayer;

        WGPUTexelCopyTextureInfo destination = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
        destination.texture = m_texture;
        destination.mipLevel = dstSubResource.baseMipLevel + levelIndex;
        destination.origin.x = dstOffset.x >> levelIndex;
        destination.origin.y = dstOffset.y >> levelIndex;
        destination.origin.z = isDestinationVolume ? (dstOffset.z >> levelIndex) : dstSubResource.baseArrayLayer;

        const WGPUExtent3D copyExtent {
            MathUtil::Max(extent.x >> levelIndex, 1u),
            MathUtil::Max(extent.y >> levelIndex, 1u),
            (isSourceVolume || isDestinationVolume) ? MathUtil::Max(extent.z >> levelIndex, 1u) : numLayers
        };

        wgpuCommandEncoderCopyTextureToTexture(commandBuffer->GetEncoder(), &source, &destination, &copyExtent);
    }
}

void WebGPUGpuImage::CopyDepthRegion(
    WebGPUCommandBuffer* commandBuffer,
    const WebGPUGpuImage* srcImage,
    uint32 srcMipLevel, uint32 srcArrayLayer,
    uint32 dstMipLevel, uint32 dstArrayLayer,
    const Vec2u& srcOffset,
    const Vec2u& dstOffset,
    const Vec2u& extent)
{
    WGPURenderPipeline pipeline;
    WGPUBindGroupLayout bindGroupLayout;

    {
        Mutex::Guard guard(g_depthCopyResources.mutex);

        g_depthCopyResources.Initialize();

        pipeline = g_depthCopyResources.GetOrCreatePipeline(ToWGPUTextureFormat(m_textureDesc.format));
        bindGroupLayout = g_depthCopyResources.bindGroupLayout;
    }

    if (pipeline == nullptr)
    {
        return;
    }

    const int32 params[4] = { int32(srcOffset.x), int32(srcOffset.y), int32(dstOffset.x), int32(dstOffset.y) };

    WGPUBufferDescriptor paramsBufferDescriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    paramsBufferDescriptor.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    paramsBufferDescriptor.size = sizeof(params);

    WGPUBuffer paramsBuffer = wgpuDeviceCreateBuffer(RI.GetDevice(), &paramsBufferDescriptor);
    wgpuQueueWriteBuffer(RI.GetQueue(), paramsBuffer, 0, params, sizeof(params));

    WGPUTextureViewDescriptor srcViewDescriptor = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    srcViewDescriptor.dimension = WGPUTextureViewDimension_2D;
    srcViewDescriptor.baseMipLevel = srcMipLevel;
    srcViewDescriptor.mipLevelCount = 1;
    srcViewDescriptor.baseArrayLayer = srcArrayLayer;
    srcViewDescriptor.arrayLayerCount = 1;
    srcViewDescriptor.aspect = WGPUTextureAspect_DepthOnly;

    WGPUTextureView srcView = wgpuTextureCreateView(srcImage->GetWGPUTexture(), &srcViewDescriptor);
    WGPUTextureView dstView = CreateSingleSubresourceView(m_texture, m_textureDesc.format, dstMipLevel, dstArrayLayer, WGPUTextureAspect_All);

    WGPUBindGroupEntry entries[2] = {
        WGPU_BIND_GROUP_ENTRY_INIT,
        WGPU_BIND_GROUP_ENTRY_INIT
    };

    entries[0].binding = 0;
    entries[0].textureView = srcView;

    entries[1].binding = 1;
    entries[1].buffer = paramsBuffer;
    entries[1].size = sizeof(params);

    WGPUBindGroupDescriptor bindGroupDescriptor = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    bindGroupDescriptor.layout = bindGroupLayout;
    bindGroupDescriptor.entryCount = 2;
    bindGroupDescriptor.entries = entries;

    WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(RI.GetDevice(), &bindGroupDescriptor);

    WGPURenderPassDepthStencilAttachment depthStencilAttachment = WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
    depthStencilAttachment.view = dstView;
    depthStencilAttachment.depthLoadOp = WGPULoadOp_Load;
    depthStencilAttachment.depthStoreOp = WGPUStoreOp_Store;

    if (TextureUtils::HasStencilComponent(m_textureDesc.format))
    {
        depthStencilAttachment.stencilLoadOp = WGPULoadOp_Load;
        depthStencilAttachment.stencilStoreOp = WGPUStoreOp_Store;
    }

    WGPURenderPassDescriptor passDescriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
    passDescriptor.depthStencilAttachment = &depthStencilAttachment;

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(commandBuffer->GetEncoder(), &passDescriptor);

    wgpuRenderPassEncoderSetPipeline(pass, pipeline);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, bindGroup, 0, nullptr);
    wgpuRenderPassEncoderSetViewport(pass, float(dstOffset.x), float(dstOffset.y), float(extent.x), float(extent.y), 0.0f, 1.0f);
    wgpuRenderPassEncoderSetScissorRect(pass, dstOffset.x, dstOffset.y, extent.x, extent.y);
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    wgpuBindGroupRelease(bindGroup);
    wgpuTextureViewRelease(dstView);
    wgpuTextureViewRelease(srcView);
    wgpuBufferRelease(paramsBuffer);
}

void WebGPUGpuImage::Fill(
    WebGPUCommandBuffer* commandBuffer,
    float value,
    const ImageSubResource& subResource,
    const Vec3u& offset,
    const Vec3u& extent)
{
    AssertDebug(IsCreated(), "Image is not created");

    if (m_textureDesc.type == TextureType::Texture3D)
    {
        HYP_LOG(RenderingBackend, Warning, "Fill is not supported for 3D textures on WebGPU");

        return;
    }

    const uint32 numLevels = MathUtil::Min(uint32(subResource.numLevels), NumMips() - uint32(subResource.baseMipLevel));
    const uint32 numLayers = MathUtil::Min(uint32(subResource.numLayers), uint32(NumArrayLayers()) - uint32(subResource.baseArrayLayer));

    const bool isDepthStencil = m_textureDesc.IsDepthStencil();
    const bool hasStencil = TextureUtils::HasStencilComponent(m_textureDesc.format);

    commandBuffer->EndActivePass();

    // the only clear WebGPU has is a render pass load operation, which always covers the whole subresource
    for (uint32 mipLevel = subResource.baseMipLevel; mipLevel < subResource.baseMipLevel + numLevels; mipLevel++)
    {
        for (uint32 arrayLayer = subResource.baseArrayLayer; arrayLayer < subResource.baseArrayLayer + numLayers; arrayLayer++)
        {
            WGPUTextureView view = CreateSingleSubresourceView(m_texture, m_textureDesc.format, mipLevel, arrayLayer, WGPUTextureAspect_All);

            WGPURenderPassDescriptor passDescriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;

            WGPURenderPassColorAttachment colorAttachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
            WGPURenderPassDepthStencilAttachment depthStencilAttachment = WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;

            if (isDepthStencil)
            {
                depthStencilAttachment.view = view;
                depthStencilAttachment.depthLoadOp = WGPULoadOp_Clear;
                depthStencilAttachment.depthStoreOp = WGPUStoreOp_Store;
                depthStencilAttachment.depthClearValue = value;

                if (hasStencil)
                {
                    depthStencilAttachment.stencilLoadOp = WGPULoadOp_Clear;
                    depthStencilAttachment.stencilStoreOp = WGPUStoreOp_Store;
                    depthStencilAttachment.stencilClearValue = 0;
                }

                passDescriptor.depthStencilAttachment = &depthStencilAttachment;
            }
            else
            {
                colorAttachment.view = view;
                colorAttachment.loadOp = WGPULoadOp_Clear;
                colorAttachment.storeOp = WGPUStoreOp_Store;
                colorAttachment.clearValue = WGPUColor { value, value, value, value };

                passDescriptor.colorAttachmentCount = 1;
                passDescriptor.colorAttachments = &colorAttachment;
            }

            WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(commandBuffer->GetEncoder(), &passDescriptor);
            wgpuRenderPassEncoderEnd(pass);
            wgpuRenderPassEncoderRelease(pass);

            wgpuTextureViewRelease(view);
        }
    }
}

WebGPUGpuImageViewRef WebGPUGpuImage::MakeLayerImageView(uint32 layerIndex) const
{
    if (!IsCreated())
    {
        HYP_LOG(RenderingBackend, Warning, "Attempt to create image view on uninitialized image");

        return WebGPUGpuImageViewRef::Null();
    }

    return RI.MakeImageView(MakeStrongRef(this), 0, m_textureDesc.NumMips(), layerIndex, 1);
}

#ifdef HYP_RHI_DEBUG_NAMES
void WebGPUGpuImage::SetDebugName(Name name)
{
    GpuImageBase::SetDebugName(name);

    if (m_texture != nullptr && name.IsValid())
    {
        wgpuTextureSetLabel(m_texture, ToWGPUStringView(*name));
    }
}
#endif

} // namespace Hyperion
