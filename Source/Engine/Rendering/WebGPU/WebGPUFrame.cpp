/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <WebGPUPch.hpp>

#include <Rendering/WebGPU/WebGPUFrame.hpp>
#include <Rendering/WebGPU/WebGPUCommandBuffer.hpp>
#include <Rendering/WebGPU/WebGPURenderInterface.hpp>

#include <WebGPUFrame.generated.inl>

namespace Hyperion {

extern WebGPURenderInterface RI;

WebGPUFrame::WebGPUFrame()
    : FrameBase(0)
{
}

WebGPUFrame::WebGPUFrame(uint32 frameIndex)
    : FrameBase(frameIndex)
{
}

WebGPUFrame::~WebGPUFrame()
{
}

bool WebGPUFrame::IsCreated() const
{
    return true;
}

RendererResult WebGPUFrame::Create()
{
    return {};
}

void WebGPUFrame::OnFrameStart()
{
    FrameBase::OnFrameStart();
}

void WebGPUFrame::WriteCommandBuffer(CommandBuffer* commandBuffer)
{
    AssertOnThread(g_renderThread);

    Array<CommandRecorder*, RenderAllocator> commandRecorders;
    commandRecorders.Reserve(5);

    commandRecorders.PushBack(&preRenderCommands);
    commandRecorders.PushBack(&RI.commandRecorderAllocator.rootPreRender);
    commandRecorders.PushBack(&cr);
    commandRecorders.PushBack(&RI.commandRecorderAllocator.root);
    commandRecorders.PushBack(&postRenderCommands);

    if (OnPresent.AnyBound())
    {
        OnPresent(this);
        OnPresent.RemoveAllDetached();
    }

    for (CommandRecorder* commandRecorder : commandRecorders)
    {
        commandRecorder->Execute(commandBuffer);
        commandRecorder->Reset(/* freeMemory */ false);
    }
}

void WebGPUFrame::ResetTransientStates()
{
}

} // namespace Hyperion
