/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <WebGPUPch.hpp>

#include <Rendering/WebGPU/WebGPUPipelineCompiler.hpp>
#include <Rendering/WebGPU/WebGPUShaderInstance.hpp>

#include <Rendering/Shader.hpp>

#include <Core/Threading/Threads.hpp>

namespace Hyperion {

// The browser compiles its shaders one at a time, a background compile included, so a pipeline the frame is waiting on
// gets stuck behind any that is running. Background compiles start once the others have stopped coming.
static constexpr uint32 g_numQuietFramesBeforeBackgroundCompile = 30;

// These ray traversal shaders take the browser seconds each to compile.
static const ANSIStringView g_backgroundCompiledShaderPrefix = "Glimmer";

WebGPUPipelineCompileMode WebGPUPipelineCompiler::GetCompileMode(const WebGPUShaderInstance& shaderInstance, bool canStartLate) const
{
    const ANSIStringView shaderName = shaderInstance.GetShader()->GetName().LookupString();

    if (canStartLate
        && shaderName.Size() >= g_backgroundCompiledShaderPrefix.Size()
        && Memory::StrCmp(shaderName.Data(), g_backgroundCompiledShaderPrefix.Data(), g_backgroundCompiledShaderPrefix.Size()) == 0)
    {
        return WebGPUPipelineCompileMode::InBackground;
    }

    // transient command buffers are recorded on other threads while the frame executes, and are not executed again
    if (m_isFramePassCompiling && IsOnThread(g_renderThread))
    {
        return WebGPUPipelineCompileMode::WithFrame;
    }

    return WebGPUPipelineCompileMode::Immediate;
}

void WebGPUPipelineCompiler::BeginFramePass(bool allowCompilingWithFrame)
{
    AssertOnThread(g_renderThread);

    m_isFramePassCompiling = allowCompilingWithFrame;
    m_numFrameCompilesStarted = 0;
}

void WebGPUPipelineCompiler::EndFramePass()
{
    m_isFramePassCompiling = false;
}

void WebGPUPipelineCompiler::OnCompileStarted(WebGPUPipelineCompileMode compileMode)
{
    switch (compileMode)
    {
    case WebGPUPipelineCompileMode::WithFrame:
        ++m_numFrameCompilesStarted;
        ++m_numFrameCompilesRunning;

        m_numQuietFrames = 0;
        break;
    case WebGPUPipelineCompileMode::InBackground:
        ++m_numBackgroundCompiles;
        break;
    default:
        m_numQuietFrames = 0;
        break;
    }
}

void WebGPUPipelineCompiler::OnCompileFinished(WebGPUPipelineCompileMode compileMode)
{
    switch (compileMode)
    {
    case WebGPUPipelineCompileMode::WithFrame:
        AssertDebug(m_numFrameCompilesRunning != 0);
        --m_numFrameCompilesRunning;
        break;
    case WebGPUPipelineCompileMode::InBackground:
        AssertDebug(m_numBackgroundCompiles != 0);
        --m_numBackgroundCompiles;

        m_isBackgroundCompileRunning = false;
        break;
    default:
        break;
    }
}

void WebGPUPipelineCompiler::QueueBackgroundCompile(Proc<bool()>&& start)
{
    m_queuedBackgroundCompiles.PushBack(std::move(start));
}

void WebGPUPipelineCompiler::Update()
{
    ++m_numQuietFrames;

    while (!m_isBackgroundCompileRunning
        && m_numFrameCompilesRunning == 0
        && m_numQuietFrames >= g_numQuietFramesBeforeBackgroundCompile
        && m_queuedBackgroundCompiles.Any())
    {
        Proc<bool()> start = m_queuedBackgroundCompiles.PopFront();

        if (start())
        {
            m_isBackgroundCompileRunning = true;
        }
        else
        {
            OnCompileFinished(WebGPUPipelineCompileMode::InBackground);
        }
    }
}

} // namespace Hyperion
