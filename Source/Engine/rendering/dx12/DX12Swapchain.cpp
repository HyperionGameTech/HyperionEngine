/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12Swapchain.hpp>
#include <rendering/dx12/DX12RenderInterface.hpp>
#include <rendering/dx12/DX12GpuImage.hpp>
#include <rendering/dx12/DX12Framebuffer.hpp>

#include <DX12Swapchain.generated.inl>

namespace Hyperion {

extern DX12RenderInterface* g_renderInterface;

static constexpr bool AllowTearingDefault = true;
static constexpr bool VSyncDefault = false;

#pragma region DX12Swapchain

DX12Swapchain::DX12Swapchain(HWND hwnd, const Vec2u& extent)
    : SwapchainBase(extent),
      m_hwnd(hwnd),
      m_currentBackBufferIndex(0),
      m_allowTearing(AllowTearingDefault),
      m_vsync(VSyncDefault)
{
}

DX12Swapchain::~DX12Swapchain()
{
    m_framebuffers.Clear();

    if (m_rtvDescriptorHandle.IsValid())
    {
        g_renderInterface->descriptorHeapManager->Free(DX12DescriptorHeapType::RTV, std::move(m_rtvDescriptorHandle));
    }

    for (ID3D12Resource* backBuffer : m_backBuffers)
    {
        /// since we don't use ComPtr for the backbuffers we need to call Release()
        backBuffer->Release();
    }
}

bool DX12Swapchain::IsCreated() const
{
    return m_swapChain != nullptr;
}

RendererResult DX12Swapchain::Create()
{
    if (IsCreated())
    {
        return {};
    }

    ID3D12Device* device = g_renderInterface->GetDevice();

    const DX12QueueData* queueData = g_renderInterface->GetQueueData(D3D12_COMMAND_LIST_TYPE_DIRECT);
    Assert(queueData != nullptr);

    ComPtr<IDXGIFactory4> factory = g_renderInterface->dxgiFactory;
    Assert(factory != nullptr);

    // Check tearing support
    ComPtr<IDXGIFactory5> factory5;
    if (SUCCEEDED(factory.As(&factory5)))
    {
        BOOL allowTearing = FALSE;
        if (SUCCEEDED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing))))
        {
            m_allowTearing = (allowTearing == TRUE);
        }
        else
        {
            m_allowTearing = false;
        }
    }
    else
    {
        m_allowTearing = false;
    }

    HYP_LOG(RenderingBackend, Info, "DX12 swapchain tearing support: {}", m_allowTearing);

    if (m_extent.Volume() == 0)
    {
        return HYP_MAKE_ERROR(RendererError, "Cannot create swapchain with zero extent");
    }

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc {};
    swapChainDesc.BufferCount = 3;
    swapChainDesc.Width = m_extent.x;
    swapChainDesc.Height = m_extent.y;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // @TODO Use proper swapchain format!!!
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Flags = m_allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;

    ComPtr<IDXGISwapChain1> swapChain1;
    HRESULT hr = factory->CreateSwapChainForHwnd(
        queueData->commandQueue.Get(),
        m_hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1
    );

    if (FAILED(hr))
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to create swapchain", hr);
    }

    if (FAILED(swapChain1.As(&m_swapChain)))
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to query IDXGISwapChain4");
    }

    m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Allocate RTV descriptors
    m_rtvDescriptorHandle = g_renderInterface->descriptorHeapManager->Allocate(DX12DescriptorHeapType::RTV, swapChainDesc.BufferCount);
    if (!m_rtvDescriptorHandle.IsValid())
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to allocate RTV descriptors for swapchain");
    }

    // Create RTVs
    m_backBuffers.Resize(swapChainDesc.BufferCount);
    m_rtvHandles.Resize(swapChainDesc.BufferCount);

    const uint32 rtvIncrement = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvDescriptorHandle.cpuHandle;

    for (uint32 i = 0; i < swapChainDesc.BufferCount; i++)
    {
        if (FAILED(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i]))))
        {
            return HYP_MAKE_ERROR(RendererError, "Failed to get swapchain back buffer");
        }

        device->CreateRenderTargetView(m_backBuffers[i], nullptr, rtvHandle);
        m_rtvHandles[i] = rtvHandle;

        rtvHandle.ptr += rtvIncrement;
    }

    // Create framebuffers for each back buffer
    m_framebuffers.Clear();
    m_framebuffers.Resize(swapChainDesc.BufferCount);

    for (uint32 i = 0; i < swapChainDesc.BufferCount; i++)
    {
        FramebufferDesc framebufferDesc {};
        framebufferDesc.extent = m_extent;
        framebufferDesc.renderPassMode = RenderPassMode::Present;

        DX12FramebufferRef framebuffer = g_renderInterface->MakeFramebuffer(framebufferDesc);
        Assert(framebuffer.IsValid());

        framebuffer->SetExternalRTVHandle(m_rtvHandles[i], m_backBuffers[i], m_extent.x, m_extent.y);
        CheckResultOrReturn(framebuffer->Create());

        m_framebuffers[i] = framebuffer;
    }

    m_acquiredImageIndex = m_currentBackBufferIndex;

    return {};
}

