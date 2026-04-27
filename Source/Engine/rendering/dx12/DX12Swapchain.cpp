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
#include <rendering/dx12/DX12CommandBuffer.hpp>

#include <DX12Swapchain.generated.inl>

namespace Hyperion {

extern DX12RenderInterface* g_renderInterface;

#pragma region DX12Swapchain

DX12Swapchain::DX12Swapchain(HWND hwnd, const Vec2u& extent)
    : SwapchainBase(extent),
      m_hwnd(hwnd),
      m_currentBackBufferIndex(0),
      m_allowTearing(false),
      m_vsync(true)
{
}

DX12Swapchain::~DX12Swapchain()
{
    if (m_rtvDescriptorHandle.IsValid())
    {
        g_renderInterface->descriptorHeapManager->Free(DX12DescriptorHeapType::RTV, std::move(m_rtvDescriptorHandle));
    }

    // Clear framebuffers first (they reference the images)
    m_framebuffers.Clear();
    
    // Clear images (they hold references to the back buffer resources)
    m_images.Clear();
    
    // Note: We don't need to manually Release() back buffers anymore
    // because the DX12GpuImage wrappers now own them via ComPtr
    m_backBuffers.Clear();
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

    // Create RTVs, GpuImages, and Framebuffers
    m_backBuffers.Resize(swapChainDesc.BufferCount);
    m_rtvHandles.Resize(swapChainDesc.BufferCount);
    m_images.Clear();
    m_framebuffers.Clear();

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

        // Create GpuImage wrapper for state tracking
        TextureDesc textureDesc;
        textureDesc.type = TextureType::Texture2D;
        textureDesc.format = TextureFormat::RGBA8;
        textureDesc.extent = Vec3u { m_extent.x, m_extent.y, 1 };
        textureDesc.imageUsage = IU_ATTACHMENT | IU_SAMPLED;
        
        DX12GpuImageRef gpuImage = MakeHandle<DX12GpuImage>(textureDesc);
        gpuImage->SetDebugName(NAME_FMT("SwapchainBackBuffer_{}", i));
        
        // Set the existing resource and mark as created
        gpuImage->SetResource(m_backBuffers[i]);
        //gpuImage->MarkAsCreated();
        
        // Set initial state to PRESENT (swapchain back buffers start in this state)
        gpuImage->SetResourceState(ResourceState::RS_PRESENT);
        
        m_images.PushBack(gpuImage);

        // Create framebuffer for this back buffer
        FramebufferDesc framebufferDesc {};
        framebufferDesc.extent = m_extent;
        framebufferDesc.renderPassMode = RenderPassMode::Present;

        DX12FramebufferRef framebuffer = MakeHandle<DX12Framebuffer>(framebufferDesc);
        
        // Create image view for the attachment
        DX12GpuImageViewRef imageView = g_renderInterface->MakeImageView(gpuImage);
        
        framebuffer->AddAttachment(
            0,
            AttachmentDesc {
                TextureType::Texture2D,
                textureDesc.format,
                LoadOperation::CLEAR,
                StoreOperation::STORE
            },
            imageView);

        RendererResult fbResult = framebuffer->Create();
        if (!fbResult)
        {
            return HYP_MAKE_ERROR(RendererError, "Failed to create swapchain framebuffer");
        }

        m_framebuffers.PushBack(framebuffer);

        rtvHandle.ptr += rtvIncrement;
    }

    return {};
}

void DX12Swapchain::SetExtent(Vec2u newExtent)
{
    m_extent = newExtent;
}

void DX12Swapchain::Recreate()
{
    if (!IsCreated())
    {
        return;
    }

    // Clear framebuffers and images before resizing
    // (they hold references to the back buffer resources)
    m_framebuffers.Clear();
    m_images.Clear();
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

    // Re-create RTVs, GpuImages, and Framebuffers
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

        // Create GpuImage wrapper for state tracking
        TextureDesc textureDesc;
        textureDesc.type = TextureType::Texture2D;
        textureDesc.format = TextureFormat::RGBA8;
        textureDesc.extent = Vec3u { m_extent.x, m_extent.y, 1 };
        textureDesc.imageUsage = IU_ATTACHMENT | IU_SAMPLED;
        
        DX12GpuImageRef gpuImage = MakeHandle<DX12GpuImage>(textureDesc);
        gpuImage->SetDebugName(NAME_FMT("SwapchainBackBuffer_{}", i));
        
        // Set the existing resource and mark as created
        gpuImage->SetResource(m_backBuffers[i]);
        
        // Set initial state to PRESENT (swapchain back buffers start in this state)
        gpuImage->SetResourceState(ResourceState::RS_PRESENT);
        
        m_images.PushBack(gpuImage);

        // Create framebuffer for this back buffer
        FramebufferDesc framebufferDesc {};
        framebufferDesc.extent = m_extent;
        framebufferDesc.renderPassMode = RenderPassMode::Present;

        DX12FramebufferRef framebuffer = MakeHandle<DX12Framebuffer>(framebufferDesc);
        
        // Create image view for the attachment
        DX12GpuImageViewRef imageView = g_renderInterface->MakeImageView(gpuImage);
        
        framebuffer->AddAttachment(
            0,
            AttachmentDesc {
                TextureType::Texture2D,
                textureDesc.format,
                LoadOperation::CLEAR,
                StoreOperation::STORE
            },
            imageView);

        RendererResult fbResult = framebuffer->Create();
        if (!fbResult)
        {
            HYP_LOG(RenderingBackend, Error, "Failed to create swapchain framebuffer during recreation");
            return;
        }

        m_framebuffers.PushBack(framebuffer);

        rtvHandle.ptr += rtvIncrement;
    }
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
    UINT flags = (m_allowTearing && !m_vsync) ? DXGI_PRESENT_ALLOW_TEARING : 0;

    HRESULT hr = m_swapChain->Present(syncInterval, flags);

    if (FAILED(hr))
    {
        HYP_LOG(RenderingBackend, Error, "Failed to present swapchain! Error: {}", hr);
        // Handle device lost etc?
    }

    m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
}

#pragma endregion DX12Swapchain

} // namespace Hyperion
