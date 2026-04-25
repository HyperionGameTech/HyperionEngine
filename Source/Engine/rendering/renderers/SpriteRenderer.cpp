/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/RenderProxyList.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/Frame.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/MaterialInstance.hpp>
#include <rendering/Shader.hpp>
#include <rendering/CBufferAllocator.hpp>
#include <rendering/StructuredBufferAllocator.hpp>

#include <rendering/renderers/SpriteRenderer.hpp>

#include <util/MeshBuilder.hpp>

#include <scene/View.hpp>
#include <scene/Sprite.hpp>
#include <scene/camera/Camera.hpp> // Needed for .Get()

#include <asset/AssetRegistry.hpp>

#include <Core/utilities/DeferredScope.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/CVarManager.hpp>
#include <Engine/EngineGlobals.hpp>

#include <SpriteRenderer.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Sprite);

struct SpriteInstanceData
{
    Vec4f positionSize;
    Vec4f color;
    Vec4u flags; // x = alwaysFaceCamera
};

#pragma region SpriteRenderer

SpriteRenderer::SpriteRenderer()
{
}

void SpriteRenderer::Initialize()
{
    m_quadMesh = MeshBuilder::Cube(); // Quad();
    m_quadMesh->SetName(NAME("SpriteMesh"));
    m_quadMesh->SetFlags(MeshFlags::ViewIndependent);
    m_quadMesh->SetIsTransient(true);
    InitObject(m_quadMesh);

    GetEngineAssetRegistry()->PutAsset(m_quadMesh);
}

void SpriteRenderer::Shutdown()
{
    EnqueueDeletion(std::move(m_quadMesh));
}

void SpriteRenderer::RenderFrame(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    Assert(renderSetup.view != nullptr);

    SpriteRendererPassData* pd = DynamicCast<SpriteRendererPassData>(FetchViewPassData(renderSetup.view));
    AssertDebug(pd != nullptr);

    RenderProxyList& rpl = GetConsumerProxyList(renderSetup.view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    if (!rpl.GetSprites().NumCurrent())
    {
        return;
    }

    Mesh* quadMesh = m_quadMesh;

    if (!quadMesh)
    {
        return;
    }

    uint32 numSprites = 0;
    for (Sprite* sprite : rpl.GetSprites())
    {
        if (sprite)
        {
            numSprites++;
        }
    }

    if (numSprites == 0)
    {
        return;
    }

    StructuredBuffer& instanceBuffer = g_renderInterface->sbufferAllocator->AcquireBuffer(numSprites, sizeof(SpriteInstanceData));

    size_t offset = 0;

    for (Sprite* sprite : rpl.GetSprites())
    {
        if (!sprite)
        {
            continue;
        }

        RenderProxySprite* spriteProxy = rpl.GetSprites().GetProxy(sprite->Id());
        if (!spriteProxy)
        {
            continue;
        }

        SpriteInstanceData data {};
        data.positionSize = Vec4f(sprite->GetWorldTranslation(), sprite->size);
        data.color = Vec4f(sprite->color);
        data.flags = Vec4u(spriteProxy->bufferData.alwaysFaceCamera, 0u, 0u, 0u);

        instanceBuffer.Write(offset, sizeof(SpriteInstanceData), &data);
        offset += sizeof(SpriteInstanceData);
    }

    instanceBuffer.Flush();

    CommandRecorder& cr = frame->cr;
    View* view = renderSetup.view;

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(view->GetCamera().Get()));
    Assert(cameraProxy != nullptr);

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    g_renderInterface->cbufferAllocator->Write(&cameraProxy->bufferData);
    g_renderInterface->cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    cr << SetCurrentViewport(renderSetup.viewport);
    cr << SetTopology(Topology::TOP_TRIANGLES);
    cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);

    ShaderDesc shaderDesc;
    shaderDesc.name = NAME("Sprite");

    cr << SetCurrentShader(shaderDesc);

    cr << SetShaderUniform(0, "SpriteInstanceBuffer"_sh, instanceBuffer.gpuBuffer);
    cr << SetShaderUniform(1, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

    cr << SetCurrentBlendFunction(BlendFunction::AlphaBlending()); // @TODO premul?
    cr << SetDepthTest(true);
    cr << SetDepthWrite(false);
    cr << SetFaceCullMode(FaceCullMode::FCM_NONE);

    cr << CommitDrawState();

    cr << BindVertexBuffer(quadMesh->GetVertexBuffer());
    cr << BindIndexBuffer(quadMesh->GetIndexBuffer());

    cr << DrawIndexed(quadMesh->NumIndices(), numSprites);
}

PassData* SpriteRenderer::CreateViewPassData(View* view, PassDataExt&)
{
    SpriteRendererPassData* pd = new SpriteRendererPassData();

    pd->view = MakeWeakRef(view);

    return pd;
}

#pragma endregion SpriteRenderer

} // namespace Hyperion