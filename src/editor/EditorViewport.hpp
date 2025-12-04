/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/reflection/ObjectBase.hpp>

namespace hyperion {

class View;
class Scene;
class EditorSubsystem;

namespace sys {
class ApplicationWindow;
} // namespace sys

using sys::ApplicationWindow;

HYP_CLASS()
class EditorViewport : public ObjectBase
{
    HYP_OBJECT_BODY(EditorViewport);

public:
    EditorViewport();
    explicit EditorViewport(
        const Handle<View>& view,
        const Handle<ApplicationWindow>& window = Handle<ApplicationWindow>::Null());
    ~EditorViewport() override;

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<View>& GetView() const
    {
        return m_view;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<ApplicationWindow>& GetWindow() const
    {
        return m_window;
    }

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
    Handle<View> m_view;
    Handle<ApplicationWindow> m_window;
};

} // namespace hyperion
