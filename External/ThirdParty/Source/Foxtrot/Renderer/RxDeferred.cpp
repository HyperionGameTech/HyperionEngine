#include "RxDeferred.hpp"

#include "Backend/RxDsLayoutBuilder.hpp"
#include "Backend/RxShader.hpp"
#include "Backend/RxVertexDescription.hpp"
#include "FxCamera.hpp"
#include "FxEngine.hpp"
#include "RxGlobals.hpp"
#include "RxPipelineBuilder.hpp"
#include "RxRenderBackend.hpp"
#include "RxShaderCache.hpp"
#include "RxState.hpp"

#include <FxObjectManager.hpp>


FX_SET_MODULE_NAME("DeferredRenderer")

void RxDeferredRenderer::Create(const FxVec2u& extent)
{
    DescriptorPool.AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10);
    DescriptorPool.AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10);
    DescriptorPool.AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 10);
    DescriptorPool.AddPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 5);
    DescriptorPool.AddPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10);
    DescriptorPool.Create(gRenderer->GetDevice(), 16);

    CreateGPassPipeline();
    CreateLightingPipeline();
    CreateCompPipeline();

    CreateUnlitPipeline();
}

void RxDeferredRenderer::Destroy()
{
    DestroyCompPipeline();
    DestroyGPassPipeline();
    DestroyLightingPipeline();
}

void RxDeferredRenderer::CreateGPass()
{
    GPass.Create(gRenderer->Swapchain.Extent);

    // Albedo target
    GPass.AddTarget(RxImageFormat::eBGRA8_UNorm, RxTarget::scFullScreen,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, RxImageAspectFlag::eColor);

    // Normals target
    GPass.AddTarget(RxImageFormat::eRGBA16_Float, RxTarget::scFullScreen,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, RxImageAspectFlag::eColor);

    // Depth target
    GPass.AddTarget(RxImageFormat::eD32_Float, RxTarget::scFullScreen,
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    RxImageAspectFlag::eDepth);

    GPass.BuildRenderStage();
}

/////////////////////////////////////
// FxRenderer GPass Functions
/////////////////////////////////////

VkPipelineLayout RxDeferredRenderer::CreateGPassPipelineLayout()
{
    // Material descriptor set
    {
        // Create material properties DS layout
        RxDsLayoutBuilder builder {};
        builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, RxShaderType::eFragment);
        DsLayoutLightingMaterialProperties = builder.Build();
    }


    {
        RxDsLayoutBuilder builder {};
        builder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RxShaderType::eFragment);
        builder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RxShaderType::eFragment);
        builder.AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RxShaderType::eFragment);
        // builder.AddBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, RxShaderType::eVertex);
        DsLayoutGPassMaterial = builder.Build();
    }

    {
        RxDsLayoutBuilder builder {};
        builder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RxShaderType::eFragment);
        DsLayoutGPassMaterialAlbedoOnly = builder.Build();
    }

    FxStackArray<VkDescriptorSetLayout, 3> ds_layouts = {
        DsLayoutGPassMaterial,
        DsLayoutLightingMaterialProperties,
        gObjectManager->DsLayoutObjectBuffer,
    };

    FxStackArray<RxPushConstants, 1> push_consts = {
        RxPushConstants {
            .Size = sizeof(FxDrawPushConstants),
            .StageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };

    VkPipelineLayout layout = RxPipeline::CreateLayout(FxSlice(push_consts), FxSlice(ds_layouts));

    RxUtil::SetDebugLabel("Geometry", VK_OBJECT_TYPE_PIPELINE_LAYOUT, layout);

    return layout;
}


VkPipelineLayout RxDeferredRenderer::CreateGPassSkinnedPipelineLayout()
{
    {
        RxDsLayoutBuilder builder {};
        builder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RxShaderType::eFragment);
        builder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RxShaderType::eFragment);
        builder.AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RxShaderType::eFragment);
        builder.AddBinding(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, RxShaderType::eVertex);
        DsLayoutGPassSkinned = builder.Build();
    }


    FxStackArray<VkDescriptorSetLayout, 3> ds_layouts = {
        DsLayoutGPassSkinned,
        DsLayoutLightingMaterialProperties,
        gObjectManager->DsLayoutObjectBuffer,
    };

    FxStackArray<RxPushConstants, 1> push_consts = {
        RxPushConstants {
            .Size = sizeof(FxDrawPushConstants),
            .StageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };

    VkPipelineLayout layout = RxPipeline::CreateLayout(FxSlice(push_consts), FxSlice(ds_layouts));

    RxUtil::SetDebugLabel("GeometrySkinned", VK_OBJECT_TYPE_PIPELINE_LAYOUT, layout);

    return layout;
}

