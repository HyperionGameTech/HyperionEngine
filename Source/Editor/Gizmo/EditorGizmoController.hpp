/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Editor/Gizmo/EditorGizmoBase.hpp>
#include <Editor/EditorMemory.hpp>

#include <Core/Containers/Set.hpp>
#include <Core/Reflection/Handle.hpp>

namespace Hyperion {

class EditorSubsystem;
class Node;

struct MouseEvent;

/*! \brief Owns the set of manipulation-tool gizmos (translate/rotate/scale/volume), which one is
 *  currently selected/hovered, and their proximity-based visibility. Everything a gizmo needs
 *  beyond this reaches back through EditorSubsystem, the same way EditorTerrainState does. */
class EDITOR_API EditorGizmoController
{
public:
    using EditorGizmoSet = HashTable<Handle<EditorGizmoBase>, &EditorGizmoBase::GetManipulationMode, EditorAllocator>;

    EditorGizmoController();
    ~EditorGizmoController();

    void Initialize(EditorSubsystem* subsystem);
    void Shutdown();

    EditorManipulationMode GetSelectedManipulationMode() const;
    void SetSelectedManipulationMode(EditorManipulationMode mode);

    EditorGizmoBase* GetSelectedGizmo() const;
    EditorGizmoBase* GetGizmo(EditorManipulationMode mode) const;
    const EditorGizmoSet& GetGizmos() const;

    void SetHoveredGizmo(
        const MouseEvent& event,
        EditorGizmoBase* gizmo,
        const Handle<Node>& gizmoNode);

    HYP_FORCE_INLINE const WeakHandle<EditorGizmoBase>& GetHoveredGizmo() const
    {
        return m_hoveredGizmo;
    }

    HYP_FORCE_INLINE const WeakHandle<Node>& GetHoveredGizmoNode() const
    {
        return m_hoveredGizmoNode;
    }

    HYP_FORCE_INLINE bool IsHoveringGizmo() const
    {
        return m_hoveredGizmo.IsValid() && m_hoveredGizmoNode.IsValid();
    }

    void UpdateGizmoProximityVisibility();

    HYP_FORCE_INLINE bool AreGizmosHiddenByProximity() const
    {
        return m_gizmosHiddenByProximity;
    }

    HYP_FORCE_INLINE bool IsSnapToGridEnabled() const
    {
        return m_snapToGridEnabled;
    }

    HYP_FORCE_INLINE void SetSnapToGridEnabled(bool snapToGrid)
    {
        m_snapToGridEnabled = snapToGrid;
    }

private:
    EditorSubsystem* m_subsystem = nullptr;

    EditorManipulationMode m_selectedManipulationMode;
    EditorGizmoSet m_gizmos;

    WeakHandle<EditorGizmoBase> m_hoveredGizmo;
    WeakHandle<Node> m_hoveredGizmoNode;

    bool m_gizmosHiddenByProximity = false;
    bool m_snapToGridEnabled = false;
};

} // namespace Hyperion
