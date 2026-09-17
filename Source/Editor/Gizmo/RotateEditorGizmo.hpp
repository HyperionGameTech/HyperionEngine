/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Editor/Gizmo/EditorGizmoBase.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Utilities/Pair.hpp>
#include <Core/Utilities/Optional.hpp>
#include <Core/Math/Quat4f.hpp>

namespace Hyperion {

HYP_CLASS()
class RotateEditorGizmo : public EditorGizmoBase
{
    HYP_OBJECT_BODY(RotateEditorGizmo);

public:
    virtual ~RotateEditorGizmo() override = default;

    virtual EditorManipulationMode GetManipulationMode() const override
    {
        return EditorManipulationMode::Rotate;
    }

    virtual String GetMenuText() const override
    {
        return "Rotate";
    }

    virtual int GetPriority() const override
    {
        return 0;
    }

    virtual void OnDragStart(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node, const Vec3f& hitpoint) override;
    virtual void OnDragEnd(const Handle<Camera>& camera, const MouseEvent& mouseEvent) override;

    virtual bool OnMouseHover(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node) override;
    virtual bool OnMouseLeave(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node) override;
    virtual bool OnMouseMove(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node) override;

    virtual bool OnKeyPress(const Handle<Camera>& camera, const KeyboardEvent& keyboardEvent, const Handle<Node>& node) override;

protected:
    struct DragData
    {
        Vec3f axis;
        Vec3f planePoint;
        Vec3f startVector;
        Quat4f startRotation;
        Quat4f currentRotation;
    };

    virtual Handle<Node> Load_Internal() const override;

    Optional<DragData> m_dragData;
    Array<Pair<Handle<Node>, Quat4f>> m_selectedNodes;
};

} // namespace Hyperion
