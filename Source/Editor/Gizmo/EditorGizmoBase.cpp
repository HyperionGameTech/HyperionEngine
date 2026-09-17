/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/Gizmo/EditorGizmoBase.hpp>
#include <Editor/EditorSubsystem.hpp>

#include <Scene/Node.hpp>
#include <Scene/Camera/Camera.hpp>
#include <Scene/EnvProbe.hpp>

#include <Input/InputManager.hpp>
#include <Input/Event.hpp>

#include <System/AppContext.hpp>

#include <Core/Logging/Logger.hpp>

#include <EditorGizmoBase.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

EditorGizmoBase::EditorGizmoBase()
    : m_isDragging(false),
      m_mouseLockScope(nullptr)
{
}

EditorGizmoBase::~EditorGizmoBase()
{
    if (m_mouseLockScope)
    {
        delete m_mouseLockScope;
        m_mouseLockScope = nullptr;
    }
}

void EditorGizmoBase::Init()
{
    // Keep the node around so we only have to load it once.
    if (m_node.IsValid() || IsA(NullEditorGizmo::StaticClass()))
    {
        return;
    }

    m_node = Load_Internal();

    if (!m_node.IsValid())
    {
        HYP_LOG(Editor, Warning, "Failed to create gizmo node for \"{}\"!", InstanceClass()->GetName());

        // Create default node so we don't crash trying to use it
        m_node = MakeHandle<Node>();
        m_node->SetName(NAME_FMT("{}_FallbackGizmoNode", InstanceClass()->GetName()));
    }

    m_node->UnlockTransform();

    m_node->SetNodeFlags(m_node->GetNodeFlags() | NodeFlags::HideInSceneOutline);
}

void EditorGizmoBase::Shutdown()
{
    m_focusedNodeTransformHandler.Reset();

    if (m_node.IsValid())
    {
        // Remove from scene
        m_node->Remove();
    }

    m_focusedNode.Reset();
}

void EditorGizmoBase::SetFocusedNode(const Handle<Node>& focusedNode)
{
    // Stop tracking the previously focused node's transform
    m_focusedNodeTransformHandler.Reset();

    if (!focusedNode.IsValid() || focusedNode->IsRoot() || focusedNode->IsA<SkyProbe>())
    {
        // don't want to move the root node or sky
        m_focusedNode.Reset();

        return;
    }

    m_focusedNode = focusedNode;

    if (!m_node.IsValid())
    {
        return;
    }

    m_node->SetWorldTranslation(focusedNode->GetWorldTranslation());

    // Keep the gizmo in sync when the focused node's transform changes externally
    // (e.g. swatch overrides applied on active-swatch switch, undo/redo from other paths).
    const WeakHandle<Node> weakFocused = focusedNode;
    const Handle<Node> gizmoNode = m_node;

    m_focusedNodeTransformHandler = Node::TransformUpdated.Bind(
        focusedNode.Get(),
        [weakFocused, gizmoNode](Node* updatedNode) -> void
        {
            if (!gizmoNode.IsValid())
            {
                return;
            }

            const Handle<Node> focused = weakFocused.Lock();

            if (!focused.IsValid() || focused.Get() != updatedNode)
            {
                return;
            }

            gizmoNode->SetWorldTranslation(focused->GetWorldTranslation());
        });
}

void EditorGizmoBase::OnDragStart(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node, const Vec3f& hitpoint)
{
    m_isDragging = true;

    if (!m_mouseLockScope)
    {
        m_mouseLockScope = new InputMouseLockScope();
    }

    *m_mouseLockScope = g_appContext->GetMainWindow()->GetInputManager()->AcquireMouseLock(/* syncToVirtualPosition */ true);
}

void EditorGizmoBase::OnDragEnd(const Handle<Camera>& camera, const MouseEvent& mouseEvent)
{
    m_isDragging = false;

    if (m_mouseLockScope)
    {
        m_mouseLockScope->Reset();
    }
}

Handle<EditorProject> EditorGizmoBase::GetCurrentProject() const
{
    return m_currentProject.Lock();
}

} // namespace Hyperion
