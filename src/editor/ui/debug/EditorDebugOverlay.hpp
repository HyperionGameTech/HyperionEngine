/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/HypObject.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/math/Color.hpp>

#include <core/Handle.hpp>
#include <core/Defines.hpp>

#include <ui/UIObject.hpp>

namespace hyperion {

class UIStage;
class Texture;

HYP_CLASS(Abstract)
class HYP_API EditorDebugOverlayBase : public HypObjectBase
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
    void Update(float delta);

    HYP_METHOD(Scriptable)
    Handle<UIObject> CreateUIObject(UIObject* spawnParent);

    HYP_METHOD(Scriptable)
    Name GetName() const;

    HYP_METHOD(Scriptable)
    bool IsEnabled() const;

protected:
    virtual Handle<UIObject> CreateUIObject_Impl(UIObject* spawnParent);

    virtual void Update_Impl(float delta)
    {
    }

    virtual Name GetName_Impl() const
    {
        HYP_PURE_VIRTUAL();
    }

    virtual bool IsEnabled_Impl() const
    {
        return true;
    }

    Handle<UIObject> m_uiObject;
};

HYP_CLASS()
class HYP_API TextureEditorDebugOverlay : public EditorDebugOverlayBase
{
    HYP_OBJECT_BODY(TextureEditorDebugOverlay);

public:
    TextureEditorDebugOverlay(const Handle<Texture>& texture);
    virtual ~TextureEditorDebugOverlay() override;

protected:
    virtual Handle<UIObject> CreateUIObject_Impl(UIObject* spawnParent) override;

    virtual Name GetName_Impl() const override
    {
        return NAME("TextureEditorDebugOverlay");
    }

    Handle<Texture> m_texture;
};

HYP_CLASS()
class HYP_API TextEditorDebugOverlay : public EditorDebugOverlayBase
{
    HYP_OBJECT_BODY(TextEditorDebugOverlay);

public:
    TextEditorDebugOverlay(const String& text, Color textColor = Color::White(), float textSize = 10.0f);
    virtual ~TextEditorDebugOverlay() override;

protected:
    virtual Handle<UIObject> CreateUIObject_Impl(UIObject* spawnParent) override;

    virtual Name GetName_Impl() const override
    {
        return NAME("TextEditorDebugOverlay");
    }

    String m_text;
    Color m_textColor;
    float m_textSize;
};

} // namespace hyperion

