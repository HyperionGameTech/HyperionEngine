/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <UIPch.hpp>

#include <ui/UIImage.hpp>

#include <rendering/Texture.hpp>

#include <engine/EngineDriver.hpp>

#include <UIImage.generated.inl>

namespace Hyperion {

UIImage::UIImage()
{
    SetBackgroundColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
}

void UIImage::Init()
{
    UIObject::Init();
}

void UIImage::SetTexture(const Handle<Texture>& texture)
{
    if (texture == m_texture)
    {
        return;
    }

    m_texture = texture;

    InitObject(m_texture);

    UpdateMaterial(false);
}

MaterialAttributes UIImage::GetMaterialAttributes() const
{
    return UIObject::GetMaterialAttributes();
}

MaterialTextures UIImage::GetMaterialTextures() const
{
    return MaterialTextures {
        { MaterialTextureKey::Diffuse, m_texture }
    };
}

} // namespace Hyperion
