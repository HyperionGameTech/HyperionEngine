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
#include <rendering/dx12/DX12Helpers.hpp>

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
    Destroy();
}

void DX12Swapchain::Destroy()
{
    m_framebuffers.Clear();

    if (m_rtvDescriptorHandle.IsValid())
    {
        g_renderInterface->descriptorHeapManager->Free(DX12DescriptorHeapType::RTV, std::move(m_rtvDescriptorHandle));
    }

    for (ID3D12Resource* backBuffer : m_backBuffers)
    {
        backBuffer->Release();
    }

    m_backBuffers.Clear();
    m_rtvHandles.Clear();

    // Release the swapchain itself so Create() can make a fresh one
    m_swapChain.Reset();
    m_currentBackBufferIndex = 0;

    if (m_flushEvent != nullptr)
    {
        CloseHandle(m_flushEvent);
        m_flushEvent = nullptr;
    }

    m_flushFence.Reset();
    m_flushCommandList.Reset();
    m_flushAllocator.Reset();
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

    // IDXGISwapChain4 doesn't inherit ID3D12Object, so no SetName. The back buffers get debug names instead.

    m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Create flush fence (used when resizing to wait for GPU to finish)
    {
        HRESULT hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_flushFence));
        Assert(SUCCEEDED(hr));

        m_flushEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        Assert(m_flushEvent != nullptr);
    }

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

#ifdef HYP_DEBUG_MODE
        std::wstring name = L"D3D12 SwapChain Back Buffer " + std::to_wstring(i);
        m_backBuffers[i]->SetName(name.c_str());
#endif

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

    // Flush the GPU and wait for all pending operations before releasing resources.
    // This prevents ERROR #921 OBJECT_DELETED_WHILE_STILL_IN_USE.
    FlushGPU();

    // Destroy old resources
    Destroy();

    // Now recreate
    Create();

    m_needsRecreate = false;
}

void DX12Swapchain::FlushGPU()
{
    const DX12QueueData* queueData = g_renderInterface->GetQueueData(D3D12_COMMAND_LIST_TYPE_DIRECT);
    if (!queueData || !queueData->commandQueue)
    {
        return;
    }

    HRESULT hr = g_renderInterface->GetDevice()->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        __uuidof(ID3D12CommandAllocator),
        &m_flushAllocator);
    Assert(SUCCEEDED(hr));

    hr = g_renderInterface->GetDevice()->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_flushAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(&m_flushCommandList));
    Assert(SUCCEEDED(hr));

    m_flushCommandList->Close();

    ID3D12CommandList* commandLists[] = { m_flushCommandList.Get() };
    queueData->commandQueue->ExecuteCommandLists(1, commandLists);

    hr = queueData->commandQueue->Signal(m_flushFence.Get(), m_flushFenceValue + 1);
    Assert(SUCCEEDED(hr));
    m_flushFenceValue++;

    if (m_flushFence->GetCompletedValue() < m_flushFenceValue)
    {
        hr = m_flushFence->SetEventOnCompletion(m_flushFenceValue, m_flushEvent);
        Assert(SUCCEEDED(hr));

        WaitForSingleObject(m_flushEvent, INFINITE);
    }

    m_flushCommandList.Reset();
    m_flushAllocator.Reset();
}

void DX12Swapchain::PrepareForFrame(DX12Frame* frame)
{
    if (m_needsRecreate)
    {
        Recreate();
    }

    m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
    m_acquiredImageIndex = m_currentBackBufferIndex;

    m_framebuffers[m_currentBackBufferIndex]->ResetExternalRTResourceState();
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

        // Check for device removal on device-related errors
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET || hr == DXGI_ERROR_DEVICE_HUNG)
        {
            CheckDeviceRemovedReason(g_renderInterface->GetDevice());
        }

        return;
    }

    m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
    m_acquiredImageIndex = m_currentBackBufferIndex;
}

#pragma endregion DX12Swapchain

} // namespace Hyperion
