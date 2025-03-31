/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIImage.hpp>

#include <rendering/SafeDeleter.hpp>

#include <scene/Texture.hpp>

#include <Engine.hpp>

namespace hyperion {

UIImage::UIImage()
    : UIObject(UIObjectType::IMAGE)
{
    SetBackgroundColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
}

void UIImage::Init()
{
    UIObject::Init();
}

void UIImage::SetTexture(const Handle<Texture> &texture)
{
    if (texture == m_texture) {
        return;
    }

    if (m_texture.IsValid()) {
        g_safe_deleter->SafeRelease(std::move(m_texture));
    }

    m_texture = texture;

    InitObject(m_texture);

    UpdateMaterial(false);
}

MaterialAttributes UIImage::GetMaterialAttributes() const
{
    return UIObject::GetMaterialAttributes();
    // return MaterialAttributes {
    //     .shader_definition  = ShaderDefinition { NAME("UIObject"), ShaderProperties(static_mesh_vertex_attributes, { "TYPE_IMAGE" }) },
    //     .bucket             = Bucket::BUCKET_UI,
    //     .blend_function     = BlendFunction(BlendModeFactor::SRC_ALPHA, BlendModeFactor::ONE_MINUS_SRC_ALPHA,
    //                                         BlendModeFactor::ONE, BlendModeFactor::ONE_MINUS_SRC_ALPHA),
    //     .cull_faces         = FaceCullMode::BACK,
    //     .flags              = MaterialAttributeFlags::NONE
    // };
}

Material::TextureSet UIImage::GetMaterialTextures() const
{
    return Material::TextureSet {
        { MaterialTextureKey::ALBEDO_MAP, m_texture }
    };
}

} // namespace hyperion
