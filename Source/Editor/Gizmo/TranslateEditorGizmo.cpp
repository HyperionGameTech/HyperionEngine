/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

///Must be before
#include <Scene/Entity.hpp>

#include <Editor/Gizmo/TranslateEditorGizmo.hpp>
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

#include <TranslateEditorGizmo.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

void TranslateEditorGizmo::OnDragStart(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node, const Vec3f& hitpoint)
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

    DragData dragData {
        .axisDirection = Vec3f::Zero(),
        .planeNormal = Vec3f::Zero(),
        .planePoint = m_node->GetWorldTranslation(),
        .hitpointOrigin = hitpoint,
        .nodeOrigin = focusedNode->GetWorldTranslation()
    };

    const Ray ray = camera->GetPickRay(mouseEvent.relativePos);

    if (axis == -1)
    {
        // Centroid - allow dragging to any direction (screen space)
        dragData.planeNormal = -camera->GetDirection();
    }
    else
    {
        dragData.axisDirection[axis] = 1.0f;

        if (axis == 1) // +Y, -Y
        {
            dragData.planeNormal = dragData.axisDirection.Cross(camera->GetSideVector()).Normalize();
        }
        else
        {
            dragData.planeNormal = dragData.axisDirection.Cross(camera->GetUpVector()).Normalize();
        }

        RayHit planeRayHit;

        if (Optional<RayHit> planeRayHitOpt = ray.TestPlane(dragData.planePoint, dragData.planeNormal))
        {
            planeRayHit = *planeRayHitOpt;
        }
        else
        {
            HYP_LOG(Editor, Verbose, "Ray plane test returned no hit. plane point : {}, plane normal {}", dragData.planePoint, dragData.planeNormal);
            return;
        }
    }

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

            m_selectedNodes.PushBack({ selectedNode, selectedNode->GetWorldTranslation() });
        }
    }
}

void TranslateEditorGizmo::OnDragEnd(const Handle<Camera>& camera, const MouseEvent& mouseEvent)
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
            const Vec3f focusedFinalPosition = focusedNode->GetWorldTranslation();
            const Vec3f focusedOrigin = m_dragData->nodeOrigin;

            // Sort nodes by depth (ancestors first) so SetWorldTranslation on a parent
            // happens before its descendants, preventing the parent's accumulated transform
            // from corrupting the descendant's computed local transform during undo/redo.
            auto nodeData = m_selectedNodes;
            std::sort(nodeData.Begin(), nodeData.End(),
                      [](const Pair<Handle<Node>, Vec3f>& a, const Pair<Handle<Node>, Vec3f>& b)
                      {
                          return a.first->CalculateDepth() < b.first->CalculateDepth();
                      });

            String text = nodeData.Size() == 1
                ? HYP_FORMAT("Translate {}", nodeData[0].first->GetName())
                : HYP_FORMAT("Translate {} nodes", nodeData.Size());

            EditorSubsystem* overrideModeSubsystem = GetEditorSubsystem();
            const bool overrideMode = overrideModeSubsystem && overrideModeSubsystem->IsSwatchOverrideModeEnabled();

            Array<SwatchOverrideTransformEditState> overrideEdits = CaptureSwatchOverrideTransformEdits(nodeData, overrideMode);

            EditorActionStack* actionStack = overrideModeSubsystem != nullptr
                ? overrideModeSubsystem->GetActiveActionStack()
                : project->GetActionStack().Get();

            actionStack->PushAction(MakeHandle<FunctionalEditorAction>(
                text,
                [focusedNode, node = m_node, focusedFinalPosition, focusedOrigin, nodeData = std::move(nodeData), overrideEdits = std::move(overrideEdits)]() -> EditorActionFunctions
                {
                    auto nodeDataPtr = MakeShared<decltype(nodeData)>(std::move(nodeData));
                    auto overrideEditsPtr = MakeShared<decltype(overrideEdits)>(std::move(overrideEdits));

                    return {
                        [focusedNode, node, focusedFinalPosition, focusedOrigin, nodeDataPtr, overrideEditsPtr](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                        {
                            const auto& nodeData = *nodeDataPtr;
                            const Vec3f translationDelta = focusedFinalPosition - focusedOrigin;

                            for (const auto& pair : nodeData)
                            {
                                const Handle<Node>& selectedNode = pair.first;

                                if (!selectedNode.IsValid())
                                {
                                    continue;
                                }

                                selectedNode->SetWorldTranslation(pair.second + translationDelta);
                            }

                            ExecuteSwatchOverrideTransformEdits(*overrideEditsPtr);

                            if (Node* parent = node->FindParentWithName("TranslateGizmo"))
                            {
                                parent->SetWorldTranslation(focusedFinalPosition);
                            }

                            editorSubsystem->SetFocusedNode(focusedNode, true);
                        },
                        [focusedNode, node, focusedOrigin, nodeDataPtr, overrideEditsPtr](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                        {
                            const auto& nodeData = *nodeDataPtr;

                            for (const auto& pair : nodeData)
                            {
                                const Handle<Node>& selectedNode = pair.first;

                                if (!selectedNode.IsValid())
                                {
                                    continue;
                                }

                                selectedNode->SetWorldTranslation(pair.second);
                            }

                            RevertSwatchOverrideTransformEdits(*overrideEditsPtr);

                            if (Node* parent = node->FindParentWithName("TranslateGizmo"))
                            {
                                parent->SetWorldTranslation(focusedOrigin);
                            }

                            editorSubsystem->SetFocusedNode(focusedNode, true);
                        }
                    };
                }));
        }
    }

    m_dragData.Unset();
    m_selectedNodes.Clear();
}

