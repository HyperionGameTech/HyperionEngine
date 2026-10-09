/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <WebGPUPch.hpp>

#include <Rendering/WebGPU/WebGPUAsyncCompute.hpp>
#include <Rendering/WebGPU/WebGPUCommandBuffer.hpp>
#include <Rendering/WebGPU/WebGPURenderInterface.hpp>

#include <Core/Threading/Threads.hpp>

namespace Hyperion {

extern WebGPURenderInterface RI;

WebGPUAsyncCompute::WebGPUAsyncCompute()
    : m_commandBuffer(nullptr),
      m_isPending(false)
{
}

WebGPUAsyncCompute::~WebGPUAsyncCompute()
{
    while (!CheckStatus())
    {
        ThreadSleep(0);
    }

    if (m_commandBuffer != nullptr)
    {
        m_commandBuffer->Release();
        m_commandBuffer = nullptr;
    }
}

bool WebGPUAsyncCompute::CheckStatus()
{
    if (!m_isPending.Get(MemoryOrder::ACQUIRE))
    {
        return true;
    }

    RI.ProcessEvents();

    return !m_isPending.Get(MemoryOrder::ACQUIRE);
}

void WebGPUAsyncCompute::Create()
{
    m_commandBuffer = new WebGPUCommandBuffer();

    Check(m_commandBuffer->Create());
}

void WebGPUAsyncCompute::OnWorkDone(WGPUQueueWorkDoneStatus status, WGPUStringView message, void* userdata1, void* userdata2)
{
    static_cast<WebGPUAsyncCompute*>(userdata1)->m_isPending.Set(false, MemoryOrder::RELEASE);
}

void WebGPUAsyncCompute::Submit()
{
    Assert(CheckStatus(), "GPU work must be completed from previous submission before WebGPUAsyncCompute::Submit() is ever called!");

    m_commandBuffer->Begin();
    cr.Execute(m_commandBuffer);
    m_commandBuffer->End();

    m_isPending.Set(true, MemoryOrder::RELEASE);

    RI.Submit(*m_commandBuffer);

    WGPUQueueWorkDoneCallbackInfo callbackInfo = WGPU_QUEUE_WORK_DONE_CALLBACK_INFO_INIT;
    callbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    callbackInfo.callback = &WebGPUAsyncCompute::OnWorkDone;
    callbackInfo.userdata1 = this;

    wgpuQueueOnSubmittedWorkDone(RI.GetQueue(), callbackInfo);
}

} // namespace Hyperion
