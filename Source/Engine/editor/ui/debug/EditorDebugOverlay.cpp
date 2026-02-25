/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <EditorPch.hpp>

#include <editor/ui/debug/EditorDebugOverlay.hpp>

#include <Core/threading/Threads.hpp>

#include <rendering/Texture.hpp>

#include <ui/UIImage.hpp>
#include <ui/UIText.hpp>

#include <EditorDebugOverlay.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

#pragma region EditorDebugOverlayBase

EditorDebugOverlayBase::EditorDebugOverlayBase()
{
}

EditorDebugOverlayBase::~EditorDebugOverlayBase()
{
}

void EditorDebugOverlayBase::Initialize(UIObject* spawnParent)
{
    AssertOnThread(g_simThread);
    Assert(spawnParent != nullptr);

    m_uiObject = CreateUIObject(spawnParent);

    if (!m_uiObject)
    {
        return;
    }
}

Handle<UIObject> EditorDebugOverlayBase::CreateUIObject_Impl(UIObject* spawnParent)
{
    return spawnParent->CreateUIObject<UIImage>(InstanceClass()->GetName(), Vec2i::Zero(), UIObjectSize({ 100, UIObjectSize::PIXEL }, { 75, UIObjectSize::PIXEL }));
}

#pragma endregion EditorDebugOverlayBase

#pragma region TextureOverlay

TextureOverlay::TextureOverlay(const Handle<Texture>& texture)
    : m_texture(texture)
{
}

TextureOverlay::~TextureOverlay()
{
}

Handle<UIObject> TextureOverlay::CreateUIObject_Impl(UIObject* spawnParent)
{
    InitObject(m_texture);

    Handle<UIImage> image = spawnParent->CreateUIObject<UIImage>(InstanceClass()->GetName(), Vec2i::Zero(), UIObjectSize({ 100, UIObjectSize::PIXEL }, { 75, UIObjectSize::PIXEL }));
    image->SetTexture(m_texture);

    return image;
}

#pragma endregion TextureOverlay

#pragma region TextOverlay

TextOverlay::TextOverlay(const String& text, Color textColor, float textSize)
    : m_text(text),
      m_textColor(textColor),
      m_textSize(textSize)
{
}

TextOverlay::~TextOverlay()
{
}

Handle<UIObject> TextOverlay::CreateUIObject_Impl(UIObject* spawnParent)
{
    Handle<UIText> uiText = spawnParent->CreateUIObject<UIText>(InstanceClass()->GetName(), Vec2i::Zero(), UIObjectSize({ 0, UIObjectSize::AUTO }, { 0, UIObjectSize::AUTO }));
    uiText->SetText(m_text);
    uiText->SetTextColor(m_textColor);
    uiText->SetTextSize(m_textSize);
    uiText->SetPadding(Vec2i { 2, 2 });

    return uiText;
}

#pragma endregion TextOverlay

} // namespace Hyperion
