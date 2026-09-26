/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/Gizmo/EditorGizmoBase.hpp>
#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorViewport.hpp>

#include <Scene/Node.hpp>
#include <Scene/Camera/Camera.hpp>
#include <Scene/EnvProbe.hpp>

#include <Input/InputManager.hpp>
#include <Input/Event.hpp>

#include <System/AppContext.hpp>

#include <Framework/CVarManager.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Logging/Logger.hpp>

#include <EditorGizmoBase.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

// Size of one gizmo-local unit (roughly one axis length) as a fraction of the viewport height
static CVar<float> s_cvEditorGizmoScreenSize { "Editor.Gizmo.ScreenSize", 0.12f };

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

void EditorGizmoBase::UpdateScreenSpaceSize(const Handle<Camera>& camera)
{
    if (!m_node.IsValid() || !camera.IsValid() || !IsScreenSpaceSized())
    {
        return;
    }

    const Mat4f& projection = camera->GetProjectionMatrix();

    if (projection[1][1] <= MathUtil::epsilonF)
    {
        return;
    }

    // [3][3] is 0 for perspective projections and 1 for orthographic ones
    const bool isPerspective = projection[3][3] == 0.0f;

    float viewDepth = 1.0f;

    if (isPerspective)
    {
        const Vec3f cameraToGizmo = m_node->GetWorldTranslation() - camera->GetWorldTranslation();

        viewDepth = MathUtil::Max(cameraToGizmo.Dot(camera->GetDirection()), camera->GetNearClip());
    }

    const float viewHeightAtDepth = 2.0f * viewDepth / projection[1][1];
    const float scale = MathUtil::Max(viewHeightAtDepth * s_cvEditorGizmoScreenSize.Get(), MathUtil::epsilonF);

    if (m_node->GetWorldScale() == Vec3f(scale))
    {
        return;
    }

    m_node->SetWorldScale(Vec3f(scale));
}

void EditorGizmoBase::UpdateScreenSpaceSizeForActiveViewport()
{
    if (m_editorSubsystem == nullptr)
    {
        return;
    }

    if (EditorViewport* activeViewport = m_editorSubsystem->GetActiveViewport())
    {
        UpdateScreenSpaceSize(activeViewport->GetCamera());
    }
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

    AlignToFocusedNode(*focusedNode);

    const WeakHandle<Node> weakFocused = focusedNode;

    // Keep the gizmo's translation in sync with the focused node's transform, when it changes externally.
    m_focusedNodeTransformHandler = Node::TransformUpdated.Bind(
        focusedNode.Get(),
        [this, weakFocused](Node* updatedNode) -> void
        {
            if (!m_node.IsValid())
            {
                return;
            }

            const Handle<Node> focused = weakFocused.Lock();

            if (!focused.IsValid() || focused.Get() != updatedNode)
            {
                return;
            }

            AlignToFocusedNode(*focused);
        });
}

void EditorGizmoBase::AlignToFocusedNode(const Node& focusedNode)
{
    m_node->SetWorldTranslation(focusedNode.GetWorldTranslation());

    if (IsLocalSpace())
    {
        m_node->SetWorldRotation(focusedNode.GetWorldRotation());
    }

    UpdateScreenSpaceSizeForActiveViewport();
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
