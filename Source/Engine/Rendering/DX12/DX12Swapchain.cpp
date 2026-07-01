/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <DX12Pch.hpp>

#include <Rendering/DX12/DX12Swapchain.hpp>
#include <Rendering/DX12/DX12RenderInterface.hpp>
#include <Rendering/DX12/DX12GpuImage.hpp>
#include <Rendering/DX12/DX12Framebuffer.hpp>
#include <Rendering/DX12/DX12Helpers.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Rendering/CrashHandler.hpp>

#include <Framework/CVarManager.hpp>

#include <DX12Swapchain.generated.inl>

namespace Hyperion {

extern CVar<bool> g_cvEnableVSync;

extern DX12RenderInterface RI;

static constexpr bool AllowTearingDefault = true;

#pragma region DX12Swapchain

DX12Swapchain::DX12Swapchain(HWND hwnd, const Vec2u& extent)
    : SwapchainBase(extent),
      m_hwnd(hwnd),
      m_currentBackBufferIndex(0),
      m_allowTearing(AllowTearingDefault)
{
}

DX12Swapchain::~DX12Swapchain()
{
    Destroy();
}

void DX12Swapchain::Destroy()
{
    // Wait for GPU to be idle before destroying resources
    FlushGPU();

    m_framebuffers.Clear();

    if (m_rtvDescriptorHandle.IsValid())
    {
        RI.descriptorHeapManager->Free(DX12DescriptorHeapType::RTV, std::move(m_rtvDescriptorHandle));
    }

    for (ID3D12Resource* backBuffer : m_backBuffers)
    {
        backBuffer->Release();
    }

    m_backBuffers.Clear();
    m_rtvHandles.Clear();

    m_swapChain.Reset();

    m_currentBackBufferIndex = 0;

#ifdef HYP_DX12_USE_FRAME_LATENCY_WAITABLE
    if (m_frameLatencyWaitableObject != nullptr)
    {
        CloseHandle(m_frameLatencyWaitableObject);
        m_frameLatencyWaitableObject = nullptr;
    }
#endif

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

    ID3D12Device* device = RI.GetDevice();

    const DX12QueueData* queueData = RI.GetQueueData(D3D12_COMMAND_LIST_TYPE_DIRECT);
    Assert(queueData != nullptr);

    ComPtr<IDXGIFactory4> factory = RI.dxgiFactory;
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
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    if (m_allowTearing)
    {
        swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    }
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;

    ComPtr<IDXGISwapChain1> swapChain1;
    HRESULT hr = factory->CreateSwapChainForHwnd(
        queueData->commandQueue.Get(),
        m_hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1);

    if (FAILED(hr))
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to create swapchain", hr);
    }

    if (FAILED(swapChain1.As(&m_swapChain)))
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to query IDXGISwapChain4");
    }

#ifdef HYP_DX12_USE_FRAME_LATENCY_WAITABLE
    {
        ComPtr<IDXGISwapChain2> swapChain2;
        if (SUCCEEDED(m_swapChain.As(&swapChain2)))
        {
            m_frameLatencyWaitableObject = swapChain2->GetFrameLatencyWaitableObject();
        }
    }
#endif

    {
        ComPtr<IDXGIDevice1> dxgiDevice1;
        if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dxgiDevice1))))
        {
            HRESULT hr = dxgiDevice1->SetMaximumFrameLatency(1);
            if (FAILED(hr))
            {
                HYP_LOG(RenderingBackend, Warning, "Failed to set maximum frame latency: {}", hr);
            }
        }
    }

    m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Create flush fence (used when resizing to wait for GPU to finish)
    {
        HRESULT hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_flushFence));
        Assert(SUCCEEDED(hr));

        m_flushEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        Assert(m_flushEvent != nullptr);
    }

    // Allocate RTV descriptors
    m_rtvDescriptorHandle = RI.descriptorHeapManager->Allocate(DX12DescriptorHeapType::RTV, swapChainDesc.BufferCount);
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
#ifdef HYP_RHI_DEBUG_NAMES
        std::wstring name = L"D3D12 SwapChain Back Buffer " + std::to_wstring(i);
        m_backBuffers[i]->SetName(name.c_str());
#endif

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc {};
        rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;

        device->CreateRenderTargetView(m_backBuffers[i], &rtvDesc, rtvHandle);
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

        DX12FramebufferRef framebuffer = RI.MakeFramebuffer(framebufferDesc);
        Assert(framebuffer.IsValid());

        framebuffer->SetExternalRTVHandle(m_rtvHandles[i], m_backBuffers[i], m_extent, TextureFormat::RGBA8_SRGB);
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
    const DX12QueueData* queueData = RI.GetQueueData(D3D12_COMMAND_LIST_TYPE_DIRECT);
    if (!queueData || !queueData->commandQueue)
    {
        return;
    }

    HRESULT hr = RI.GetDevice()->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        __uuidof(ID3D12CommandAllocator),
        &m_flushAllocator);
    Assert(SUCCEEDED(hr));

    hr = RI.GetDevice()->CreateCommandList(
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

#ifdef HYP_DX12_USE_FRAME_LATENCY_WAITABLE
    if (g_cvEnableVSync.Get() && m_frameLatencyWaitableObject != nullptr)
    {
        WaitForSingleObject(m_frameLatencyWaitableObject, INFINITE);
    }
#endif

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

    const bool vsync = g_cvEnableVSync.Get();
    UINT syncInterval = vsync ? 1 : 0;
    UINT flags = 0;

    if (m_allowTearing && !vsync)
    {
        flags = DXGI_PRESENT_ALLOW_TEARING;
    }

    HRESULT hr = m_swapChain->Present(syncInterval, flags);

    if (FAILED(hr))
    {
        HYP_LOG(RenderingBackend, Error, "Failed to present swapchain! Error: {}", hr);

        // Check for device removal on device-related errors
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET || hr == DXGI_ERROR_DEVICE_HUNG)
        {
            CrashHandler::Dump();

            const char* deviceRemovedReason = CheckDeviceRemovedReason(RI.GetDevice());
            if (deviceRemovedReason)
            {
                HYP_LOG(RenderingBackend, Fatal, "Device removed: {}", deviceRemovedReason);
            }
        }

        return;
    }

    m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
    m_acquiredImageIndex = m_currentBackBufferIndex;
}

#pragma endregion DX12Swapchain

} // namespace Hyperion