VkPipelineLayout RxDeferredRenderer::CreateUnlitPipelineLayout()
{
    FxStackArray<VkDescriptorSetLayout, 3> ds_layouts = {
        DsLayoutGPassMaterial,
        DsLayoutLightingMaterialProperties,
        gObjectManager->DsLayoutObjectBuffer,
    };

    FxStackArray<RxPushConstants, 1> push_consts = {
        RxPushConstants {
            .Size = sizeof(FxDrawPushConstants),
            .StageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };

    VkPipelineLayout layout = RxPipeline::CreateLayout(FxSlice(push_consts), FxSlice(ds_layouts));

    RxUtil::SetDebugLabel("Unlit Pipeline Layout", VK_OBJECT_TYPE_PIPELINE_LAYOUT, layout);

    return layout;
}

void RxDeferredRenderer::CreateUnlitPipeline()
{
    VkPipelineLayout layout = CreateUnlitPipelineLayout();

    FxRef<RxShader> shader_unlit = gShaderCache->Request(RxShaderName::eUnlit);
    FxRef<RxShaderProgram> vertex_shader = shader_unlit->GetProgram(RxShaderType::eVertex, {});
    FxRef<RxShaderProgram> fragment_shader = shader_unlit->GetProgram(RxShaderType::eFragment, {});

    RxVertexDescription vertex_info = RxVertexUtil::BuildDescription<RxVertexType::eDefault>();

    RxPipelineBuilder builder {};
    RxTargetList attachments {};

    RxTarget* lp_light_attachment = LightPass.GetTarget(RxImageFormat::eRGBA16_Float);
    RxTarget* lp_depth_attachment = GPass.GetTarget(RxImageFormat::eD32_Float);

    FxAssert(lp_light_attachment != nullptr && lp_depth_attachment != nullptr);

    RxTarget depth_attachment(RxImageFormat::eD32_Float, gRenderer->Swapchain.Extent);
    depth_attachment.LoadOp = RxLoadOp::eLoad;
    depth_attachment.StoreOp = RxStoreOp::eDontCare;
    depth_attachment.Aspect = RxImageAspectFlag::eDepth;
    depth_attachment.Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    depth_attachment.InitialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    depth_attachment.SetImage(lp_depth_attachment->GetImage());
    attachments.Add(depth_attachment);

    RxTarget light_attachment(RxImageFormat::eRGBA16_Float, gRenderer->Swapchain.Extent);
    light_attachment.LoadOp = RxLoadOp::eLoad;
    light_attachment.StoreOp = RxStoreOp::eStore;
    light_attachment.InitialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    light_attachment.FinalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    light_attachment.SetImage(lp_light_attachment->GetImage());

    attachments.Add(light_attachment);


    RpForward.Create(attachments, RxTarget::scFullScreen);
    FbForward.Create(attachments.GetImageViews(), RpForward, RxTarget::scFullScreen);

    builder.SetLayout(layout)
        .SetName("Unlit Pipeline")
        .AddBlendAttachment({ .Enabled = false })
        .SetAttachments(&attachments)
        .SetShaders(vertex_shader, fragment_shader)
        .SetRenderPass(&RpForward)
        .SetVertexDescription(&vertex_info)
        .SetCullMode(VK_CULL_MODE_BACK_BIT)
        .SetWindingOrder(VK_FRONT_FACE_CLOCKWISE);
    builder.Build(PlUnlit);
}


void RxDeferredRenderer::CreateGPassPipeline()
{
    VkPipelineLayout gpass_layout = CreateGPassPipelineLayout();

    CreateGPass();

    FxRef<RxShader> shader_geometry = gShaderCache->Request(RxShaderName::eGeometry);
    FxRef<RxShaderProgram> vertex_shader = shader_geometry->GetProgram(RxShaderType::eVertex, {});
    FxRef<RxShaderProgram> fragment_shader = shader_geometry->GetProgram(RxShaderType::eFragment, {});

    RxVertexDescription vertex_info = RxVertexUtil::BuildDescription<RxVertexType::eDefault>();

    RxPipelineBuilder builder;

    builder.SetLayout(gpass_layout)
        .SetName("Geometry Pipeline")
        .AddBlendAttachment({ .Enabled = false })
        .AddBlendAttachment({ .Enabled = false })
        .SetAttachments(&GPass.GetTargets())
        .SetShaders(vertex_shader, fragment_shader)
        .SetRenderPass(&GPass.GetRenderPass())
        .SetVertexDescription(&vertex_info)
        .SetCullMode(VK_CULL_MODE_BACK_BIT)
        .SetWindingOrder(VK_FRONT_FACE_CLOCKWISE);

    builder.SetPolygonMode(VK_POLYGON_MODE_FILL).Build(PlGeometry);
    builder.SetPolygonMode(VK_POLYGON_MODE_LINE).Build(PlGeometryWireframe);

    // Create geometry pipeline with normal maps
    {
        FxSizedArray<FxShaderMacro> normal_mapped_macros { FxShaderMacro { "USE_NORMAL_MAPS", "1" } };

        FxRef<RxShaderProgram> nm_vertex_shader = shader_geometry->GetProgram(RxShaderType::eVertex,
                                                                              normal_mapped_macros);

        FxRef<RxShaderProgram> nm_fragment_shader = shader_geometry->GetProgram(RxShaderType::eFragment,
                                                                                normal_mapped_macros);

        builder.SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetShaders(nm_vertex_shader, nm_fragment_shader)
            .Build(PlGeometryWithNormalMaps);
    }

    {
        vertex_info = RxVertexUtil::BuildDescription<RxVertexType::eSkinned>();

        FxSizedArray<FxShaderMacro> macros = { FxShaderMacro { "USE_NORMAL_MAPS", "1" },
                                               FxShaderMacro { "USE_SKINNING", "1" } };

        FxRef<RxShaderProgram> nm_vertex_shader = shader_geometry->GetProgram(RxShaderType::eVertex, macros);
        FxRef<RxShaderProgram> nm_fragment_shader = shader_geometry->GetProgram(RxShaderType::eFragment, macros);

        VkPipelineLayout skinned_layout = CreateGPassSkinnedPipelineLayout();

        builder.SetLayout(skinned_layout)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetVertexDescription(&vertex_info)
            .SetShaders(nm_vertex_shader, nm_fragment_shader)
            .Build(PlGeometrySkinned);
    }

    pGeometryPipeline = &PlGeometry;
}

void RxDeferredRenderer::DestroyGPassPipeline()
{
    VkDevice device = gRenderer->GetDevice()->Device;

    // Destroy descriptor set layouts

    if (DsLayoutGPassMaterial) {
        vkDestroyDescriptorSetLayout(device, DsLayoutGPassMaterial, nullptr);
        DsLayoutGPassMaterial = nullptr;
    }
    if (DsLayoutGPassSkinned) {
        vkDestroyDescriptorSetLayout(device, DsLayoutGPassSkinned, nullptr);
        DsLayoutGPassSkinned = nullptr;
    }
    if (DsLayoutGPassMaterialAlbedoOnly) {
        vkDestroyDescriptorSetLayout(device, DsLayoutGPassMaterialAlbedoOnly, nullptr);
        DsLayoutGPassMaterialAlbedoOnly = nullptr;
    }

    PlGeometry.Destroy();

    PlGeometryWithNormalMaps.Layout = nullptr;
    PlGeometryWithNormalMaps.Destroy();

    PlGeometryWireframe.Layout = nullptr;
    PlGeometryWireframe.Destroy();

    PlGeometrySkinned.Destroy();
}


void RxDeferredRenderer::CreateLightingDSLayout()
{
    // Fragment DS

    RxDsLayoutBuilder builder {};

    // sDepth
    builder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RxShaderType::eFragment);
    // sAlbedo
    builder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RxShaderType::eFragment);
    // sNormal
    builder.AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RxShaderType::eFragment);
    // sShadowDepth
    builder.AddBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RxShaderType::eFragment);

    builder.AddBinding(4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, RxShaderType::eFragment);

    DsLayoutLightingFrag = builder.Build();
}

