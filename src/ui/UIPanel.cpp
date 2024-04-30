/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIPanel.hpp>

#include <Engine.hpp>

namespace hyperion {

UIPanel::UIPanel(UIStage *parent, NodeProxy node_proxy, UIObjectType type)
    : UIObject(parent, std::move(node_proxy), type)
{
    SetBorderRadius(5);
    SetBorderFlags(UOB_ALL);

    OnScroll.Bind([this](const UIMouseEventData &event_data) -> UIEventHandlerResult
    {
        SetScrollOffset(GetScrollOffset() - event_data.wheel);

        return UEHR_STOP_BUBBLING;
    }).Detach();
}

UIPanel::UIPanel(UIStage *parent, NodeProxy node_proxy)
    : UIPanel(parent, std::move(node_proxy), UOT_PANEL)
{
}

Handle<Material> UIPanel::GetMaterial() const
{
    return g_material_system->GetOrCreate(
        MaterialAttributes {
            .shader_definition  = ShaderDefinition { HYP_NAME(UIObject), ShaderProperties(static_mesh_vertex_attributes, { "TYPE_PANEL" }) },
            .bucket             = Bucket::BUCKET_UI,
            .blend_function     = BlendFunction(BlendModeFactor::SRC_ALPHA, BlendModeFactor::ONE_MINUS_SRC_ALPHA,
                                                BlendModeFactor::ONE, BlendModeFactor::ONE_MINUS_SRC_ALPHA),
            .cull_faces         = FaceCullMode::BACK,
            .flags              = MaterialAttributes::RAF_NONE,
            .layer              = GetDrawableLayer(),
            .stencil_function   = StencilFunction(StencilOp::KEEP, StencilOp::REPLACE, StencilOp::REPLACE, StencilCompareOp::ALWAYS, 0xFF, 0x1)// <-- @TEMP test
        },
        {
            { Material::MATERIAL_KEY_ALBEDO, Vec4f { 0.075f, 0.09f, 0.135f, 1.0f } }
        },
        {
            { Material::MATERIAL_TEXTURE_ALBEDO_MAP, Handle<Texture> { } }
        }
    );
}

} // namespace hyperion