void DX12Swapchain::SetExtent(Vec2u newExtent)
{
    if (m_extent == newExtent)
    {
        return;
    }

    m_extent = newExtent;
    m_needsRecreate = true;
}

void DX12Swapchain::Recreate()
{
    if (!IsCreated())
    {
        return;
    }

    // Don't release or resize if extent is zero (e.g. window is minimized).
    // Wait for a valid extent before proceeding.
    if (m_extent.Volume() == 0)
    {
        return;
    }

    // Release back buffers before resizing
    for (ID3D12Resource* buffer : m_backBuffers)
    {
        buffer->Release();
    }

    m_backBuffers.Clear();

    HRESULT hr = m_swapChain->ResizeBuffers(
        0, // Preserve buffer count
        m_extent.x,
        m_extent.y,
        DXGI_FORMAT_UNKNOWN,
        m_allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0
    );

    if (FAILED(hr))
    {
        HYP_LOG(RenderingBackend, Error, "Failed to resize swapchain buffers: {}", hr);
        return;
    }

    m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Re-create RTVs
    // We can reuse the existing allocation since count didn't change
    ID3D12Device* device = g_renderInterface->GetDevice();
    const uint32 rtvIncrement = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvDescriptorHandle.cpuHandle;

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    m_swapChain->GetDesc1(&desc);

    m_backBuffers.Resize(desc.BufferCount);
    // m_rtvHandles is already sized, but we refresh values just in case
    
    for (uint32 i = 0; i < desc.BufferCount; i++)
    {
        if (FAILED(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i]))))
        {
            HYP_LOG(RenderingBackend, Error, "Failed to get swapchain back buffer during recreation");
            return;
        }

        device->CreateRenderTargetView(m_backBuffers[i], nullptr, rtvHandle);
        m_rtvHandles[i] = rtvHandle;

        rtvHandle.ptr += rtvIncrement;
    }

    // Recreate framebuffers for each back buffer
    m_framebuffers.Clear();
    m_framebuffers.Resize(desc.BufferCount);

    for (uint32 i = 0; i < desc.BufferCount; i++)
    {
        FramebufferDesc framebufferDesc {};
        framebufferDesc.extent = m_extent;
        framebufferDesc.renderPassMode = RenderPassMode::Present;

        DX12FramebufferRef framebuffer = g_renderInterface->MakeFramebuffer(framebufferDesc);
        Assert(framebuffer.IsValid());

        framebuffer->SetExternalRTVHandle(m_rtvHandles[i], m_backBuffers[i], m_extent.x, m_extent.y);
        if (!framebuffer->Create())
        {
            HYP_LOG(RenderingBackend, Error, "Failed to create swapchain framebuffer during recreation");
            return;
        }

        m_framebuffers[i] = framebuffer;
    }

    m_acquiredImageIndex = m_currentBackBufferIndex;

    m_needsRecreate = false;
}

void DX12Swapchain::PrepareForFrame(DX12Frame* frame)
{
    if (m_needsRecreate)
    {
        Recreate();
    }
}

void DX12Swapchain::PresentFrame(DX12Frame* frame)
{
    if (!IsCreated())
    {
        return;
    }

    UINT syncInterval = m_vsync ? 1 : 0;
    UINT flags = 0;

    if (m_allowTearing && !m_vsync)
    {
        flags = DXGI_PRESENT_ALLOW_TEARING;
    }

    static bool s_loggedPresentInfo = false;
    if (!s_loggedPresentInfo)
    {
        HYP_LOG(RenderingBackend, Info, "DX12 Present params — vsync: {}, tearing: {}, syncInterval: {}, flags: {}",
            m_vsync, m_allowTearing, syncInterval, flags);
        s_loggedPresentInfo = true;
    }

    HRESULT hr = m_swapChain->Present(syncInterval, flags);

    if (FAILED(hr) && hr != DXGI_ERROR_WAS_STILL_DRAWING)
    {
        HYP_LOG(RenderingBackend, Error, "Failed to present swapchain! Error: {}", hr);
        return;
    }

    m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
    m_acquiredImageIndex = m_currentBackBufferIndex;
}

#pragma endregion DX12Swapchain

} // namespace Hyperion
