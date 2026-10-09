/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <WebGPUPch.hpp>

#include <Rendering/WebGPU/WebGPUFrame.hpp>
#include <Rendering/WebGPU/WebGPUCommandBuffer.hpp>
#include <Rendering/WebGPU/WebGPURenderInterface.hpp>
#include <Rendering/WebGPU/WebGPUPipelineCompiler.hpp>
#include <Rendering/WebGPU/WebGPUFramebuffer.hpp>

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

    // See WebGPUPipelineCompileMode::WithFrame. A pass that had to start compiling pipelines left their draws out, so
    // once they are in, what it encoded is thrown away and the commands are executed again. The last pass allowed
    // compiles the plain way, which leaves nothing out.
    static constexpr uint32 maxNumPasses = 3;

    WebGPUPipelineCompiler& pipelineCompiler = RI.GetPipelineCompiler();

    const WebGPUCommandBuffer::SectionMark sectionMark = commandBuffer->BeginDiscardableSection();
    const RenderInterface::State stateBefore = RI.state;

    for (uint32 passIndex = 0; passIndex < maxNumPasses; passIndex++)
    {
        pipelineCompiler.BeginFramePass(/* allowCompilingWithFrame */ passIndex + 1 != maxNumPasses);

        for (CommandRecorder* commandRecorder : commandRecorders)
        {
            // the custom commands hand payloads over to the end of the frame, once
            commandRecorder->Execute(commandBuffer, /* keepCommands */ true, /* skipCustomCommands */ passIndex != 0);
        }

        pipelineCompiler.EndFramePass();

        if (pipelineCompiler.GetNumFrameCompilesStarted() == 0)
        {
            break;
        }

        while (pipelineCompiler.GetNumFrameCompilesRunning() != 0 && !RI.IsDeviceLost())
        {
            RI.WaitForEvents();
        }

        if (RI.state.boundFramebuffer != nullptr)
        {
            RI.state.boundFramebuffer->EndCapture(commandBuffer);
        }

        commandBuffer->DiscardSection(sectionMark);

        RI.state = stateBefore;
    }

    for (CommandRecorder* commandRecorder : commandRecorders)
    {
        commandRecorder->Reset(/* freeMemory */ false);
    }
}

void WebGPUFrame::ResetTransientStates()
{
}

} // namespace Hyperion
