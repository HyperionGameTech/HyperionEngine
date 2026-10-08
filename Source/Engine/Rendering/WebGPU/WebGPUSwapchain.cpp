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
      m_currentTexture(nullptr),
      m_currentTextureView(nullptr),
      m_isConfigured(false)
{
    m_imageFormat = g_swapchainFormat;
}

WebGPUSwapchain::~WebGPUSwapchain()
{
    ReleaseCurrentTexture();

    m_framebuffers.Clear();

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

void WebGPUSwapchain::ReleaseCurrentTexture()
{
    if (m_currentTextureView != nullptr)
    {
        wgpuTextureViewRelease(m_currentTextureView);
        m_currentTextureView = nullptr;
    }

    if (m_currentTexture != nullptr)
    {
        wgpuTextureRelease(m_currentTexture);
        m_currentTexture = nullptr;
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

    framebuffer->SetExternalTextureView(nullptr, m_extent, g_swapchainFormat);
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
    configuration.usage = WGPUTextureUsage_RenderAttachment;
    configuration.width = MathUtil::Max(m_extent.x, 1u);
    configuration.height = MathUtil::Max(m_extent.y, 1u);
    configuration.viewFormatCount = 1;
    configuration.viewFormats = &viewFormat;
    configuration.alphaMode = WGPUCompositeAlphaMode_Opaque;
    configuration.presentMode = g_cvEnableVSync.Get() ? WGPUPresentMode_Fifo : WGPUPresentMode_Immediate;

    wgpuSurfaceConfigure(m_surface, &configuration);

    m_isConfigured = true;
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
    ReleaseCurrentTexture();

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

    if (!IsCreated())
    {
        return;
    }

    ReleaseCurrentTexture();

    WGPUSurfaceTexture surfaceTexture = WGPU_SURFACE_TEXTURE_INIT;
    wgpuSurfaceGetCurrentTexture(m_surface, &surfaceTexture);

    const bool isUsable = surfaceTexture.status == WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal
        || surfaceTexture.status == WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal;

    WebGPUFramebuffer* framebuffer = m_framebuffers[0].Get();

    if (!isUsable || surfaceTexture.texture == nullptr)
    {
        if (surfaceTexture.texture != nullptr)
        {
            wgpuTextureRelease(surfaceTexture.texture);
        }

        framebuffer->SetExternalTextureView(nullptr, m_extent, g_swapchainFormat);

        m_needsRecreate = true;

        return;
    }

    m_currentTexture = surfaceTexture.texture;

    WGPUTextureViewDescriptor viewDescriptor = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    viewDescriptor.format = ToWGPUTextureFormat(g_swapchainFormat);
    viewDescriptor.dimension = WGPUTextureViewDimension_2D;
    viewDescriptor.mipLevelCount = 1;
    viewDescriptor.arrayLayerCount = 1;

    m_currentTextureView = wgpuTextureCreateView(m_currentTexture, &viewDescriptor);

    framebuffer->SetExternalTextureView(m_currentTextureView, m_extent, g_swapchainFormat);
}

void WebGPUSwapchain::PresentFrame()
{
    if (!IsCreated() || m_currentTexture == nullptr)
    {
        return;
    }

    wgpuSurfacePresent(m_surface);

    ReleaseCurrentTexture();

    m_framebuffers[0]->SetExternalTextureView(nullptr, m_extent, g_swapchainFormat);
}

} // namespace Hyperion