VkPipelineLayout RxDeferredRenderer::CreateLightingPipelineLayout()
{
    VkDescriptorSetLayout layouts[] = {
        DsLayoutLightingFrag,
        DsLayoutLightingMaterialProperties,
        gObjectManager->DsLayoutObjectBuffer,
    };

    FxStackArray<RxPushConstants, 2> push_consts = {
        RxPushConstants { .Size = sizeof(FxLightVertPushConstants), .StageFlags = VK_SHADER_STAGE_VERTEX_BIT },
    };

    VkPipelineLayout layout = RxPipeline::CreateLayout(FxSlice(push_consts), FxMakeSlice(layouts, std::size(layouts)));
    RxUtil::SetDebugLabel("Lighting Pipeline Layout", VK_OBJECT_TYPE_PIPELINE_LAYOUT, layout);

    PlLightingOutsideVolume.SetLayout(layout);
    PlLightingInsideVolume.SetLayout(layout);

    return layout;
}

void RxDeferredRenderer::CreateLightingPipeline()
{
    if (DsLayoutLightingFrag == nullptr) {
        CreateLightingDSLayout();
    }

    LightPass.Create(gRenderer->Swapchain.Extent);


    LightPass.AddTarget(RxImageFormat::eRGBA16_Float, RxTarget::scFullScreen,
                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, RxImageAspectFlag::eColor);

    RxTarget* light_target = LightPass.GetTarget(RxImageFormat::eRGBA16_Float);
    FxAssert(light_target != nullptr);
    light_target->FinalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // LightPass.AddTarget(RxImageFormat::eD32_Float, RxTarget::scFullScreen,
    //                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    //                     RxImageAspectFlag::eDepth);

    // RxTarget* depth_attachment = LightPass.GetTarget(RxImageFormat::eD32_Float);
    // depth_attachment->LoadOp = RxLoadOp::eLoad;
    // depth_attachment->StoreOp = RxStoreOp::eDontCare;
    // depth_attachment->bRenderPassOnly = true;
    // depth_attachment->SetImage(GPass.GetTarget(RxImageFormat::eD32_Float)->GetImage());


    LightPass.BuildRenderStage();

    DsLighting.Create(DescriptorPool, DsLayoutLightingFrag, true);

    // sDepth
    DsLighting.AddImageFromTarget(0, GPass.GetTarget(RxImageFormat::eD32_Float), &gRenderer->Swapchain.DepthSampler);
    // sAlbedo
    DsLighting.AddImageFromTarget(1, GPass.GetTarget(RxImageFormat::eBGRA8_UNorm), &gRenderer->Swapchain.ColorSampler);
    // sNormals
    DsLighting.AddImageFromTarget(2, GPass.GetTarget(RxImageFormat::eRGBA16_Float),
                                  &gRenderer->Swapchain.NormalsSampler);
    // Skip 3 for the shadow target, added by RxDirectionalShadows
    DsLighting.AddBuffer(4, &gRenderer->Uniforms.GetGpuBuffer(), 0, gRenderer->Uniforms.Size);

    DsLighting.Build();

    RxShader lighting_shader("Lighting");

    VkPipelineLayout layout = CreateLightingPipelineLayout();

    {
        FxRef<RxShaderProgram> vertex_shader = lighting_shader.GetProgram(RxShaderType::eVertex, {});
        FxRef<RxShaderProgram> fragment_shader = lighting_shader.GetProgram(RxShaderType::eFragment, {});

        RxVertexDescription vertex_info = RxVertexUtil::BuildDescription<RxVertexType::eSlim>();

        RxPipelineBuilder builder {};
        builder.SetLayout(layout)
            .SetName("Lighting(Point)")
            .AddBlendAttachment({
                .Enabled = true,
                .AlphaBlend { .Ops {
                    .Src = VK_BLEND_FACTOR_ONE,
                    .Dst = VK_BLEND_FACTOR_ZERO,
                } },
                .ColorBlend { .Ops { .Src = VK_BLEND_FACTOR_SRC_ALPHA, .Dst = VK_BLEND_FACTOR_ONE } },
            })
            .SetAttachments(&LightPass.GetTargets())
            .SetShaders(vertex_shader, fragment_shader)
            .SetRenderPass(&LightPass.GetRenderPass())
            .SetVertexDescription(&vertex_info)
            .SetProperties({ .bDisableDepthTest = true, .bDisableDepthWrite = true })
            .SetWindingOrder(VK_FRONT_FACE_CLOCKWISE);

        builder.SetCullMode(VK_CULL_MODE_BACK_BIT).Build(PlLightingOutsideVolume);
        builder.SetCullMode(VK_CULL_MODE_BACK_BIT).Build(PlLightingInsideVolume);
    }
    {
        FxSizedArray<FxShaderMacro> directional_macros { FxShaderMacro { "FX_LIGHT_DIRECTIONAL", "1" } };

        FxRef<RxShaderProgram> vertex_shader = lighting_shader.GetProgram(RxShaderType::eVertex, directional_macros);
        FxRef<RxShaderProgram> fragment_shader = lighting_shader.GetProgram(RxShaderType::eFragment,
                                                                            directional_macros);

        RxPipelineBuilder builder {};

        builder.SetLayout(layout)
            .SetName("Lighting(Directional)")
            .AddBlendAttachment({
                .Enabled = true,
                .AlphaBlend { .Ops {
                    .Src = VK_BLEND_FACTOR_ONE,
                    .Dst = VK_BLEND_FACTOR_ZERO,
                } },
                .ColorBlend { .Ops { .Src = VK_BLEND_FACTOR_SRC_ALPHA, .Dst = VK_BLEND_FACTOR_ONE } },
            })
            .SetAttachments(&LightPass.GetTargets())
            .SetShaders(vertex_shader, fragment_shader)
            .SetRenderPass(&LightPass.GetRenderPass())
            .SetVertexDescription(nullptr)
            .SetWindingOrder(VK_FRONT_FACE_CLOCKWISE);

        builder.Build(PlLightingDirectional);
    }
}

