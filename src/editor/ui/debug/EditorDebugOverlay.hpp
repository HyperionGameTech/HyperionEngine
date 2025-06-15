/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_EDITOR_DEBUG_OVERLAY_HPP
#define HYPERION_EDITOR_DEBUG_OVERLAY_HPP

#include <core/object/HypObject.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/Handle.hpp>

#include <core/Defines.hpp>

#include <ui/UIObject.hpp>

namespace hyperion {

class UIStage;
class Texture;

HYP_CLASS(Abstract)
class HYP_API EditorDebugOverlayBase : public HypObject<EditorDebugOverlayBase>
{
    HYP_OBJECT_BODY(EditorDebugOverlayBase);

public:
    EditorDebugOverlayBase();
    virtual ~EditorDebugOverlayBase();

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<UIObject>& GetUIObject() const
    {
        return m_ui_object;
    }

    void Initialize(UIObject* spawn_parent);

    HYP_METHOD(Scriptable)
    void Update(float delta);

    HYP_METHOD(Scriptable)
    Handle<UIObject> CreateUIObject(UIObject* spawn_parent);

    HYP_METHOD(Scriptable)
    Name GetName() const;

    HYP_METHOD(Scriptable)
    bool IsEnabled() const;

protected:
    virtual Handle<UIObject> CreateUIObject_Impl(UIObject* spawn_parent);

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

    Handle<UIObject> m_ui_object;
};

HYP_CLASS()
class HYP_API TextureEditorDebugOverlay : public EditorDebugOverlayBase
{
    HYP_OBJECT_BODY(TextureEditorDebugOverlay);

public:
    TextureEditorDebugOverlay(const Handle<Texture>& texture);
    virtual ~TextureEditorDebugOverlay() override;

protected:
    virtual Handle<UIObject> CreateUIObject_Impl(UIObject* spawn_parent) override;

    virtual Name GetName_Impl() const override
    {
        return NAME("TextureEditorDebugOverlay");
    }

    Handle<Texture> m_texture;
};

} // namespace hyperion

#endif