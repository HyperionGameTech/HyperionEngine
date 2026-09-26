/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

///Must be before
#include <Scene/Entity.hpp>

#include <Editor/Gizmo/ScaleEditorGizmo.hpp>
#include <Editor/Gizmo/GizmoSwatchOverrideHelpers.hpp>

#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorProject.hpp>
#include <Editor/EditorActionStack.hpp>
#include <Editor/EditorAction.hpp>

#include <Scene/Prefab.hpp>
#include <Scene/EntityManager.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Scene/Components/MeshComponent.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Rendering/Material.hpp>

#include <Input/InputManager.hpp>
#include <Input/Event.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Math/Ray.hpp>
#include <Core/Logging/Logger.hpp>
#include <Core/Utilities/GlobalContext.hpp>
#include <Core/Debug/Debug.hpp>

#include <System/AppContext.hpp>

#include <ScaleEditorGizmo.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

Handle<Node> ScaleEditorGizmo::Load_Internal() const
{
    GlobalContextScope assetRegistryScope { AssetRegistryContext { GetEditorAssetRegistry() } };

    if (Handle<Prefab> prefab = GetCurrentAssetRegistry()->GetAsset<Prefab>(AssetBuckets::Prefabs, "ScaleGizmo"_sh); prefab.IsValid())
    {
        return prefab->GetRoot();
    }

    return Handle<Node>::Null();
}

void ScaleEditorGizmo::OnDragStart(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node, const Vec3f& hitpoint)
{
    EditorGizmoBase::OnDragStart(camera, mouseEvent, node, hitpoint);

    m_dragData.Unset();

    Entity* entity = DynamicCast<Entity>(node);
    if (!entity)
    {
        return;
    }

    MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>();

    if (!meshComponent || !meshComponent->material)
    {
        return;
    }

    const NodeTag& axisTag = node->GetTag("TransformWidgetAxis"_sh);

    if (!axisTag)
    {
        return;
    }

    int axis = -1;
    axisTag.data.Visit(
        [&axis](auto&& value)
        {
            if constexpr (std::is_integral_v<NormalizedType<decltype(value)>>)
            {
                axis = int(value);
            }
        });

    Handle<Node> focusedNode = m_focusedNode.Lock();

    if (!focusedNode.IsValid())
    {
        return;
    }

    const Vec3f nodeOrigin = focusedNode->GetWorldTranslation();
    const Vec3f initialScale = focusedNode->GetWorldScale();
    const Vec3f cameraDirection = camera->GetDirection();

    Vec3f axisDirection = Vec3f::Zero();
    Vec3f planeNormal = -cameraDirection;

    if (axis >= 0)
    {
        axisDirection[axis] = 1.0f;
        axisDirection = focusedNode->GetWorldRotation().Inverse().RotateVector(axisDirection).Normalized();

        const Vec3f cameraFacingNormal = cameraDirection - axisDirection * cameraDirection.Dot(axisDirection);

        if (cameraFacingNormal.LengthSquared() > MathUtil::epsilonF)
        {
            planeNormal = cameraFacingNormal.Normalized();
        }
    }

    DragData dragData {};
    dragData.axisDirection = axisDirection;
    dragData.planeNormal = planeNormal;
    dragData.planePoint = nodeOrigin;
    dragData.hitpointOrigin = hitpoint;
    dragData.nodeOrigin = nodeOrigin;
    dragData.initialScale = initialScale;
    dragData.axis = axis;

    m_dragData = dragData;

    m_selectedNodes.Clear();

    if (EditorSubsystem* subsystem = GetEditorSubsystem())
    {
        Array<Handle<Node>> selectedNodes = subsystem->GetGizmoTargetNodes();

        for (const Handle<Node>& selectedNode : selectedNodes)
        {
            if (!selectedNode.IsValid())
            {
                continue;
            }

            m_selectedNodes.PushBack({ selectedNode, { selectedNode->GetWorldScale(), selectedNode->GetWorldTranslation() } });
        }
    }
}