void RxDeferredRenderer::DestroyLightingPipeline()
{
    VkDevice device = gRenderer->GetDevice()->Device;

    // Destroy descriptor set layouts
    if (DsLayoutLightingFrag) {
        vkDestroyDescriptorSetLayout(device, DsLayoutLightingFrag, nullptr);
        DsLayoutLightingFrag = nullptr;
    }

    if (DsLayoutLightingMaterialProperties) {
        vkDestroyDescriptorSetLayout(device, DsLayoutLightingMaterialProperties, nullptr);
        DsLayoutLightingMaterialProperties = nullptr;
    }

    PlLightingOutsideVolume.Destroy();

    PlLightingInsideVolume.Layout = nullptr;
    PlLightingInsideVolume.Destroy();

    PlLightingDirectional.Layout = nullptr;
    PlLightingDirectional.Destroy();
}

//////////////////////////////////////////
// RxDeferredRenderer CompPass Functions
//////////////////////////////////////////

VkPipelineLayout RxDeferredRenderer::CreateCompPipelineLayout()
{
    {
        RxDsLayoutBuilder builder {};

        builder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RxShaderType::eFragment)
            .AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RxShaderType::eFragment)
            .AddBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RxShaderType::eFragment)
            .AddBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RxShaderType::eFragment);

        DsLayoutCompFrag = builder.Build();
    }


    VkDescriptorSetLayout layouts[] = {
        DsLayoutCompFrag,
    };

    FxStackArray<RxPushConstants, 1> push_consts = { RxPushConstants { .Size = sizeof(FxCompositionPushConstants),
                                                                       .StageFlags = VK_SHADER_STAGE_FRAGMENT_BIT } };

    VkPipelineLayout layout = RxPipeline::CreateLayout(FxSlice(push_consts), FxMakeSlice(layouts, std::size(layouts)));

    RxUtil::SetDebugLabel("Composition Layout", VK_OBJECT_TYPE_PIPELINE_LAYOUT, layout);
    PlComposition.SetLayout(layout);


    return layout;
}

