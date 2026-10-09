/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/WebGPU/WebGPUShared.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Functional/Proc.hpp>

namespace Hyperion {

class WebGPUShaderInstance;

enum class WebGPUPipelineCompileMode : uint8
{
    /// Created with the plain call. The browser compiles it when it gets to it, holding up everything queued behind.
    Immediate,

    /// Created with the async call while the frame's recorded commands execute, so the browser compiles the frame's new
    /// pipelines side by side. What the pipeline would have drawn is left out, and WebGPUFrame executes the commands
    /// again once every one of them is in.
    WithFrame,

    /// Queued, and created with the async call once nothing else is being compiled. Its draws and dispatches are left
    /// out until then and nothing is executed again, so whoever uses it has to cope with a late start.
    InBackground
};

/// A pipeline for one set of bind group layouts. Heap allocated: a compile in progress holds on to it until it has
/// finished, whatever became of the pipeline object in the meantime.
template <class PipelineType>
struct WebGPUPipelineVariant
{
    uint64 key = 0;
    PipelineType pipeline = nullptr;
    WebGPUPipelineCompileMode compileMode = WebGPUPipelineCompileMode::Immediate;
    bool isCompiling = false;
    bool isOrphaned = false;
};

class WebGPUPipelineCompiler final
{
public:
    /// \param canStartLate whether the caller copes with InBackground
    WebGPUPipelineCompileMode GetCompileMode(const WebGPUShaderInstance& shaderInstance, bool canStartLate) const;

    /// Brackets one execution of the frame's recorded commands.
    void BeginFramePass(bool allowCompilingWithFrame);
    void EndFramePass();

    HYP_FORCE_INLINE uint32 GetNumFrameCompilesStarted() const
    {
        return m_numFrameCompilesStarted;
    }

    HYP_FORCE_INLINE uint32 GetNumFrameCompilesRunning() const
    {
        return m_numFrameCompilesRunning;
    }

    /// Queued and running.
    HYP_FORCE_INLINE uint32 GetNumBackgroundCompiles() const
    {
        return m_numBackgroundCompiles;
    }

    void OnCompileStarted(WebGPUPipelineCompileMode compileMode);
    void OnCompileFinished(WebGPUPipelineCompileMode compileMode);

    void QueueBackgroundCompile(Proc<bool()>&& start);

    void Update();

private:
    bool m_isFramePassCompiling = false;
    uint32 m_numFrameCompilesStarted = 0;
    uint32 m_numFrameCompilesRunning = 0;

    Array<Proc<bool()>, WebGPUAllocator> m_queuedBackgroundCompiles;

    uint32 m_numBackgroundCompiles = 0;
    
    bool m_isBackgroundCompileRunning = false;
    uint32 m_numQuietFrames = 0;
};

} // namespace Hyperion
