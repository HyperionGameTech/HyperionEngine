/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/Gizmo/EditorGizmoController.hpp>
#include <Editor/Gizmo/TranslateEditorGizmo.hpp>
#include <Editor/Gizmo/RotateEditorGizmo.hpp>
#include <Editor/Gizmo/ScaleEditorGizmo.hpp>
#include <Editor/Gizmo/VolumeEditorGizmo.hpp>

#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorViewport.hpp>

#include <Scene/Node.hpp>
#include <Scene/Scene.hpp>
#include <Scene/Camera/Camera.hpp>

#include <Input/Event.hpp>

#include <Rendering/Passes/EditorGridPass.hpp>

#include <Core/Threading/Threads.hpp>

namespace Hyperion {

EditorGizmoController::EditorGizmoController()
    : m_selectedManipulationMode(EditorManipulationMode::None)
{
    m_gizmos.Insert(MakeHandle<NullEditorGizmo>());
    m_gizmos.Insert(MakeHandle<TranslateEditorGizmo>());
    m_gizmos.Insert(MakeHandle<RotateEditorGizmo>());
    m_gizmos.Insert(MakeHandle<ScaleEditorGizmo>());
    m_gizmos.Insert(MakeHandle<VolumeEditorGizmo>());
}

EditorGizmoController::~EditorGizmoController() = default;

void EditorGizmoController::Initialize(EditorSubsystem* subsystem)
{
    m_subsystem = subsystem;

    AssertOnThread(g_simThread);

    for (const Handle<EditorGizmoBase>& gizmo : m_gizmos)
    {
        gizmo->SetEditorSubsystem(m_subsystem);
        gizmo->SetCurrentProject(m_subsystem->GetCurrentProject());

        InitObject(gizmo);
    }
}

void EditorGizmoController::Shutdown()
{
    AssertOnThread(g_simThread);

    if (m_selectedManipulationMode != EditorManipulationMode::None)
    {
        m_selectedManipulationMode = EditorManipulationMode::None;

        m_subsystem->OnSelectedGizmoChanged(
            m_gizmos.At(EditorManipulationMode::None),
            m_gizmos.At(m_selectedManipulationMode));
    }

    for (auto& it : m_gizmos)
    {
        it->Shutdown();
    }
}

EditorManipulationMode EditorGizmoController::GetSelectedManipulationMode() const
{
    return m_selectedManipulationMode;
}

void EditorGizmoController::SetSelectedManipulationMode(EditorManipulationMode mode)
{
    AssertOnThread(g_simThread);

    if (mode == m_selectedManipulationMode)
    {
        return;
    }

    if (!m_gizmos.Contains(mode))
    {
        SetSelectedManipulationMode(EditorManipulationMode::None);
        return;
    }

    EditorGizmoBase* newGizmo = m_gizmos.At(mode);
    EditorGizmoBase* prevGizmo = m_gizmos.At(m_selectedManipulationMode);

    m_selectedManipulationMode = mode;

    m_subsystem->OnSelectedGizmoChanged(newGizmo, prevGizmo);
}

EditorGizmoBase* EditorGizmoController::GetSelectedGizmo() const
{
    AssertOnThread(g_simThread);

    return m_gizmos.At(m_selectedManipulationMode);
}

EditorGizmoBase* EditorGizmoController::GetGizmo(EditorManipulationMode mode) const
{
    AssertOnThread(g_simThread);

    if (!m_gizmos.Contains(mode))
    {
        return nullptr;
    }

    return m_gizmos.At(mode);
}

const EditorGizmoController::EditorGizmoSet& EditorGizmoController::GetGizmos() const
{
    AssertOnThread(g_simThread);

    return m_gizmos;
}

void EditorGizmoController::SetHoveredGizmo(
    const MouseEvent& event,
    EditorGizmoBase* gizmo,
    const Handle<Node>& gizmoNode)
{
    EditorViewport* activeViewport = m_subsystem->GetActiveViewport();
    if (activeViewport == nullptr)
    {
        return;
    }

    if (m_hoveredGizmo.IsValid() && m_hoveredGizmoNode.IsValid())
    {
        Handle<Node> hoveredGizmoNode = m_hoveredGizmoNode.Lock();
        Handle<EditorGizmoBase> hoveredGizmo = m_hoveredGizmo.Lock();

        if (hoveredGizmoNode && hoveredGizmo)
        {
            hoveredGizmo->OnMouseLeave(activeViewport->GetCamera(), event, hoveredGizmoNode);
        }
    }

    if (gizmo != nullptr)
    {
        m_hoveredGizmo = MakeWeakRef(gizmo);
    }
    else
    {
        m_hoveredGizmo.Reset();
    }

    m_hoveredGizmoNode = gizmoNode;
}

void EditorGizmoController::UpdateGizmoProximityVisibility()
{
    AssertOnThread(g_simThread);

    EditorGizmoBase* gizmo = GetSelectedGizmo();

    if (gizmo == nullptr || gizmo->GetManipulationMode() == EditorManipulationMode::None)
    {
        m_gizmosHiddenByProximity = false;

        return;
    }

    // Never change gizmo visibility while a drag is active
    if (gizmo->IsDragging())
    {
        return;
    }

    const Handle<Node>& gizmoNode = gizmo->GetNode();

    const Handle<Scene>& editorScene = m_subsystem->GetEditorScene();

    if (!gizmoNode.IsValid() || !editorScene.IsValid())
    {
        return;
    }

    EditorViewport* activeViewport = m_subsystem->GetActiveViewport();

    if (activeViewport == nullptr || !activeViewport->GetCamera().IsValid())
    {
        return;
    }

    static constexpr float HideDistanceFactor = 0.8f;
    static constexpr float ShowDistanceFactor = 1.2f;

    const float gizmoScale = gizmoNode->GetWorldScale().Max();
    const float hideDistance = gizmoScale * HideDistanceFactor;
    const float showDistance = gizmoScale * ShowDistanceFactor;

    const float cameraDistance = (activeViewport->GetCamera()->GetWorldTranslation() - gizmoNode->GetWorldTranslation()).Length();

    const bool shouldHide = m_gizmosHiddenByProximity
        ? cameraDistance < showDistance
        : cameraDistance < hideDistance;

    if (shouldHide == m_gizmosHiddenByProximity)
    {
        if (shouldHide && gizmoNode->GetParent() != nullptr)
        {
            SetHoveredGizmo(MouseEvent {}, nullptr, Handle<Node>::Null());

            gizmoNode->Remove();
        }

        return;
    }

    m_gizmosHiddenByProximity = shouldHide;

    if (shouldHide)
    {
        SetHoveredGizmo(MouseEvent {}, nullptr, Handle<Node>::Null());

        gizmoNode->Remove();
    }
    else
    {
        editorScene->GetRoot()->AddChild(gizmoNode);
    }
}

static float SnapToGridLine(float value, float gridOffset, float gridSize)
{
    return MathUtil::Round((value - gridOffset) / gridSize) * gridSize + gridOffset;
}

Vec3f EditorGizmoController::SnapToGrid(const Vec3f& position) const
{
    const float gridSize = MathUtil::Max(g_cvEditorGridSize.Get(), MathUtil::epsilonF);
    const Vec3f gridOffset(g_cvEditorGridOffsetX.Get(), g_cvEditorGridOffsetY.Get(), g_cvEditorGridOffsetZ.Get());

    return Vec3f(
        SnapToGridLine(position.x, gridOffset.x, gridSize),
        SnapToGridLine(position.y, gridOffset.y, gridSize),
        SnapToGridLine(position.z, gridOffset.z, gridSize));
}

float EditorGizmoController::SnapToGridAlongAxis(const Vec3f& origin, const Vec3f& axisDirection, float distance) const
{
    const float gridSize = MathUtil::Max(g_cvEditorGridSize.Get(), MathUtil::epsilonF);

    if (MathUtil::Abs(axisDirection).Max() < 1.0f - 1e-4f)
    {
        return MathUtil::Round(distance / gridSize) * gridSize;
    }

    const Vec3f gridOffset(g_cvEditorGridOffsetX.Get(), g_cvEditorGridOffsetY.Get(), g_cvEditorGridOffsetZ.Get());
    const float originAlongAxis = origin.Dot(axisDirection);

    return SnapToGridLine(originAlongAxis + distance, gridOffset.Dot(axisDirection), gridSize) - originAlongAxis;
}

} // namespace Hyperion
