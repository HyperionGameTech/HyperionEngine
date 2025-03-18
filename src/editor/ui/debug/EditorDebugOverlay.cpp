/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/ui/debug/EditorDebugOverlay.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/Logger.hpp>

#include <ui/UIImage.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

#pragma region EditorDebugOverlayBase

EditorDebugOverlayBase::EditorDebugOverlayBase()
{
}

EditorDebugOverlayBase::~EditorDebugOverlayBase()
{
}

void EditorDebugOverlayBase::Initialize(UIObject *spawn_parent)
{
    Threads::AssertOnThread(g_game_thread);
    AssertThrow(spawn_parent != nullptr);

    m_ui_object = CreateUIObject(spawn_parent);
}

RC<UIObject> EditorDebugOverlayBase::CreateUIObject_Impl(UIObject *spawn_parent)
{
    return spawn_parent->CreateUIObject<UIImage>(GetName(), Vec2i::Zero(), UIObjectSize({ 100, UIObjectSize::PIXEL }, { 75, UIObjectSize::PIXEL }));
}

#pragma endregion EditorDebugOverlayBase

#pragma region TextureEditorDebugOverlay

TextureEditorDebugOverlay::TextureEditorDebugOverlay(const Handle<Texture> &texture)
    : m_texture(texture)
{
}

TextureEditorDebugOverlay::~TextureEditorDebugOverlay()
{
}

RC<UIObject> TextureEditorDebugOverlay::CreateUIObject_Impl(UIObject *spawn_parent)
{
    InitObject(m_texture);

    RC<UIImage> image = spawn_parent->CreateUIObject<UIImage>(GetName(), Vec2i::Zero(), UIObjectSize({ 100, UIObjectSize::PIXEL }, { 75, UIObjectSize::PIXEL }));
    image->SetTexture(m_texture);

    return image;
}

#pragma endregion TextureEditorDebugOverlay

} // namespace hyperion