/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Editor/EditorMemory.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Functional/Delegate.hpp>

#include <Core/Math/Vector3.hpp>

namespace Hyperion {

class EditorSubsystem;
class EditorProject;
class Camera;
class Node;

struct MouseEvent;
struct KeyboardEvent;
struct InputMouseLockScope;

HYP_ENUM()
enum class EditorManipulationMode : uint8
{
    None = 0,

    Translate,
    Rotate,
    Scale,
    ReshapeVolume
};

/*! \brief A widget that can manipulate the selected object. (e.g translate, rotate, scale) */
HYP_CLASS(Abstract)
class EDITOR_API EditorGizmoBase : public ObjectBase
{
    HYP_OBJECT_BODY(EditorGizmoBase);

public:
    static Pool* GetAllocator() { return g_editorPool; }

    EditorGizmoBase();
    virtual ~EditorGizmoBase();

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Node>& GetNode() const
    {
        return m_node;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsDragging() const
    {
        return m_isDragging;
    }

    HYP_FORCE_INLINE void SetCurrentProject(const WeakHandle<EditorProject>& project)
    {
        m_currentProject = project;
    }

    HYP_FORCE_INLINE void SetEditorSubsystem(EditorSubsystem* editorSubsystem)
    {
        m_editorSubsystem = editorSubsystem;
    }

    void Shutdown();

    HYP_METHOD()
    virtual EditorManipulationMode GetManipulationMode() const = 0;

    HYP_METHOD()
    virtual int GetPriority() const
    {
        return -1;
    }

    HYP_METHOD()
    virtual String GetMenuText() const = 0;

    virtual bool IsScreenSpaceSized() const
    {
        return true;
    }

    void UpdateScreenSpaceSize(const Handle<Camera>& camera);

    virtual void SetFocusedNode(const Handle<Node>& focusedNode);

    virtual void OnDragStart(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node, const Vec3f& hitpoint);
    virtual void OnDragEnd(const Handle<Camera>& camera, const MouseEvent& mouseEvent);

    virtual bool OnMouseHover(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node)
    {
        return false;
    }

    virtual bool OnMouseLeave(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node)
    {
        return false;
    }

    virtual bool OnMouseMove(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node)
    {
        return false;
    }

    virtual bool OnKeyPress(const Handle<Camera>& camera, const KeyboardEvent& keyboardEvent, const Handle<Node>& node)
    {
        return false;
    }

protected:
    virtual void Init() override;

    virtual Handle<Node> Load_Internal() const = 0;

    Handle<EditorProject> GetCurrentProject() const;

    HYP_FORCE_INLINE EditorSubsystem* GetEditorSubsystem() const
    {
        return m_editorSubsystem;
    }

    void UpdateScreenSpaceSizeForActiveViewport();

    WeakHandle<Node> m_focusedNode;
    Handle<Node> m_node;
    struct InputMouseLockScope* m_mouseLockScope;

    // Keeps the gizmo in sync when the focused node's transform changes externally
    // (e.g. swatch overrides applied on active-swatch switch)
    DelegateHandler m_focusedNodeTransformHandler;

private:
    EditorSubsystem* m_editorSubsystem;
    WeakHandle<EditorProject> m_currentProject;

    bool m_isDragging;
};

HYP_CLASS()
class NullEditorGizmo : public EditorGizmoBase
{
    HYP_OBJECT_BODY(NullEditorGizmo);

public:
    virtual ~NullEditorGizmo() override = default;

    virtual String GetMenuText() const override
    {
        return "<null>";
    }

    virtual EditorManipulationMode GetManipulationMode() const override
    {
        return EditorManipulationMode::None;
    }

protected:
    virtual Handle<Node> Load_Internal() const override
    {
        return Handle<Node>::empty;
    }
};

} // namespace Hyperion