void ScaleEditorGizmo::OnDragEnd(const Handle<Camera>& camera, const MouseEvent& mouseEvent)
{
    EditorGizmoBase::OnDragEnd(camera, mouseEvent);

    // OnDragStart can bail before setting drag data; pushing a junk action would wipe redo
    if (!m_dragData)
    {
        m_selectedNodes.Clear();

        return;
    }

    if (Handle<EditorProject> project = GetCurrentProject(); project.IsValid())
    {
        if (Handle<Node> focusedNode = m_focusedNode.Lock(); focusedNode.IsValid())
        {
            const Vec3f finalScale = focusedNode->GetWorldScale();
            const Vec3f originScale = m_dragData->initialScale;
            const Vec3f scaleFactor = finalScale / originScale;

            // Sort nodes by depth (ancestors first) so SetWorldScale on a parent
            // happens before its descendants, preventing the parent's accumulated transform
            // from corrupting the descendant's computed local transform during undo/redo.
            auto nodeData = m_selectedNodes;
            std::sort(nodeData.Begin(), nodeData.End(),
                      [](const Pair<Handle<Node>, Pair<Vec3f, Vec3f>>& a, const Pair<Handle<Node>, Pair<Vec3f, Vec3f>>& b)
                      {
                          return a.first->CalculateDepth() < b.first->CalculateDepth();
                      });

            EditorSubsystem* overrideModeSubsystem = GetEditorSubsystem();
            const bool overrideMode = overrideModeSubsystem && overrideModeSubsystem->IsSwatchOverrideModeEnabled();

            Array<SwatchOverrideTransformEditState> overrideEdits = CaptureSwatchOverrideTransformEdits(nodeData, overrideMode);

            EditorActionStack* actionStack = overrideModeSubsystem != nullptr
                ? overrideModeSubsystem->GetActiveActionStack()
                : project->GetActionStack().Get();

            actionStack->PushAction(MakeHandle<FunctionalEditorAction>(
                nodeData.Size() == 1
                    ? HYP_FORMAT("Scale {}", nodeData[0].first->GetName())
                    : HYP_FORMAT("Scale {} nodes", nodeData.Size()),
                [focusedNode, finalScale, originScale, scaleFactor, nodeData, overrideEdits = std::move(overrideEdits)]() -> EditorActionFunctions
                {
                    auto nodeDataPtr = MakeShared<decltype(nodeData)>(std::move(nodeData));
                    auto overrideEditsPtr = MakeShared<decltype(overrideEdits)>(std::move(overrideEdits));

                    return {
                        [focusedNode, scaleFactor, nodeDataPtr, overrideEditsPtr](EditorSubsystem* editorSubsystem, EditorProject*)
                        {
                            const auto& nodeData = *nodeDataPtr;

                            for (const auto& pair : nodeData)
                            {
                                const Handle<Node>& selectedNode = pair.first;

                                if (!selectedNode.IsValid())
                                {
                                    continue;
                                }

                                selectedNode->SetWorldScale(pair.second.first * scaleFactor);
                            }

                            ExecuteSwatchOverrideTransformEdits(*overrideEditsPtr);

                            editorSubsystem->SetFocusedNode(focusedNode, true);
                        },
                        [focusedNode, nodeDataPtr, overrideEditsPtr](EditorSubsystem* editorSubsystem, EditorProject*)
                        {
                            const auto& nodeData = *nodeDataPtr;

                            for (const auto& pair : nodeData)
                            {
                                const Handle<Node>& selectedNode = pair.first;

                                if (!selectedNode.IsValid())
                                {
                                    continue;
                                }

                                selectedNode->SetWorldScale(pair.second.first);
                            }

                            RevertSwatchOverrideTransformEdits(*overrideEditsPtr);

                            editorSubsystem->SetFocusedNode(focusedNode, true);
                        }
                    };
                }));
        }
    }

    m_dragData.Unset();
    m_selectedNodes.Clear();
}

