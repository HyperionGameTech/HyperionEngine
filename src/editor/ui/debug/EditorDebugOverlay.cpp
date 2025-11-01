/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/ui/debug/EditorDebugOverlay.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/Logger.hpp>

#include <rendering/Texture.hpp>

#include <ui/UIImage.hpp>
#include <ui/UIText.hpp>

namespace hyperion {

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
    Threads::AssertOnThread(g_gameThread);
    Assert(spawnParent != nullptr);

    m_uiObject = CreateUIObject(spawnParent);

    if (!m_uiObject)
    {
        return;
    }
}

Handle<UIObject> EditorDebugOverlayBase::CreateUIObject_Impl(UIObject* spawnParent)
{
    return spawnParent->CreateUIObject<UIImage>(GetName(), Vec2i::Zero(), UIObjectSize({ 100, UIObjectSize::PIXEL }, { 75, UIObjectSize::PIXEL }));
}

#pragma endregion EditorDebugOverlayBase

#pragma region TextureEditorDebugOverlay

TextureEditorDebugOverlay::TextureEditorDebugOverlay(const Handle<Texture>& texture)
    : m_texture(texture)
{
}

TextureEditorDebugOverlay::~TextureEditorDebugOverlay()
{
}

Handle<UIObject> TextureEditorDebugOverlay::CreateUIObject_Impl(UIObject* spawnParent)
{
    InitObject(m_texture);

    Handle<UIImage> image = spawnParent->CreateUIObject<UIImage>(GetName(), Vec2i::Zero(), UIObjectSize({ 100, UIObjectSize::PIXEL }, { 75, UIObjectSize::PIXEL }));
    image->SetTexture(m_texture);

    return image;
}

#pragma endregion TextureEditorDebugOverlay

#pragma region TextEditorDebugOverlay

TextEditorDebugOverlay::TextEditorDebugOverlay(const String& text, Color textColor, float textSize)
    : m_text(text),
      m_textColor(textColor),
      m_textSize(textSize)
{
}

TextEditorDebugOverlay::~TextEditorDebugOverlay()
{
}

Handle<UIObject> TextEditorDebugOverlay::CreateUIObject_Impl(UIObject* spawnParent)
{
    Handle<UIText> uiText = spawnParent->CreateUIObject<UIText>(GetName(), Vec2i::Zero(), UIObjectSize({ 0, UIObjectSize::AUTO }, { 0, UIObjectSize::AUTO }));
    uiText->SetText(m_text);
    uiText->SetTextColor(m_textColor);
    uiText->SetTextSize(m_textSize);
    uiText->SetPadding(Vec2i { 2, 2 });

    return uiText;
}

#pragma endregion TextEditorDebugOverlay

} // namespace hyperion
