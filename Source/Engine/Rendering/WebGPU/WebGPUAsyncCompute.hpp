/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/AsyncCompute.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Rendering/WebGPU/WebGPUShared.hpp>

#include <Core/Threading/AtomicVar.hpp>

namespace Hyperion {

class WebGPURenderInterface;
class WebGPUCommandBuffer;

class WebGPUAsyncCompute final : public AsyncComputeBase
{
    friend class WebGPURenderInterface;

public:
    WebGPUAsyncCompute();
    ~WebGPUAsyncCompute() override;

    bool IsSupported() const override
    {
        return false;
    }

    bool CheckStatus() override;
    void Create() override;

    HYP_FORCE_INLINE WebGPUCommandBuffer* GetCommandBuffer() const
    {
        return m_commandBuffer;
    }

private:
    void Submit();

    static void OnWorkDone(WGPUQueueWorkDoneStatus status, WGPUStringView message, void* userdata1, void* userdata2);

    WebGPUCommandBuffer* m_commandBuffer;
    AtomicVar<bool> m_isPending;
};

} // namespace Hyperion
