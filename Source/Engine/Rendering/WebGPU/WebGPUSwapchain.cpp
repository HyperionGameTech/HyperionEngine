/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <WebGPUPch.hpp>

#include <Rendering/WebGPU/WebGPUSwapchain.hpp>
#include <Rendering/WebGPU/WebGPUFramebuffer.hpp>
#include <Rendering/WebGPU/WebGPURenderInterface.hpp>
#include <Rendering/WebGPU/WebGPUHelpers.hpp>

#include <System/AppContext.hpp>

#include <Framework/CVarManager.hpp>

#include <WebGPUSwapchain.generated.inl>

#ifdef HYP_WEB
#include <emscripten.h>

EM_ASYNC_JS(void, WaitForAnimationFrame, (), {
    // a hidden page gets no animation frames, the timer keeps the engine ticking there
    await new Promise((resolve) => {
        requestAnimationFrame(resolve);
        setTimeout(resolve, 100);
    });
});
#endif

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern WebGPURenderInterface RI;

extern CVar<bool> g_cvEnableVSync;

// the surface texture is BGRA8, rendered to through an sRGB view so the final pass output is encoded on write
static constexpr TextureFormat g_swapchainFormat = TextureFormat::BGRA8_SRGB;

WebGPUSwapchain::WebGPUSwapchain(ApplicationWindow* window, const Vec2u& extent)
    : SwapchainBase(extent),
      m_window(window),
      m_surface(nullptr),
      m_backbufferTexture(nullptr),
      m_backbufferTextureView(nullptr),
      m_isConfigured(false)
{
    m_imageFormat = g_swapchainFormat;
}

WebGPUSwapchain::~WebGPUSwapchain()
{
    m_framebuffers.Clear();

    ReleaseBackbuffer();

    if (m_surface != nullptr)
    {
        if (m_isConfigured)
        {
            wgpuSurfaceUnconfigure(m_surface);
        }

        wgpuSurfaceRelease(m_surface);
    }
}

bool WebGPUSwapchain::IsCreated() const
{
    return m_surface != nullptr && m_isConfigured;
}

void WebGPUSwapchain::ReleaseBackbuffer()
{
    if (m_backbufferTextureView != nullptr)
    {
        wgpuTextureViewRelease(m_backbufferTextureView);
        m_backbufferTextureView = nullptr;
    }

    if (m_backbufferTexture != nullptr)
    {
        wgpuTextureRelease(m_backbufferTexture);
        m_backbufferTexture = nullptr;
    }
}

RendererResult WebGPUSwapchain::Create()
{
    if (m_surface == nullptr)
    {
        Assert(m_window != nullptr);

        WGPUSurfaceDescriptor surfaceDescriptor = WGPU_SURFACE_DESCRIPTOR_INIT;

#ifdef HYP_WINDOWS
        WGPUSurfaceSourceWindowsHWND surfaceSource = WGPU_SURFACE_SOURCE_WINDOWS_HWND_INIT;
        surfaceSource.hinstance = GetModuleHandleW(nullptr);
        surfaceSource.hwnd = m_window->GetHWND();

        surfaceDescriptor.nextInChain = &surfaceSource.chain;
#elif defined(HYP_WEB)
        WGPUEmscriptenSurfaceSourceCanvasHTMLSelector surfaceSource = WGPU_EMSCRIPTEN_SURFACE_SOURCE_CANVAS_HTML_SELECTOR_INIT;
        surfaceSource.selector = { "#canvas", WGPU_STRLEN };

        surfaceDescriptor.nextInChain = &surfaceSource.chain;
#else
        return HYP_MAKE_ERROR(RendererError, "No WebGPU surface source for this platform");
#endif

        m_surface = wgpuInstanceCreateSurface(RI.GetInstance(), &surfaceDescriptor);

        if (m_surface == nullptr)
        {
            return HYP_MAKE_ERROR(RendererError, "Failed to create WebGPU surface");
        }
    }

    Configure();

    FramebufferDesc framebufferDesc {};
    framebufferDesc.extent = m_extent;
    framebufferDesc.renderPassMode = RenderPassMode::Present;

    WebGPUFramebufferRef framebuffer = RI.MakeFramebuffer(framebufferDesc);
    Assert(framebuffer.IsValid());

    framebuffer->SetExternalTextureView(m_backbufferTextureView, m_extent, g_swapchainFormat);
    CheckResultOrReturn(framebuffer->Create());

    m_framebuffers.Clear();
    m_framebuffers.PushBack(framebuffer);

    m_acquiredImageIndex = 0;
    m_needsRecreate = false;

    return {};
}