bool TranslateEditorGizmo::OnMouseHover(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node)
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
    newParameters.albedo = Vec4f(1.0f, 1.0f, 0.0, 1.0);

    meshComponent->material->SetParameters(newParameters);

    return true;
}

bool TranslateEditorGizmo::OnMouseLeave(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node)
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

bool TranslateEditorGizmo::OnMouseMove(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node)
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

    const NodeTag& axisTag = node->GetTag("TransformWidgetAxis"_sh);

    if (!axisTag)
    {
        return false;
    }

    AssertDebug(mouseEvent.baseEvent->GetWindow() != nullptr);

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

    const EditorGizmoController* snapController = GetEditorSubsystem() && GetEditorSubsystem()->IsSnapToGridEnabled()
        ? GetEditorSubsystem()->GetGizmoController()
        : nullptr;

    Vec3f translation;

    if (m_dragData->axisDirection == Vec3f::Zero())
    {
        translation = m_dragData->nodeOrigin + (planeRayHit.hitpoint - m_dragData->hitpointOrigin);

        if (snapController)
        {
            translation = snapController->SnapToGrid(translation);
        }
    }
    else
    {
        float t = (planeRayHit.hitpoint - m_dragData->hitpointOrigin).Dot(m_dragData->axisDirection);

        if (snapController)
        {
            t = snapController->SnapToGridAlongAxis(m_dragData->nodeOrigin, m_dragData->axisDirection, t);
        }

        translation = m_dragData->nodeOrigin + (m_dragData->axisDirection * t);
    }

    Handle<Node> focusedNode = m_focusedNode.Lock();

    if (!focusedNode.IsValid())
    {
        return false;
    }

    NodeUnlockTransformScope unlockTransformScope(*focusedNode);
    focusedNode->SetWorldTranslation(translation);

    if (Node* parent = node->FindParentWithName("TranslateGizmo"))
    {
        parent->SetWorldTranslation(translation);
    }

    // Apply the same translation delta to all selected nodes
    const Vec3f translationDelta = translation - m_dragData->nodeOrigin;

    for (const auto& pair : m_selectedNodes)
    {
        const Handle<Node>& selectedNode = pair.first;

        if (!selectedNode.IsValid())
        {
            continue;
        }

        selectedNode->SetWorldTranslation(pair.second + translationDelta);
    }

    return true;
}

