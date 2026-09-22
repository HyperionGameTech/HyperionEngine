/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Passes/DecalPass.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/Buffers.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/RawBufferAllocator.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/StencilMasks.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/MeshBuilder.hpp>

#include <Scene/Decal/DecalProxy.hpp>
#include <Scene/Camera/Camera.hpp>
#include <Rendering/RenderProxyList.hpp>

#include <Framework/View.hpp>

#include <DecalPass.generated.inl>

#include <algorithm>

namespace Hyperion {

struct DecalDrawBatch
{
    const RenderProxyDecalProxy* proxy;
    uint32 firstInstance;
};

#pragma region DecalPassData

DecalPassData::~DecalPassData()
{
    if (applyFramebuffer.IsValid())
    {
        EnqueueDeletion(std::move(applyFramebuffer));
    }

    if (resolveFramebuffer.IsValid())
    {
        EnqueueDeletion(std::move(resolveFramebuffer));
    }
}

#pragma endregion DecalPassData

#pragma region DecalPass

DecalPass::DecalPass()
{
}

void DecalPass::Initialize()
{
    m_cubeMesh = MeshBuilder::Cube();
    m_cubeMesh->SetName(NAME("DecalCube"));
    m_cubeMesh->SetIsTransient(true);
    m_cubeMesh->SetFlags(MeshFlags::ViewIndependent);
    m_cubeMesh->UploadGpuData();

    m_quadMesh = MeshBuilder::Quad();
    m_quadMesh->SetName(NAME("DecalResolveQuad"));
    m_quadMesh->SetIsTransient(true);
    m_quadMesh->SetFlags(MeshFlags::ViewIndependent);
    m_quadMesh->UploadGpuData();
}

void DecalPass::Shutdown()
{
    EnqueueDeletion(std::move(m_cubeMesh));
    EnqueueDeletion(std::move(m_quadMesh));
}

PassData* DecalPass::CreateViewPassData(View* view, PassDataExt&)
{
    DecalPassData* pd = new DecalPassData();
    pd->view = MakeWeakRef(view);

    return pd;
}

void DecalPass::UpdateFramebuffers(View* view, DecalPassData& passData)
{
    GBuffer* gbuffer = view->GetOutputTarget().GetGBuffer();
    Assert(gbuffer != nullptr);

    Framebuffer* opaqueFramebuffer = view->GetOutputTarget().GetFramebuffer(GBufferPass::Opaque);
    Assert(opaqueFramebuffer != nullptr);

    // the gbuffer recreates its framebuffers on resize, so ours would be holding views of the old images
    if (passData.applyFramebuffer.IsValid() && passData.sourceFramebuffer == opaqueFramebuffer && passData.extent == gbuffer->GetExtent())
    {
        return;
    }

    if (passData.applyFramebuffer.IsValid())
    {
        EnqueueDeletion(std::move(passData.applyFramebuffer));
    }

    if (passData.resolveFramebuffer.IsValid())
    {
        EnqueueDeletion(std::move(passData.resolveFramebuffer));
    }

    passData.extent = gbuffer->GetExtent();
    passData.sourceFramebuffer = opaqueFramebuffer;

    auto addSharedAttachment = [opaqueFramebuffer](Framebuffer* framebuffer, uint32 sourceBinding, uint32 binding, const BlendFunction& blendFunction)
    {
        AttachmentBase* sourceAttachment = opaqueFramebuffer->GetAttachment(sourceBinding);
        Assert(sourceAttachment != nullptr);

        AttachmentDesc attachmentDesc = sourceAttachment->GetAttachmentDesc();
        attachmentDesc.loadOp = LoadOperation::Load;
        attachmentDesc.storeOp = StoreOperation::Store;

        Attachment* attachment = framebuffer->AddAttachment(binding, attachmentDesc, RI.MakeImageView(sourceAttachment->GetGpuImage()));
        attachment->SetBlendFunction(blendFunction);
    };

    // stencil only, so the depth aspect can still be sampled while this framebuffer is bound
    auto addStencilAttachment = [opaqueFramebuffer](Framebuffer* framebuffer, uint32 binding)
    {
        const GpuImageViewRef& depthImageView = opaqueFramebuffer->GetAttachment(GBufferTarget::Depth)->GetImageView();
        Assert(depthImageView.IsValid());

        AttachmentDesc depthAttachmentDesc {};
        depthAttachmentDesc.imageType = TextureType::Texture2D;
        depthAttachmentDesc.format = depthImageView->GetImage()->GetTextureFormat();
        depthAttachmentDesc.loadOp = LoadOperation::Load;
        depthAttachmentDesc.storeOp = StoreOperation::Store;
        depthAttachmentDesc.onlyStencil = true;

        framebuffer->AddAttachment(binding, depthAttachmentDesc, depthImageView);
    };

    const TextureFormat normalsFormat = opaqueFramebuffer->GetAttachment(GBufferTarget::Normals)->GetAttachmentDesc().format;

    { // apply
        FramebufferDesc framebufferDesc {};
        framebufferDesc.extent = passData.extent;

        passData.applyFramebuffer = RI.MakeFramebuffer(framebufferDesc);
#ifdef HYP_RHI_DEBUG_NAMES
        passData.applyFramebuffer->SetDebugName(NAME("DecalApplyFramebuffer"));
#endif

        // albedo alpha holds AO - keep it
        addSharedAttachment(passData.applyFramebuffer, GBufferTarget::Color, 0, BlendFunction(BlendModeFactor::SrcAlpha, BlendModeFactor::OneMinusSrcAlpha, BlendModeFactor::Zero, BlendModeFactor::One));

        // LOAD + explicit clear: a mid-pass restart for a resource transition must not wipe what earlier batches accumulated
        Attachment* normalAccumAttachment = passData.applyFramebuffer->AddAttachment(1, AttachmentDesc { TextureType::Texture2D, TextureFormat::RGBA16F, LoadOperation::Load, StoreOperation::Store });
        normalAccumAttachment->SetBlendFunction(BlendFunction(BlendModeFactor::One, BlendModeFactor::OneMinusSrcAlpha, BlendModeFactor::One, BlendModeFactor::OneMinusSrcAlpha));

        // every decal writes the same (base) value per pixel so plain overwrite is fine; only read where the stencil is marked
        Attachment* baseNormalsAttachment = passData.applyFramebuffer->AddAttachment(2, AttachmentDesc { TextureType::Texture2D, normalsFormat, LoadOperation::Load, StoreOperation::Store });
        baseNormalsAttachment->SetBlendFunction(BlendFunction::Default());

        addStencilAttachment(passData.applyFramebuffer, 3);

        Check(passData.applyFramebuffer->Create());

#ifdef HYP_RHI_DEBUG_NAMES
        normalAccumAttachment->GetGpuImage()->SetDebugName(NAME("DecalNormalAccum"));
        baseNormalsAttachment->GetGpuImage()->SetDebugName(NAME("DecalBaseNormals"));
#endif
    }

    { // resolve
        FramebufferDesc framebufferDesc {};
        framebufferDesc.extent = passData.extent;

        passData.resolveFramebuffer = RI.MakeFramebuffer(framebufferDesc);
#ifdef HYP_RHI_DEBUG_NAMES
        passData.resolveFramebuffer->SetDebugName(NAME("DecalResolveFramebuffer"));
#endif

        addSharedAttachment(passData.resolveFramebuffer, GBufferTarget::Normals, 0, BlendFunction::Default());
        addStencilAttachment(passData.resolveFramebuffer, 1);

        Check(passData.resolveFramebuffer->Create());
    }
}

void DecalPass::RenderFrame(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    View* view = renderSetup.view;
    Assert(view != nullptr);

    if (!m_cubeMesh || !m_quadMesh)
    {
        return;
    }

    RenderProxyList& rpl = GetConsumerProxyList(view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    if (!rpl.GetDecalProxies().NumCurrent())
    {
        return;
    }

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(view->GetCamera()));

    if (!cameraProxy)
    {
        return;
    }

    Array<DecalDrawBatch> batches;
    uint32 numInstances = 0;

    for (DecalProxy* decalProxy : rpl.GetDecalProxies())
    {
        const RenderProxyDecalProxy* proxy = static_cast<const RenderProxyDecalProxy*>(GetRenderProxy(decalProxy));

        if (!proxy || proxy->instances.Empty())
        {
            continue;
        }

        batches.PushBack(DecalDrawBatch { proxy, numInstances });
        numInstances += uint32(proxy->instances.Size());
    }

    if (batches.Empty())
    {
        return;
    }

    std::stable_sort(batches.Begin(), batches.End(), [](const DecalDrawBatch& lhs, const DecalDrawBatch& rhs)
        {
            return lhs.proxy->sortOrder < rhs.proxy->sortOrder;
        });

    StructuredBuffer& instanceBuffer = RI.bufferAllocator->AcquireStructuredBuffer(numInstances, sizeof(DecalInstanceShaderData));

    uint32 instanceOffset = 0;

    for (DecalDrawBatch& batch : batches)
    {
        batch.firstInstance = instanceOffset;

        const uint32 batchSize = uint32(batch.proxy->instances.Size());

        instanceBuffer.Write(instanceOffset * sizeof(DecalInstanceShaderData), batchSize * sizeof(DecalInstanceShaderData), batch.proxy->instances.Data());
        instanceOffset += batchSize;
    }

    instanceBuffer.FlushBatched();

    DecalPassData* pd = DynamicCast<DecalPassData>(FetchViewPassData(view));
    AssertDebug(pd != nullptr);

    UpdateFramebuffers(view, *pd);

    Framebuffer* opaqueFramebuffer = view->GetOutputTarget().GetFramebuffer(GBufferPass::Opaque);

    const GpuImageViewRef& placeholderView = RI.textureViewCache->GetOrCreate(RI.placeholderData->textureSolidWhite);

    CommandRecorder& cr = frame->cr;

    cr << SetCurrentViewport(renderSetup.viewport);
    cr << SetFillMode(FillMode::Fill);
    cr << SetDepthTest(false);
    cr << SetDepthWrite(false);
    // per-attachment blend functions decide
    cr << SetCurrentBlendFunction(BlendFunction::None());

    { // apply
        cr << SetCurrentFramebuffer(pd->applyFramebuffer);
        cr << ClearFramebuffer(pd->applyFramebuffer, 0x2);

        cr << SetInputLayout(m_cubeMesh->GetMeshAttributes().inputLayout);
        cr << SetTopology(m_cubeMesh->GetMeshAttributes().topology);
        // back faces only, so the box still covers the screen with the camera inside it
        cr << SetFaceCullMode(FaceCullMode::Front);

        cr << SetStencilTest(true);
        cr << SetStencilFunction(StencilFunction { StencilOp::Replace, StencilOp::Keep, StencilOp::Keep, StencilCompareOp::Always });
        cr << SetStencilState(DecalStencilMask, 0x0, DecalStencilMask);

        cr << SetCurrentShader(ShaderDesc(NAME("Decal")));

        uint32 numShaderUniforms = 0;

        cr << SetShaderUniform(numShaderUniforms++, "GBufferNormalsTexture"_sh, opaqueFramebuffer->GetAttachment(GBufferTarget::Normals)->GetImageView());
        cr << SetShaderUniform(numShaderUniforms++, "GBufferMaterialTexture"_sh, opaqueFramebuffer->GetAttachment(GBufferTarget::MatData)->GetImageView());
        cr << SetShaderUniform(numShaderUniforms++, "GBufferDepthTexture"_sh, opaqueFramebuffer->GetAttachment(GBufferTarget::Depth)->GetImageView());
        cr << SetShaderUniform(numShaderUniforms++, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
        cr << SetShaderUniform(numShaderUniforms++, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinearMipmap());
        cr << SetShaderUniform(numShaderUniforms++, "DecalInstanceBuffer"_sh, instanceBuffer);

        for (const DecalDrawBatch& batch : batches)
        {
            DecalTypeShaderData typeShaderData = batch.proxy->bufferData;
            typeShaderData.firstInstance = batch.firstInstance;

            GpuBuffer* cbuffer = nullptr;
            size_t cbufferOffset = 0;
            size_t cbufferSize = 0;

            RI.cbufferAllocator->Write(&cameraProxy->bufferData);
            RI.cbufferAllocator->Write(&typeShaderData);
            RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

            uint32 batchShaderUniforms = numShaderUniforms;

            cr << SetShaderUniform(batchShaderUniforms++, "DecalAlbedoTexture"_sh, batch.proxy->albedoTexture != nullptr ? RI.textureViewCache->GetOrCreate(batch.proxy->albedoTexture) : placeholderView);
            cr << SetShaderUniform(batchShaderUniforms++, "DecalNormalTexture"_sh, batch.proxy->normalTexture != nullptr ? RI.textureViewCache->GetOrCreate(batch.proxy->normalTexture) : placeholderView);
            cr << SetShaderUniform(batchShaderUniforms++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

            cr << CommitDrawState();

            cr << BindVertexBuffer(m_cubeMesh->GetVertexBuffer(0));
            cr << BindIndexBuffer(m_cubeMesh->GetIndexBuffer(0));
            cr << DrawIndexed(m_cubeMesh->NumIndices(0), uint32(batch.proxy->instances.Size()));
        }

        cr << SetCurrentFramebuffer(nullptr);
    }

    { // resolve
        cr << SetCurrentFramebuffer(pd->resolveFramebuffer);

        cr << SetInputLayout(m_quadMesh->GetMeshAttributes().inputLayout);
        cr << SetTopology(m_quadMesh->GetMeshAttributes().topology);
        cr << SetFaceCullMode(FaceCullMode::Back);

        cr << SetStencilTest(true);
        cr << SetStencilFunction(StencilFunction { StencilOp::Keep, StencilOp::Keep, StencilOp::Keep, StencilCompareOp::Equal });
        cr << SetStencilState(DecalStencilMask, DecalStencilMask, 0x0);

        cr << SetCurrentShader(ShaderDesc(NAME("DecalResolve")));

        uint32 numShaderUniforms = 0;

        cr << SetShaderUniform(numShaderUniforms++, "DecalNormalAccumTexture"_sh, pd->applyFramebuffer->GetAttachment(1)->GetImageView());
        cr << SetShaderUniform(numShaderUniforms++, "DecalBaseNormalsTexture"_sh, pd->applyFramebuffer->GetAttachment(2)->GetImageView());

        cr << CommitDrawState();

        cr << BindVertexBuffer(m_quadMesh->GetVertexBuffer(0));
        cr << BindIndexBuffer(m_quadMesh->GetIndexBuffer(0));
        cr << DrawIndexed(6);

        cr << SetCurrentFramebuffer(nullptr);
    }

    // back to defaults for whatever draws next
    cr << SetStencilTest(false);
    cr << SetStencilState(0, 0xFF, 0x0);
    cr << SetFaceCullMode(FaceCullMode::Back);
    cr << SetDepthTest(true);
    cr << SetDepthWrite(true);
}

#pragma endregion DecalPass

} // namespace Hyperion
