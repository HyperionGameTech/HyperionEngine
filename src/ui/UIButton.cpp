/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIButton.hpp>
#include <ui/UIText.hpp>

#include <core/utilities/Format.hpp>

#include <Engine.hpp>

namespace hyperion {

UIButton::UIButton(UIStage *parent, NodeProxy node_proxy)
    : UIObject(parent, std::move(node_proxy), UIObjectType::BUTTON)
{
    SetBorderRadius(5);
    SetBorderFlags(UIObjectBorderFlags::ALL);
}

void UIButton::Init()
{
    UIObject::Init();

    RC<UIText> text_element = GetStage()->CreateUIObject<UIText>(CreateNameFromDynamicString(HYP_FORMAT("_Text", m_name)), Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::AUTO }, { 12, UIObjectSize::PIXEL }));
    text_element->SetParentAlignment(UIObjectAlignment::CENTER);
    text_element->SetOriginAlignment(UIObjectAlignment::CENTER);
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
            .shader_definition  = ShaderDefinition { NAME("UIObject"), ShaderProperties(static_mesh_vertex_attributes, { "TYPE_BUTTON" }) },
            .bucket             = Bucket::BUCKET_UI,
            .blend_function     = BlendFunction(BlendModeFactor::SRC_ALPHA, BlendModeFactor::ONE_MINUS_SRC_ALPHA,
                                                BlendModeFactor::ONE, BlendModeFactor::ONE_MINUS_SRC_ALPHA),
            .cull_faces         = FaceCullMode::BACK,
            .flags              = MaterialAttributeFlags::NONE
        },
        {
            { Material::MATERIAL_KEY_ALBEDO, Vec4f(GetBackgroundColor()) }
        },
        {
            { Material::MATERIAL_TEXTURE_ALBEDO_MAP, Handle<Texture> { } }
        }
    );
}

} // namespace hyperion
