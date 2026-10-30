/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <CsgEditor/CsgEditorExtension.hpp>
#include <CsgEditor/EditorCsgState.hpp>
#include <Editor/EditorSubsystem.hpp>

#include <Input/Mouse.hpp>
#include <Input/Keyboard.hpp>

#include <CsgEditorExtension.generated.inl>

namespace Hyperion {

CsgEditorExtension::CsgEditorExtension() = default;

CsgEditorExtension::~CsgEditorExtension() = default;

const Handle<EditorCsgState>& CsgEditorExtension::GetCsgState() const
{
    return m_csgState;
}

void CsgEditorExtension::OnAttached()
{
    m_csgState = MakeHandle<EditorCsgState>();
    InitObject(m_csgState);

    m_csgState->Initialize(GetEditorSubsystem());
}

void CsgEditorExtension::OnDetached()
{
    m_csgState.Reset();
}

void CsgEditorExtension::Update(float delta)
{
    if (m_csgState.IsValid())
    {
        m_csgState->Update();
    }
}

void CsgEditorExtension::DebugDraw(DebugDrawCommandList& debugDrawCommandList)
{
    if (m_csgState.IsValid())
    {
        m_csgState->DebugDraw(debugDrawCommandList);
    }
}

bool CsgEditorExtension::OnViewportClick(const MouseEvent& event, const Ray& ray)
{
    if (!IsToolActive())
    {
        return false;
    }

    if (m_csgState->IsPlacing())
    {
        m_csgState->UpdatePlacementHover(event.relativePos);

        if (m_csgState->CommitPlacement())
        {
            return true;
        }
    }

    return m_csgState->IsHandleHovered() || m_csgState->TryPickBrush(ray);
}

bool CsgEditorExtension::OnViewportMouseDown(const MouseEvent& event)
{
    return IsToolActive() && event.mouseButtons[MouseButtonState::LEFT] && m_csgState->BeginHandleDrag(event.relativePos);
}

bool CsgEditorExtension::OnViewportMouseDrag(const MouseEvent& event)
{
    if (!IsToolActive() || !m_csgState->IsHandleDragActive())
    {
        return false;
    }

    m_csgState->UpdateHandleDrag(event.relativePos);

    return true;
}

void CsgEditorExtension::OnViewportMouseMove(const MouseEvent& event)
{
    if (!IsToolActive())
    {
        return;
    }

    if (m_csgState->IsPlacing())
    {
        m_csgState->UpdatePlacementHover(event.relativePos);
    }
    else if (!event.mouseButtons[MouseButtonState::LEFT])
    {
        m_csgState->UpdateHandleHover(event.relativePos);
    }
}

bool CsgEditorExtension::OnViewportKeyDown(const KeyboardEvent& event)
{
    if (!IsToolActive())
    {
        return false;
    }

    if (event.keyCode == KeyCode::KEY_ESCAPE && m_csgState->BackOut())
    {
        return true;
    }

    if (event.keyCode == KeyCode::KEY_RETURN && m_csgState->CanApply())
    {
        m_csgState->ApplyBrush();

        return true;
    }

    if (m_csgState->IsPlacing() && (event.keyCode == KeyCode::KEY_DASH || event.keyCode == KeyCode::KEY_EQUALS))
    {
        m_csgState->ScalePlacement(event.keyCode == KeyCode::KEY_EQUALS ? 1.1f : 1.0f / 1.1f);

        return true;
    }

    return false;
}

void CsgEditorExtension::EndViewportDrag()
{
    if (m_csgState.IsValid())
    {
        m_csgState->EndHandleDrag();
    }
}

bool CsgEditorExtension::IsToolActive() const
{
    return m_csgState.IsValid() && m_csgState->IsEnabled();
}

void CsgEditorExtension::ExitTool()
{
    if (m_csgState.IsValid())
    {
        m_csgState->Exit(/* saveEdits */ true);
    }
}

EditorManipulationMode CsgEditorExtension::ResolveManipulationMode(EditorManipulationMode requestedMode)
{
    if (!m_csgState.IsValid())
    {
        return requestedMode;
    }

    return m_csgState->ResolveManipulationMode(requestedMode);
}

bool CsgEditorExtension::GetGizmoTargetNodes(Array<Handle<Node>>& outTargetNodes) const
{
    if (!IsToolActive())
    {
        return false;
    }

    if (m_csgState->IsBrushSelected())
    {
        outTargetNodes.PushBack(GetEditorSubsystem()->GetFocusedNode());
    }

    return true;
}

void CsgEditorExtension::OnFocusedNodeChanged(const Handle<Node>& focusedNode)
{
    if (!m_csgState.IsValid())
    {
        return;
    }

    if (m_csgState->IsEnabled())
    {
        m_csgState->OnFocusedNodeChanged(focusedNode);

        return;
    }

    // the focused node is what Enter would target, so CanEnter may have changed
    m_csgState->OnStateChanged();
}

} // namespace Hyperion