void RxDeferredRenderer::CreateCompPipeline()
{
    VkPipelineLayout comp_layout = CreateCompPipelineLayout();

    CreateCompPass();


    RxShader shader_composition("Composition");

    FxRef<RxShaderProgram> lit_vertex_shader = shader_composition.GetProgram(RxShaderType::eVertex, {});
    FxRef<RxShaderProgram> lit_fragment_shader = shader_composition.GetProgram(RxShaderType::eFragment, {});

    RxPipelineBuilder builder;

    builder.SetLayout(comp_layout)
        .SetName("Composition Pipeline")
        .AddBlendAttachment({ .Enabled = false })
        .SetAttachments(&CompPass.GetTargets())
        .SetShaders(lit_vertex_shader, lit_fragment_shader)
        .SetRenderPass(&CompPass.GetRenderPass())
        .SetVertexDescription(nullptr)
        .SetCullMode(VK_CULL_MODE_NONE)
        .SetWindingOrder(VK_FRONT_FACE_CLOCKWISE)
        .SetProperties({ .bDisableDepthTest = true, .bDisableDepthWrite = true });


    builder.Build(PlComposition);


    FxSizedArray<FxShaderMacro> unlit_macros { FxShaderMacro { .pcName = "RENDER_UNLIT", .pcValue = "1" } };

    FxRef<RxShaderProgram> unlit_vertex_shader = shader_composition.GetProgram(RxShaderType::eVertex, unlit_macros);
    FxRef<RxShaderProgram> unlit_fragment_shader = shader_composition.GetProgram(RxShaderType::eFragment, unlit_macros);

    builder.SetShaders(unlit_vertex_shader, unlit_fragment_shader).Build(PlCompositionUnlit);
}

