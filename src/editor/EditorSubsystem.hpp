/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_EDITOR_SUBSYSTEM_HPP
#define HYPERION_EDITOR_SUBSYSTEM_HPP

#include <editor/ui/EditorUI.hpp>

#include <scene/Subsystem.hpp>
#include <scene/NodeProxy.hpp>

#include <core/containers/Array.hpp>

#include <core/functional/Delegate.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/memory/RefCountedPtr.hpp>

namespace hyperion {

class Scene;
class Camera;
class Texture;
class InputManager;
class UIStage;
class UIObject;
class FontAtlas;

namespace sys {
class AppContext;
} // namespace sys

using sys::AppContext;

HYP_CLASS()
class HYP_API EditorSubsystem : public Subsystem
{
    HYP_OBJECT_BODY(EditorSubsystem);

public:
    EditorSubsystem(const RC<AppContext> &app_context, const Handle<Scene> &scene, const Handle<Camera> &camera, const RC<UIStage> &ui_stage);
    virtual ~EditorSubsystem() override;

    virtual void Initialize() override;
    virtual void Shutdown() override;
    virtual void Update(GameCounter::TickUnit delta) override;

    virtual void OnSceneAttached(const Handle<Scene> &scene) override;
    virtual void OnSceneDetached(const Handle<Scene> &scene) override;

    HYP_FORCE_INLINE const RC<AppContext> &GetAppContext() const
        { return m_app_context; }

    HYP_METHOD()
    HYP_FORCE_INLINE const RC<UIStage> &GetUIStage() const
        { return m_ui_stage; }

    HYP_METHOD()
    void SetFocusedNode(const NodeProxy &focused_node);

    HYP_METHOD()
    HYP_FORCE_INLINE const NodeProxy &GetFocusedNode() const
        { return m_focused_node; }

    Delegate<void, const NodeProxy &, const NodeProxy &>    OnFocusedNodeChanged;

private:
    void CreateEditorUI();
    void CreateHighlightNode();

    void InitSceneOutline();
    void InitDetailView();
    
    RC<FontAtlas> CreateFontAtlas();

    void UpdateCamera(GameCounter::TickUnit delta);

    RC<AppContext>          m_app_context;
    Handle<Scene>           m_scene;
    Handle<Camera>          m_camera;
    RC<UIStage>             m_ui_stage;

    Handle<Texture>         m_scene_texture;
    RC<UIObject>            m_main_panel;

    NodeProxy               m_focused_node;
    // the actual node that displays the highlight for the focused item
    NodeProxy               m_highlight_node;

    bool                    m_editor_camera_enabled;
    bool                    m_should_cancel_next_click;
};

} // namespace hyperion

#endif
