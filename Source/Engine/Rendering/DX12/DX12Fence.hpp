/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderResult.hpp>
#include <Rendering/RenderMemory.hpp>

#include <Rendering/DX12/DX12Shared.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class DX12Fence final : public ObjectBase
{
    HYP_OBJECT_BODY(DX12Fence);

public:
    static Pool* GetAllocator()
    {
        return g_dx12Pool;
    }

    DX12Fence();

    DX12Fence(const DX12Fence&) = delete;
    DX12Fence& operator=(const DX12Fence&) = delete;

    DX12Fence(DX12Fence&& other) noexcept;
    DX12Fence& operator=(DX12Fence&& other) noexcept;

    ~DX12Fence() override;

    HYP_FORCE_INLINE ID3D12Fence* GetD3D12Fence() const
    {
        return m_fence.Get();
    }

    HYP_FORCE_INLINE uint64 GetValue() const
    {
        return m_value;
    }

    uint64 GetCompletedValue() const;

    bool CheckStatus();

    RendererResult Create();
    RendererResult Wait(bool timeoutLoop = false);
    void Increment();

#if HYP_RHI_DEBUG_NAMES
    void SetDebugName(const WideString& debugName);

    HYP_FORCE_INLINE const WideString& GetDebugName() const
    {
        return m_debugName;
    }
#endif

    bool isSubmitted;

private:
    ComPtr<ID3D12Fence> m_fence;
    HANDLE m_eventHandle;
    uint64 m_value;

#if HYP_RHI_DEBUG_NAMES
    WideString m_debugName;
#endif
};

} // namespace Hyperion
