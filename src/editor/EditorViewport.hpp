/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

namespace hyperion {

class Camera;
class View;
class Scene;
class EditorSubsystem;

namespace sys {
class ApplicationWindow;
struct WindowOptions;
} // namespace sys

using sys::ApplicationWindow;
using sys::WindowOptions;

HYP_CLASS()
class EditorViewport : public ObjectBase
{
    HYP_OBJECT_BODY(EditorViewport);

public:
    explicit EditorViewport(
        const Handle<Camera>& camera = Handle<Camera>::Null());
    ~EditorViewport() override;

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Camera>& GetCamera() const
    {
        return m_camera;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<View>& GetView() const
    {
        return m_view;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE ApplicationWindow* GetWindow() const
    {
        return m_window;
    }

    HYP_METHOD()
    Handle<ApplicationWindow> CreateViewportWindow(const WindowOptions& options);

    void Init() override;

    /*! \brief Called when the viewport is added to the editor subsystem,
     *  or when a new project is opened. */
    void OnAdded(EditorSubsystem* editorSubsystem);

    /*! \brief Called when the viewport is removed from the editor subsystem
     *  or when the current project is closed. */
    void OnRemoved(EditorSubsystem* editorSubsystem);

    /*! \brief Called from the editor subsystem when a scene is added to the current project's World. */
    void OnSceneAdded(Scene* scene);

    /*! \brief Called from the editor subsystem when a scene is removed from the current project's World. */
    void OnSceneRemoved(Scene* scene);

protected:
    Handle<Camera> m_camera;
    Handle<View> m_view;
    ApplicationWindow* m_window;
};

} // namespace hyperion