void WebGPUSwapchain::Configure()
{
    const WGPUTextureFormat viewFormat = ToWGPUTextureFormat(g_swapchainFormat);

    WGPUSurfaceConfiguration configuration = WGPU_SURFACE_CONFIGURATION_INIT;
    configuration.device = RI.GetDevice();
    configuration.format = WGPUTextureFormat_BGRA8Unorm;
    configuration.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopyDst;
    configuration.width = MathUtil::Max(m_extent.x, 1u);
    configuration.height = MathUtil::Max(m_extent.y, 1u);
    configuration.viewFormatCount = 1;
    configuration.viewFormats = &viewFormat;
    configuration.alphaMode = WGPUCompositeAlphaMode_Opaque;
    configuration.presentMode = g_cvEnableVSync.Get() ? WGPUPresentMode_Fifo : WGPUPresentMode_Immediate;

    wgpuSurfaceConfigure(m_surface, &configuration);

    m_isConfigured = true;

    // The frame is drawn into this and copied to the canvas texture when presenting. The browser expires the canvas
    // texture whenever this thread yields, which a frame waiting on pipelines or a readback does.
    ReleaseBackbuffer();

    WGPUTextureDescriptor textureDescriptor = WGPU_TEXTURE_DESCRIPTOR_INIT;
    textureDescriptor.label = { "SwapchainBackbuffer", WGPU_STRLEN };
    textureDescriptor.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    textureDescriptor.dimension = WGPUTextureDimension_2D;
    textureDescriptor.size = { configuration.width, configuration.height, 1 };
    textureDescriptor.format = configuration.format;
    textureDescriptor.mipLevelCount = 1;
    textureDescriptor.sampleCount = 1;
    textureDescriptor.viewFormatCount = 1;
    textureDescriptor.viewFormats = &viewFormat;

    m_backbufferTexture = wgpuDeviceCreateTexture(RI.GetDevice(), &textureDescriptor);

    WGPUTextureViewDescriptor viewDescriptor = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    viewDescriptor.format = viewFormat;
    viewDescriptor.dimension = WGPUTextureViewDimension_2D;
    viewDescriptor.mipLevelCount = 1;
    viewDescriptor.arrayLayerCount = 1;

    m_backbufferTextureView = wgpuTextureCreateView(m_backbufferTexture, &viewDescriptor);
}

void WebGPUSwapchain::SetExtent(Vec2u newExtent)
{
    if (m_extent == newExtent)
    {
        return;
    }

    m_extent = newExtent;
    m_needsRecreate = true;
}

void WebGPUSwapchain::Recreate()
{
    RendererResult result = Create();

    if (!result)
    {
        HYP_LOG(RenderingBackend, Error, "Failed to recreate WebGPU swapchain: {}", result.GetError().GetMessage());
    }
}

void WebGPUSwapchain::PrepareForFrame()
{
    if (m_needsRecreate)
    {
        Recreate();
    }
}

void WebGPUSwapchain::PresentFrame()
{
    if (!IsCreated() || m_backbufferTexture == nullptr)
    {
        return;
    }

    WGPUSurfaceTexture surfaceTexture = WGPU_SURFACE_TEXTURE_INIT;
    wgpuSurfaceGetCurrentTexture(m_surface, &surfaceTexture);

    const bool isUsable = surfaceTexture.status == WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal
        || surfaceTexture.status == WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal;

    if (isUsable && surfaceTexture.texture != nullptr)
    {
        WGPUTexelCopyTextureInfo source = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
        source.texture = m_backbufferTexture;

        WGPUTexelCopyTextureInfo destination = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
        destination.texture = surfaceTexture.texture;

        const WGPUExtent3D copyExtent {
            MathUtil::Min(wgpuTextureGetWidth(m_backbufferTexture), wgpuTextureGetWidth(surfaceTexture.texture)),
            MathUtil::Min(wgpuTextureGetHeight(m_backbufferTexture), wgpuTextureGetHeight(surfaceTexture.texture)),
            1
        };

        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(RI.GetDevice(), nullptr);
        wgpuCommandEncoderCopyTextureToTexture(encoder, &source, &destination, &copyExtent);

        WGPUCommandBuffer copyCommandBuffer = wgpuCommandEncoderFinish(encoder, nullptr);
        wgpuCommandEncoderRelease(encoder);

        wgpuQueueSubmit(RI.GetQueue(), 1, &copyCommandBuffer);
        wgpuCommandBufferRelease(copyCommandBuffer);
    }
    else
    {
        m_needsRecreate = true;
    }

#ifdef HYP_WEB
    // the canvas is presented when this thread yields; waiting for the next animation frame also paces the loop
    WaitForAnimationFrame();
#else
    if (isUsable)
    {
        wgpuSurfacePresent(m_surface);
    }
#endif

    if (surfaceTexture.texture != nullptr)
    {
        wgpuTextureRelease(surfaceTexture.texture);
    }
}

} // namespace Hyperion