bool ScaleEditorGizmo::OnMouseHover(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node)
{
    Entity* entity = DynamicCast<Entity>(node);
    if (!entity)
    {
        return false;
    }

    MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>();

    if (!meshComponent || !meshComponent->material)
    {
        return false;
    }

    MaterialParameters newParameters = meshComponent->material->GetParameters();
    newParameters.albedo = Vec4f(1.0f, 1.0f, 0.0f, 1.0f);

    meshComponent->material->SetParameters(newParameters);

    return true;
}

bool ScaleEditorGizmo::OnMouseLeave(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node)
{
    Entity* entity = DynamicCast<Entity>(node);
    if (!entity)
    {
        return false;
    }

    MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>();

    if (!meshComponent || !meshComponent->material)
    {
        return false;
    }

    if (const NodeTag& tag = node->GetTag("TransformWidgetElementColor"_sh))
    {
        MaterialParameters newParameters = meshComponent->material->GetParameters();
        newParameters.albedo = tag.data.TryGet<Vec4f>(Vec4f::Zero());

        meshComponent->material->SetParameters(newParameters);
    }

    return true;
}

bool ScaleEditorGizmo::OnMouseMove(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node)
{
    if (!mouseEvent.mouseButtons[MouseButtonState::LEFT])
    {
        return false;
    }

    if (!m_dragData)
    {
        return false;
    }

    Entity* entity = DynamicCast<Entity>(node);
    if (!entity)
    {
        return false;
    }

    MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>();

    if (!meshComponent || !meshComponent->material)
    {
        return false;
    }

    Handle<Node> focusedNode = m_focusedNode.Lock();

    if (!focusedNode.IsValid())
    {
        return false;
    }

    InputManager* inputMgr = mouseEvent.baseEvent->GetWindow()->GetInputManager();
    AssertDebug(inputMgr != nullptr);

    const Ray ray = camera->GetPickRay(inputMgr->GetVirtualMousePositionNormalized());

    RayHit planeRayHit;

    if (Optional<RayHit> planeRayHitOpt = ray.TestPlane(m_dragData->nodeOrigin, m_dragData->planeNormal))
    {
        planeRayHit = *planeRayHitOpt;
    }
    else
    {
        return true;
    }

    Vec3f newScale;

    if (m_dragData->axis >= 0)
    {
        const Vec3f& axisDirection = m_dragData->axisDirection;

        const float initialProj = (m_dragData->hitpointOrigin - m_dragData->nodeOrigin).Dot(axisDirection);
        const float currentProj = (planeRayHit.hitpoint - m_dragData->nodeOrigin).Dot(axisDirection);

        const float reference = MathUtil::Max(MathUtil::Abs(initialProj), 0.05f);
        const float factor = MathUtil::Max(1.0f + (currentProj - initialProj) / reference, 0.0001f);

        newScale = m_dragData->initialScale;
        newScale[m_dragData->axis] *= factor;
    }
    else
    {
        const Vec3f cameraUp = camera->GetUpVector();

        const float delta = (planeRayHit.hitpoint - m_dragData->hitpointOrigin).Dot(cameraUp);
        const float cameraDistance = (camera->GetWorldTranslation() - m_dragData->nodeOrigin).Length();
        const float reference = MathUtil::Max(cameraDistance * 0.5f, 0.001f);
        const float factor = MathUtil::Max(1.0f + delta / reference, 0.0001f);

        newScale = m_dragData->initialScale * factor;
    }

    newScale = Vec3f::Max(Vec3f(0.0001f), newScale);

    NodeUnlockTransformScope unlockTransformScope(*focusedNode);
    focusedNode->SetWorldScale(newScale);

    // Apply the same scale factor to all selected nodes
    const Vec3f scaleFactor = newScale / m_dragData->initialScale;

    for (const auto& pair : m_selectedNodes)
    {
        const Handle<Node>& selectedNode = pair.first;

        if (!selectedNode.IsValid())
        {
            continue;
        }

        selectedNode->SetWorldScale(pair.second.first * scaleFactor);
    }

    return true;
}

} // namespace Hyperion
