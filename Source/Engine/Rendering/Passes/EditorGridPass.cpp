/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Passes/EditorGridPass.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/RenderSetup.hpp>

#include <Framework/EngineStats.hpp>

#include <Scene/View.hpp>
#include <Scene/Camera/Camera.hpp>

namespace Hyperion {

static EngineStatGpuTimer s_statDrawEditorGrid("Rendering/GPU/EditorGrid");

#pragma region EditorGridPass

EditorGridPass::EditorGridPass()
    : FullScreenPass(FSP_EXTERNAL_RENDERTARGET)
{
    SetPassName(NAME("EditorGrid"));

    SetBlendFunction(BlendFunction::AlphaBlending());
}

EditorGridPass::~EditorGridPass()
{
}

void EditorGridPass::Create()
{
    AssertOnThread(g_renderThread);

    m_shaderDesc = ShaderDesc(NAME("EditorGrid"));

    FullScreenPass::Create();
}

void EditorGridPass::Render(Frame* frame, const RenderSetup& renderSetup)
{
    AssertDebug(renderSetup.world && renderSetup.view && renderSetup.framebuffer);

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(renderSetup.view->GetCamera()));

    if (!cameraProxy)
    {
        return;
    }

    CommandRecorder& cr = frame->cr;

    ENGINE_STAT_GPU_SCOPE(&s_statDrawEditorGrid, &cr);

    cr << SetFillMode(FM_FILL);
    cr << SetDepthWrite(false);
    cr << SetDepthTest(true);
    cr << SetStencilTest(false);
    cr << SetFaceCullMode(FCM_NONE);
    cr << SetTopology(TOP_TRIANGLES);
    cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);
    cr << SetCurrentBlendFunction(GetBlendFunction());

    cr << SetCurrentViewport(renderSetup.viewport);
    cr << SetCurrentFramebuffer(renderSetup.framebuffer);

    cr << SetCurrentShader(m_shaderDesc);

    cr << SetShaderUniform(0, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(renderSetup.view->GetCamera()));

    RenderFullScreenQuad(frame, renderSetup);

    cr << SetDepthTest(true);
    cr << SetDepthWrite(true);
    cr << SetCurrentBlendFunction(BlendFunction::None());

    m_isFirstFrame = false;
}

#pragma endregion EditorGridPass

} // namespace Hyperion
