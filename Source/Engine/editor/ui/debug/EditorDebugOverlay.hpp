/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <Core/math/Color.hpp>

#include <Core/reflection/Handle.hpp>
#include <Core/Defines.hpp>

#include <ui/UIObject.hpp>

namespace Hyperion {

class UIStage;
class Texture;

HYP_CLASS(Abstract)
class HYP_API EditorDebugOverlayBase : public ObjectBase
{
    HYP_OBJECT_BODY(EditorDebugOverlayBase);

public:
    EditorDebugOverlayBase();
    virtual ~EditorDebugOverlayBase();

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<UIObject>& GetUIObject() const
    {
        return m_uiObject;
    }

    void Initialize(UIObject* spawnParent);

    HYP_METHOD(Scriptable)
    int GetPlacement() const; // 0 = top-left, 1 = bottom-left, 2 = top-right, 3 = bottom-right

    HYP_METHOD(Scriptable)
    void Update(float delta);

    HYP_METHOD(Scriptable)
    Handle<UIObject> CreateUIObject(UIObject* spawnParent);

    HYP_METHOD(Scriptable)
    bool IsEnabled() const;

protected:
    virtual Handle<UIObject> CreateUIObject_Impl(UIObject* spawnParent);

    HYP_METHOD()
    virtual int GetPlacement_Impl() const
    {
        return 0; // Default to top-left
    }

    HYP_METHOD()
    virtual void Update_Impl(float delta)
    {
    }

    HYP_METHOD()
    virtual bool IsEnabled_Impl() const
    {
        return true;
    }

    Handle<UIObject> m_uiObject;
};

HYP_CLASS()
class HYP_API TextureOverlay : public EditorDebugOverlayBase
{
    HYP_OBJECT_BODY(TextureOverlay);

public:
    TextureOverlay(const Handle<Texture>& texture);
    virtual ~TextureOverlay() override;

protected:
    virtual Handle<UIObject> CreateUIObject_Impl(UIObject* spawnParent) override;

    Handle<Texture> m_texture;
};

HYP_CLASS()
class HYP_API TextOverlay : public EditorDebugOverlayBase
{
    HYP_OBJECT_BODY(TextOverlay);

public:
    TextOverlay(const String& text, Color textColor = Color::White(), float textSize = 10.0f);
    virtual ~TextOverlay() override;

protected:
    virtual Handle<UIObject> CreateUIObject_Impl(UIObject* spawnParent) override;

    String m_text;
    Color m_textColor;
    float m_textSize;
};

} // namespace Hyperion