void RxDeferredRenderer::DestroyCompPipeline()
{
    VkDevice device = gRenderer->GetDevice()->Device;

    // Destroy descriptor set layouts
    if (DsLayoutCompFrag) {
        vkDestroyDescriptorSetLayout(device, DsLayoutCompFrag, nullptr);
        DsLayoutCompFrag = nullptr;
    }

    PlComposition.Destroy();
    PlCompositionUnlit.Layout = nullptr;
    PlCompositionUnlit.Destroy();
}

void RxDeferredRenderer::CreateCompPass()
{
    DsComposition.Create(DescriptorPool, DsLayoutCompFrag, false);

    CompPass.Create(gRenderer->Swapchain.Extent);

    CompPass.ClearValues.Insert(VkClearValue { .color = { { 0.0f, 0.3f, 0.0f, 1.0f } } });

    CompPass.MarkFinalStage();
    CompPass.BuildRenderStage();

    DsComposition.AddImageFromTarget(1, GPass.GetTarget(RxImageFormat::eD32_Float), &gRenderer->Swapchain.DepthSampler);

    DsComposition.AddImageFromTarget(2, GPass.GetTarget(RxImageFormat::eBGRA8_UNorm),
                                     &gRenderer->Swapchain.ColorSampler);

    DsComposition.AddImageFromTarget(3, GPass.GetTarget(RxImageFormat::eRGBA16_Float),
                                     &gRenderer->Swapchain.NormalsSampler);

    DsComposition.AddImageFromTarget(4, LightPass.GetTarget(RxImageFormat::eRGBA16_Float),
                                     &gRenderer->Swapchain.LightsSampler);


    DsComposition.Build();
}

void RxDeferredRenderer::DoCompPass(FxCamera& camera)
{
    RxCommandBuffer& cmd = gRenderer->GetFrame()->CommandBuffer;

    FxCompositionPushConstants push_constants {};
    memcpy(push_constants.ViewInverse, camera.InvViewMatrix.RawData, sizeof(FxMat4f));
    memcpy(push_constants.ProjInverse, camera.InvProjectionMatrix.RawData, sizeof(FxMat4f));


    vkCmdPushConstants(cmd.Get(), PlComposition.Layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push_constants),
                       &push_constants);

    // CompPass.Begin(cmd, PlComposition);

    DsComposition.Bind(0, cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, PlComposition);

    // Use single triangle instead of two triangles as it removes the overlapping quads the gpu
    // renders between triangles. Source: https://wallisc.github.io/rendering/2021/04/18/Fullscreen-Pass.html
    vkCmdDraw(cmd.CommandBuffer, 3, 1, 0, 0);

    CompPass.End();

    cmd.End();
}
