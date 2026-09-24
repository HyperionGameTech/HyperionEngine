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
#include <Rendering/CBufferAllocator.hpp>

#include <Framework/EngineStats.hpp>
#include <Framework/View.hpp>

#include <Scene/Camera/Camera.hpp>

namespace Hyperion {

static EngineStatGpuTimer s_statDrawEditorGrid("Rendering/GPU/EditorGrid");

CVar<bool> g_cvEditorGrid { "Editor.ShowGrid", true };
CVar<float> g_cvEditorGridSize { "Editor.Grid.Size", 1.0f };
CVar<float> g_cvEditorGridOffsetX { "Editor.Grid.OffsetX", 0.0f };
CVar<float> g_cvEditorGridOffsetY { "Editor.Grid.OffsetY", 0.0f };
CVar<float> g_cvEditorGridOffsetZ { "Editor.Grid.OffsetZ", 0.0f };

// matches EditorGridConstants in Shaders/Editor/EditorGrid.hlsl
struct EditorGridConstants
{
    Vec4f gridOffsetAndSize;
};

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

    EditorGridConstants constants {};
    constants.gridOffsetAndSize = Vec4f(
        g_cvEditorGridOffsetX.Get(),
        g_cvEditorGridOffsetY.Get(),
        g_cvEditorGridOffsetZ.Get(),
        MathUtil::Max(g_cvEditorGridSize.Get(), MathUtil::epsilonF));

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    CommandRecorder& cr = frame->cr;

    ENGINE_STAT_GPU_SCOPE(&s_statDrawEditorGrid, &cr);

    cr << SetFillMode(FillMode::Fill);
    cr << SetDepthWrite(false);
    cr << SetDepthTest(true);
    cr << SetStencilTest(false);
    cr << SetFaceCullMode(FaceCullMode::None);
    cr << SetTopology(Topology::Triangles);
    cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);
    cr << SetCurrentBlendFunction(GetBlendFunction());

    cr << SetCurrentViewport(renderSetup.viewport);
    cr << SetCurrentFramebuffer(renderSetup.framebuffer);

    cr << SetCurrentShader(m_shaderDesc);

    cr << SetShaderUniform(0, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(renderSetup.view->GetCamera()));
    cr << SetShaderUniform(1, "EditorGridConstants"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

    RenderFullScreenQuad(frame, renderSetup);

    cr << SetDepthTest(true);
    cr << SetDepthWrite(true);
    cr << SetCurrentBlendFunction(BlendFunction::None());

    m_isFirstFrame = false;
}

#pragma endregion EditorGridPass

} // namespace Hyperion
