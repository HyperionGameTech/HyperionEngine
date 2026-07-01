/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <DX12Pch.hpp>

#include <Rendering/DX12/DX12Fence.hpp>
#include <Rendering/DX12/DX12RenderInterface.hpp>

#include <Rendering/CrashHandler.hpp>

#include <DX12Fence.generated.inl>

namespace Hyperion {

extern DX12RenderInterface RI;

#pragma region DX12Fence

DX12Fence::DX12Fence()
    : m_eventHandle(nullptr),
      m_value(0),
      isSubmitted(false)
{
}
DX12Fence::DX12Fence(DX12Fence&& other) noexcept
    : m_fence(other.m_fence),
      m_eventHandle(other.m_eventHandle),
      m_value(other.m_value),
      isSubmitted(other.isSubmitted)
{
    other.m_fence = nullptr;
    other.m_eventHandle = nullptr;
    other.m_value = 0;
    other.isSubmitted = false;

#ifdef HYP_RHI_DEBUG_NAMES
    m_debugName = std::move(other.m_debugName);
#endif
}

DX12Fence& DX12Fence::operator=(DX12Fence&& other) noexcept
{
    if (this != &other)
    {
        if (m_eventHandle != nullptr)
        {
            CloseHandle(m_eventHandle);
        }

        m_fence = other.m_fence;
        m_eventHandle = other.m_eventHandle;
        m_value = other.m_value;
        isSubmitted = other.isSubmitted;

        other.m_fence = nullptr;
        other.m_eventHandle = nullptr;
        other.m_value = 0;
        other.isSubmitted = false;
#ifdef HYP_RHI_DEBUG_NAMES
        m_debugName = std::move(other.m_debugName);
#endif
    }

    return *this;
}

DX12Fence::~DX12Fence()
{
    if (m_eventHandle != nullptr)
    {
        CloseHandle(m_eventHandle);
        m_eventHandle = nullptr;
    }
}

uint64 DX12Fence::GetCompletedValue() const
{
    if (!m_fence)
    {
        return 0;
    }

    return m_fence->GetCompletedValue();
}

bool DX12Fence::CheckStatus()
{
    if (HYP_UNLIKELY(!m_fence))
    {
        return false;
    }

    if (!isSubmitted)
    {
        return false;
    }

    return (m_fence->GetCompletedValue() >= m_value);
}

RendererResult DX12Fence::Create()
{
    Assert(m_fence == nullptr);
    Assert(m_eventHandle == nullptr);

    m_value = 0;

    m_eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    if (m_eventHandle == nullptr)
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to create wait event for D3D12 fence");
    }

    HRESULT hr = RI.GetDevice()->CreateFence(
        m_value,
        D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&m_fence));

    if (FAILED(hr))
    {
        CloseHandle(m_eventHandle);
        m_eventHandle = nullptr;

        return HYP_MAKE_ERROR(RendererError, "Failed to create D3D12 fence", hr);
    }
#ifdef HYP_RHI_DEBUG_NAMES
    if (m_debugName.Length() > 0)
    {
        m_fence->SetName(*m_debugName);
    }
#endif

    return {};
}

RendererResult DX12Fence::Wait(bool timeoutLoop)
{
    Assert(m_fence != nullptr);
    Assert(m_eventHandle != nullptr);
    Assert(isSubmitted);

    if (m_fence->GetCompletedValue() >= m_value)
    {
        isSubmitted = false;
        return {};
    }

    HRESULT hr = m_fence->SetEventOnCompletion(m_value, m_eventHandle);

    if (FAILED(hr))
    {
        CrashHandler::Dump();

        return HYP_MAKE_ERROR(RendererError, "Failed to set D3D12 fence completion event", hr);
    }

    DWORD waitResult;

    do
    {
        waitResult = WaitForSingleObject(m_eventHandle, timeoutLoop ? 100 : INFINITE);
    }
    while (timeoutLoop && waitResult == WAIT_TIMEOUT);

    if (waitResult != WAIT_OBJECT_0)
    {
        CrashHandler::Dump();

        return HYP_MAKE_ERROR(RendererError, "Failed while waiting for D3D12 fence completion");
    }

    isSubmitted = false;

    return {};
}

void DX12Fence::Increment()
{
    Assert(!isSubmitted);

    ++m_value;

    isSubmitted = true;
}

#ifdef HYP_RHI_DEBUG_NAMES
void DX12Fence::SetDebugName(const WideString& debugName)
{
    m_debugName = debugName;

    if (m_fence != nullptr)
    {
        m_fence->SetName(*debugName);
    }
}
#endif

#pragma endregion DX12Fence

} // namespace Hyperion