bool TranslateEditorGizmo::OnKeyPress(const Handle<Camera>& camera, const KeyboardEvent& keyboardEvent, const Handle<Node>& node)
{
    if (!node)
    {
        return false;
    }

    const Handle<CameraController>& controller = camera->GetCameraController();

    if (!controller)
    {
        return false;
    }

    InputHandlerBase* inputHandler = controller->GetInputHandler();

    if (!inputHandler)
    {
        return false;
    }

    switch (keyboardEvent.keyCode)
    {
    case KeyCode::KEY_LEFT:
    case KeyCode::KEY_RIGHT:
    case KeyCode::KEY_UP:
    case KeyCode::KEY_DOWN: // fallthrough
    {
        const BitField<NumKeyboardKeys>& keyStates = inputHandler->GetKeyStates();

        const bool snapMovement = keyStates.Test(uint32(KeyCode::KEY_LALT)) || keyStates.Test(uint32(KeyCode::KEY_RALT));

        float step = 1.0f;

        if (keyStates.Test(uint32(KeyCode::KEY_LSHIFT)) || keyStates.Test(uint32(KeyCode::KEY_RSHIFT)))
        {
            // use larger step with shift held down
            step *= 10.0f;
        }

        const Vec3f cameraForwardVector = camera->GetDirection();
        const Vec3f cameraSideVector = camera->GetSideVector();

        const Quat4f invNodeRotation = node->GetWorldRotation().Inverse();

        const Vec3f nodeForwardVector = invNodeRotation.RotateVector(cameraForwardVector);
        const Vec3f nodeSideVector = invNodeRotation.RotateVector(cameraSideVector);

        NodeUnlockTransformScope scope(*node);

        Vec3f moveVec;

        switch (keyboardEvent.keyCode)
        {
        case KeyCode::KEY_LEFT:
            moveVec = nodeSideVector;

            break;
        case KeyCode::KEY_RIGHT:
            moveVec = -nodeSideVector;

            break;
        case KeyCode::KEY_UP:
            moveVec = nodeForwardVector;

            break;
        case KeyCode::KEY_DOWN:
            moveVec = -nodeForwardVector;

            break;
        default:
            return false;
        }

        int dominantAxis;

        if (std::fabsf(moveVec.x) >= std::fabsf(moveVec.y) && std::fabsf(moveVec.x) >= std::fabsf(moveVec.z))
        {
            dominantAxis = 0;
        }
        else if (std::fabsf(moveVec.y) >= std::fabsf(moveVec.z) && std::fabsf(moveVec.y) >= std::fabsf(moveVec.x))
        {
            dominantAxis = 1;
        }
        else
        {
            dominantAxis = 2;
        }

        for (int i = 0; i < 3; i++)
        {
            if (i != dominantAxis)
            {
                moveVec[i] = 0.0f;
            }
        }

        moveVec = node->GetWorldRotation().RotateVector(moveVec);
        moveVec.Normalize();
        moveVec *= step;

        Vec3f worldTranslation = node->GetWorldTranslation() + moveVec;
        if (snapMovement)
        {
            /// \todo : Configurable snap value
            worldTranslation[dominantAxis] = std::fmodf(worldTranslation[dominantAxis], 1.0f);
        }

        node->SetWorldTranslation(worldTranslation);
    }

    break;
    default:
        break;
    }

    return false;
}

Handle<Node> TranslateEditorGizmo::Load_Internal() const
{
    GlobalContextScope assetRegistryScope { AssetRegistryContext { GetEditorAssetRegistry() } };

    if (Handle<Prefab> prefab = GetCurrentAssetRegistry()->GetAsset<Prefab>(AssetBuckets::Prefabs, "TranslateGizmo"_sh); prefab.IsValid())
    {
        return prefab->GetRoot();
    }

    return Handle<Node>::Null();
}

} // namespace Hyperion
