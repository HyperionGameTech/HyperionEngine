/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/AsyncCompute.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/dx12/DX12Shared.hpp>
#include <rendering/dx12/DX12Fence.hpp>

namespace Hyperion {

class DX12RenderInterface;
class DX12CommandBuffer;

class DX12AsyncCompute final : public AsyncComputeBase
{
    friend class DX12RenderInterface;

public:
    DX12AsyncCompute();
    ~DX12AsyncCompute() override;

    bool IsSupported() const override
    {
        return m_isSupported;
    }

    bool CheckStatus() override;

    void Create() override;

    HYP_FORCE_INLINE DX12CommandBuffer* GetCommandBuffer() const
    {
        return m_commandBuffer;
    }

    HYP_FORCE_INLINE DX12Fence* GetFence() const
    {
        return m_fence;
    }

private:
    void Submit();

    DX12CommandBuffer* m_commandBuffer;
    DX12Fence* m_fence;
    D3D12_COMMAND_LIST_TYPE m_commandListType;

    bool m_isSupported : 1;
    bool m_isSubmitted : 1;
};

} // namespace Hyperion
