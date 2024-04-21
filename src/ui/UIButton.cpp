/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIButton.hpp>
#include <ui/UIText.hpp>

#include <Engine.hpp>

namespace hyperion {

UIButton::UIButton(UIStage *parent, NodeProxy node_proxy)
    : UIObject(parent, std::move(node_proxy))
{
    SetBorderRadius(5);
    SetBorderFlags(UI_OBJECT_BORDER_ALL);
}

void UIButton::Init()
{
    UIObject::Init();

    auto text_element = m_parent->CreateUIObject<UIText>(CreateNameFromDynamicString(ANSIString(m_name.LookupString()) + "_Text"), Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::GROW }, { 16, UIObjectSize::PIXEL }));
    text_element->SetParentAlignment(UIObjectAlignment::UI_OBJECT_ALIGNMENT_CENTER);
    text_element->SetOriginAlignment(UIObjectAlignment::UI_OBJECT_ALIGNMENT_CENTER);
    text_element->SetTextColor(Vec4f { 1.0f, 1.0f, 1.0f, 1.0f });
    text_element->SetText(m_text);

    m_text_element = text_element;

    AddChildUIObject(text_element);
}

void UIButton::SetText(const String &text)
{
    m_text = text;

    if (m_text_element != nullptr) {
        m_text_element->SetText(m_text);
    }
}

Handle<Material> UIButton::GetMaterial() const
{
    return g_material_system->GetOrCreate(
        MaterialAttributes {
            .shader_definition  = ShaderDefinition { HYP_NAME(UIObject), ShaderProperties(static_mesh_vertex_attributes, { "TYPE_BUTTON" }) },
            .bucket             = Bucket::BUCKET_UI,
            .blend_function     = BlendFunction(BlendModeFactor::SRC_ALPHA, BlendModeFactor::ONE_MINUS_SRC_ALPHA,
                                                BlendModeFactor::ONE, BlendModeFactor::ONE_MINUS_SRC_ALPHA),
            .cull_faces         = FaceCullMode::BACK,
            .flags              = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_NONE,
            .layer              = GetDrawableLayer()
        },
        {
            { Material::MATERIAL_KEY_ALBEDO, Vec4f { 0.05f, 0.055f, 0.075f, 1.0f } }
        },
        {
            { Material::MATERIAL_TEXTURE_ALBEDO_MAP, Handle<Texture> { } }
        }
    );
}

} // namespace hyperion
