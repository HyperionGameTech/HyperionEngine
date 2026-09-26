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

namespace Hyperion {

HYP_CLASS()
class ScaleEditorGizmo : public EditorGizmoBase
{
    HYP_OBJECT_BODY(ScaleEditorGizmo);

public:
    virtual ~ScaleEditorGizmo() override = default;

    virtual EditorManipulationMode GetManipulationMode() const override
    {
        return EditorManipulationMode::Scale;
    }

    virtual String GetMenuText() const override
    {
        return "Scale";
    }

    virtual int GetPriority() const override
    {
        return 0;
    }

    virtual bool IsLocalSpace() const override
    {
        return true;
    }

    virtual void OnDragStart(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node, const Vec3f& hitpoint) override;
    virtual void OnDragEnd(const Handle<Camera>& camera, const MouseEvent& mouseEvent) override;

    virtual bool OnMouseHover(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node) override;
    virtual bool OnMouseLeave(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node) override;
    virtual bool OnMouseMove(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node) override;

protected:
    struct DragData
    {
        Vec3f axisDirection;
        Vec3f planeNormal;
        Vec3f planePoint;
        Vec3f hitpointOrigin;
        Vec3f nodeOrigin;
        Vec3f initialScale;
        int axis = -1;
    };

    virtual Handle<Node> Load_Internal() const override;

    Optional<DragData> m_dragData;
    Array<Pair<Handle<Node>, Pair<Vec3f, Vec3f>>> m_selectedNodes; // node + (origin scale, origin translation)
};

} // namespace Hyperion
