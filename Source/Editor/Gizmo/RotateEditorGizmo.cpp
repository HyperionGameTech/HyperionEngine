/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

///Must be before
#include <Scene/Entity.hpp>

#include <Editor/Gizmo/RotateEditorGizmo.hpp>
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

#include <RotateEditorGizmo.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

Handle<Node> RotateEditorGizmo::Load_Internal() const
{
    GlobalContextScope assetRegistryScope { AssetRegistryContext { GetEditorAssetRegistry() } };

    if (Handle<Prefab> prefab = GetCurrentAssetRegistry()->GetAsset<Prefab>(AssetBuckets::Prefabs, "RotateGizmo"_sh); prefab.IsValid())
    {
        return prefab->GetRoot();
    }

    return Handle<Node>::Null();
}

void RotateEditorGizmo::OnDragStart(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node, const Vec3f& hitpoint)
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

    if (axis < 0)
    {
        return;
    }

    Handle<Node> focusedNode = m_focusedNode.Lock();

    if (!focusedNode.IsValid())
    {
        return;
    }

    DragData dragData {};

    dragData.axis = Vec3f::Zero();
    dragData.axis[axis] = 1.0f;

    dragData.planePoint = m_node->GetWorldTranslation();
    dragData.startRotation = focusedNode->GetWorldRotation();
    dragData.currentRotation = dragData.startRotation;

    Vec3f startVector = hitpoint - dragData.planePoint;
    startVector = startVector - dragData.axis * startVector.Dot(dragData.axis);

    if (startVector.LengthSquared() < MathUtil::epsilonF)
    {
        Vec3f fallback = camera->GetSideVector();
        fallback = fallback - dragData.axis * fallback.Dot(dragData.axis);

        if (fallback.LengthSquared() < MathUtil::epsilonF)
        {
            return;
        }

        startVector = fallback;
    }

    dragData.startVector = startVector.Normalize();

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

            m_selectedNodes.PushBack({ selectedNode, selectedNode->GetWorldRotation() });
        }
    }
}

void RotateEditorGizmo::OnDragEnd(const Handle<Camera>& camera, const MouseEvent& mouseEvent)
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
            const Quat4f finalRotation = m_dragData->currentRotation;
            const Quat4f originRotation = m_dragData->startRotation;
            const Quat4f deltaRotation = originRotation.Inverse() * finalRotation;

            // Sort nodes by depth (ancestors first) so SetWorldRotation on a parent
            // happens before its descendants, preventing accumulated parent rotation
            // from corrupting the descendant's computed local rotation.
            auto nodeData = m_selectedNodes;
            std::sort(nodeData.Begin(), nodeData.End(),
                      [](const Pair<Handle<Node>, Quat4f>& a, const Pair<Handle<Node>, Quat4f>& b)
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
                    ? HYP_FORMAT("Rotate {}", nodeData[0].first->GetName())
                    : HYP_FORMAT("Rotate {} nodes", nodeData.Size()),
                [focusedNode, finalRotation, originRotation, deltaRotation, nodeData, overrideEdits = std::move(overrideEdits)]() -> EditorActionFunctions
                {
                    auto nodeDataPtr = MakeShared<decltype(nodeData)>(std::move(nodeData));
                    auto overrideEditsPtr = MakeShared<decltype(overrideEdits)>(std::move(overrideEdits));

                    return {
                        [focusedNode, deltaRotation, nodeDataPtr, overrideEditsPtr](EditorSubsystem* editorSubsystem, EditorProject*)
                        {
                            const auto& nodeData = *nodeDataPtr;

                            // Execute: ancestors first so parent rotation is up-to-date
                            for (const auto& pair : nodeData)
                            {
                                const Handle<Node>& selectedNode = pair.first;

                                if (!selectedNode.IsValid())
                                {
                                    continue;
                                }

                                selectedNode->SetWorldRotation(pair.second * deltaRotation);
                            }

                            ExecuteSwatchOverrideTransformEdits(*overrideEditsPtr);

                            editorSubsystem->SetFocusedNode(focusedNode, true);
                        },
                        [focusedNode, nodeDataPtr, overrideEditsPtr](EditorSubsystem* editorSubsystem, EditorProject*)
                        {
                            const auto& nodeData = *nodeDataPtr;

                            // Revert: ancestors first so parent rotation is up-to-date
                            for (const auto& pair : nodeData)
                            {
                                const Handle<Node>& selectedNode = pair.first;

                                if (!selectedNode.IsValid())
                                {
                                    continue;
                                }

                                selectedNode->SetWorldRotation(pair.second);
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

bool RotateEditorGizmo::OnMouseHover(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node)
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

bool RotateEditorGizmo::OnMouseLeave(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node)
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

bool RotateEditorGizmo::OnMouseMove(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node)
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

    AssertDebug(mouseEvent.baseEvent->GetWindow() != nullptr);

    InputManager* inputMgr = mouseEvent.baseEvent->GetWindow()->GetInputManager();
    AssertDebug(inputMgr != nullptr);

    const Ray ray = camera->GetPickRay(inputMgr->GetVirtualMousePositionNormalized());

    RayHit planeRayHit;

    if (Optional<RayHit> planeRayHitOpt = ray.TestPlane(m_dragData->planePoint, m_dragData->axis))
    {
        planeRayHit = *planeRayHitOpt;
    }
    else
    {
        return true;
    }

    Vec3f currentVector = planeRayHit.hitpoint - m_dragData->planePoint;
    currentVector = currentVector - m_dragData->axis * currentVector.Dot(m_dragData->axis);

    if (currentVector.LengthSquared() < MathUtil::epsilonF)
    {
        return true;
    }

    currentVector.Normalize();

    const Vec3f cross = m_dragData->startVector.Cross(currentVector);
    const float sinAngle = cross.Dot(m_dragData->axis);
    const float cosAngle = m_dragData->startVector.Dot(currentVector);
    float angle = std::atan2(sinAngle, cosAngle);

    if (EditorSubsystem* subsystem = GetEditorSubsystem(); subsystem && subsystem->IsSnapToGridEnabled())
    {
        angle = subsystem->GetGizmoController()->SnapAngle(angle);
    }

    const Quat4f deltaRotation = Quat4f::AxisAngles(m_dragData->axis, angle).Inverse();
    const Quat4f newRotation = m_dragData->startRotation * deltaRotation;

    m_dragData->currentRotation = newRotation;

    Handle<Node> focusedNode = m_focusedNode.Lock();

    if (!focusedNode.IsValid())
    {
        return false;
    }

    NodeUnlockTransformScope unlockTransformScope(*focusedNode);
    focusedNode->SetWorldRotation(newRotation);

    for (const auto& pair : m_selectedNodes)
    {
        const Handle<Node>& selectedNode = pair.first;

        if (!selectedNode.IsValid())
        {
            continue;
        }

        selectedNode->SetWorldRotation(pair.second * deltaRotation);
    }

    return true;
}

bool RotateEditorGizmo::OnKeyPress(const Handle<Camera>& camera, const KeyboardEvent& keyboardEvent, const Handle<Node>& node)
{
    return false;
}

} // namespace Hyperion
