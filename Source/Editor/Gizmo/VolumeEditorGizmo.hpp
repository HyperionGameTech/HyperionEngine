/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Editor/Gizmo/EditorGizmoBase.hpp>

#include <Core/Math/BoundingBox.hpp>
#include <Core/Utilities/Optional.hpp>

namespace Hyperion {

/*! \brief A gizmo for editing axis-aligned bounding boxes by dragging individual faces.
 *  Used for resizing volumes such as LightmapVolume, FogVolume, etc.
 *  Each face of the AABB is represented as a draggable quad handle.
 */
HYP_CLASS()
class VolumeEditorGizmo : public EditorGizmoBase
{
    HYP_OBJECT_BODY(VolumeEditorGizmo);

public:
    VolumeEditorGizmo();
    virtual ~VolumeEditorGizmo() override = default;

    virtual EditorManipulationMode GetManipulationMode() const override
    {
        return EditorManipulationMode::ReshapeVolume;
    }

    virtual String GetMenuText() const override
    {
        return "Volume Edit";
    }

    virtual int GetPriority() const override
    {
        return 0;
    }

    virtual bool IsScreenSpaceSized() const override
    {
        return false;
    }

    virtual void SetFocusedNode(const Handle<Node>& focusedNode) override;

    virtual void OnDragStart(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node, const Vec3f& hitpoint) override;
    virtual void OnDragEnd(const Handle<Camera>& camera, const MouseEvent& mouseEvent) override;

    virtual bool OnMouseHover(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node) override;
    virtual bool OnMouseLeave(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node) override;
    virtual bool OnMouseMove(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node) override;

    virtual bool OnKeyPress(const Handle<Camera>& camera, const KeyboardEvent& keyboardEvent, const Handle<Node>& node) override;

protected:
    struct DragData
    {
        int faceIndex;
        Vec3f faceNormal;
        Vec3f planePoint;
        Vec3f planeNormal;
        float hitOffset;
        BoundingBox originalBounds;
    };

    virtual Handle<Node> Load_Internal() const override;

private:
    void UpdateFaceGeometry(const BoundingBox& localBounds, const Vec3f& worldTranslation);

    Optional<DragData> m_dragData;
    BoundingBox m_currentBounds;
};

} // namespace Hyperion
