/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorDelegates.hpp>
#include <Editor/EditorCamera.hpp>
#include <Editor/EditorTask.hpp>
#include <Editor/EditorProject.hpp>
#include <Editor/EditorActionStack.hpp>
#include <Editor/EditorAction.hpp>
#include <Editor/EditorState.hpp>
#include <Editor/EditorViewport.hpp>
#include <Editor/EditorCommand.hpp>
#include <Editor/EditorConfig.hpp>

#include <Editor/Terrain/EditorTerrainState.hpp>

#include <Scene/Systems/Editor/EditorSpriteSystem.hpp>

#include <DotNET/DotNETHost.hpp>

#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/View.hpp>
#include <Scene/Light.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/FogVolume.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Prefab.hpp>
#include <Scene/Swatch.hpp>

#include <Scene/System.hpp>
#include <Scene/Systems/ScriptSystem.hpp>
#include <Scene/Systems/MeshSystem.hpp>
#include <Scene/Systems/SwatchOverrideSystem.hpp>

#include <Scene/WorldGrid/WorldGrid.hpp>
#include <Scene/WorldGrid/WorldGridLayer.hpp>

#include <Scene/Sky/DynamicSkySystem.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Scene/Util/SceneHelpers.hpp>

#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/VisibilityStateComponent.hpp>
#include <Scene/Components/BoundingBoxComponent.hpp>
#include <Scene/Components/TransformComponent.hpp>
#include <Scene/Components/RigidBodyComponent.hpp>
#include <Scene/Components/CharacterControllerComponent.hpp>
#include <Scene/Components/TerrainCellComponent.hpp>

#include <Physics/PhysicsShape.hpp>

#include <Scene/LightmapVolume.hpp>
#include <Scene/Volume.hpp>
#include <Scene/Sprite.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetBatch.hpp>

#include <UI/UISubsystem.hpp>
#include <UI/UIObject.hpp>
#include <UI/UIStage.hpp>
#include <UI/UIImage.hpp>
#include <UI/UIListView.hpp>
#include <UI/UIWindow.hpp>
#include <UI/UIGrid.hpp>
#include <UI/UIText.hpp>
#include <UI/UIButton.hpp>
#include <UI/UIMenuBar.hpp>
#include <UI/UIDataSource.hpp>
#include <UI/UITextbox.hpp>

#include <Input/InputManager.hpp>
#include <Input/Event.hpp>

#include <System/AppContext.hpp>
#include <System/OpenFileDialog.hpp>
#include <System/MessageBox.hpp>

#include <Core/Threading/TaskSystem.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>

#include <Rendering/Util/MeshBuilder.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Framework/CVarManager.hpp>

#include <Framework/Server/GameServer.hpp>

#include <Rendering/Texture.hpp>
#include <Rendering/RendererMain.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/DebugDrawer.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Baking/BakerSubsystem.hpp>
#include <Baking/BakeData.hpp>
#include <Baking/Baker.hpp>
#include <Baking/BakeEpoch.hpp>
#include <Baking/BakeLayer.hpp>

#include <UI/Overlays/MessagesOverlay.hpp>

// for EnumToString
#include <Core/Reflection/Enum.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Scripting/ScriptingService.hpp>

#include <Framework/Game.hpp>
#include <Framework/Client/GameClient.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/Threads/RenderThread.hpp>

#include <HyperionEngine.hpp>

#include <EditorSubsystem.generated.inl>

namespace Hyperion {

HYP_DEFINE_LOG_CHANNEL(Editor);

CVar<CVarString> g_cvCodeEditor { "Editor.CodeEditor", "VSCode" };
static CVar<bool> s_cvDebugDrawPhysics { "Physics.DebugDraw", false };

namespace CoreApi {
CORE_API extern const FilePath& GetExecutablePath();
} // namespace CoreApi

struct SuppressIdleThrottlingContext {};

static bool IsLoopbackHost(const UTF8StringView& host)
{
    return host == "localhost"
        || host == "::1"
        || host.FindFirstIndex("127.") == 0;
}

#pragma region EditorGizmoBase

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

#pragma endregion EditorGizmoBase

#pragma region TranslateEditorGizmo

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
        Array<Handle<Node>> selectedNodes = subsystem->GetSelectedNodes();

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

#pragma region Swatch Overrides

static SwatchOverrideSystem* GetSwatchOverrideSystemFor(const Entity* entity)
{
    if (!entity)
    {
        return nullptr;
    }

    World* world = entity->GetWorld();

    if (!world)
    {
        return nullptr;
    }

    return world->GetSystem<SwatchOverrideSystem>();
}

struct SwatchOverrideTransformEditState
{
    Handle<Entity> entity;
    Name swatch;
    bool routeToOverride = false;   // override mode: edits land in the active swatch's set
    bool wasOverridden = false;     // LocalTransform was overridden in the active swatch's set
    Transform preTransform;
    Transform postTransform;
};

/*! Captures swatch-override state for entities affected by a gizmo transform edit */
template <class T>
static Array<SwatchOverrideTransformEditState> CaptureSwatchOverrideTransformEdits(
    const Array<Pair<Handle<Node>, T>>& nodeData,
    bool overrideMode)
{
    Array<SwatchOverrideTransformEditState> result;

    for (const auto& pair : nodeData)
    {
        Handle<Entity> entityHandle = DynamicCast<Entity>(pair.first);

        if (!entityHandle.IsValid())
        {
            continue;
        }

        Entity* entity = entityHandle.Get();

        SwatchOverrideSystem* overrideSystem = GetSwatchOverrideSystemFor(entity);

        if (!overrideSystem)
        {
            continue;
        }

        World* world = entity->GetWorld();

        if (!world)
        {
            continue;
        }

        const Name activeSwatch = world->GetActiveSwatchName();

        if (!activeSwatch || IsDefaultSwatch(activeSwatch))
        {
            continue;
        }

        const bool hasSet = overrideSystem->HasSwatchOverrideSet(entity, activeSwatch);

        if (overrideMode)
        {
            if (!hasSet)
            {
                overrideSystem->AddSwatchOverrideSet(entity, activeSwatch);
            }

            if (overrideSystem->GetAppliedOverrideSwatch(entity) != activeSwatch)
            {
                overrideSystem->ApplyOverrides(entity, activeSwatch);
            }
        }
        else if (!hasSet)
        {
            // Plain base edit - nothing to keep in sync
            continue;
        }

        SwatchOverrideTransformEditState state;
        state.entity = entityHandle;
        state.swatch = activeSwatch;
        state.routeToOverride = overrideMode;
        state.wasOverridden = overrideSystem->IsPropertyOverriddenInSwatch(entity, activeSwatch, NAME("LocalTransform"));
        state.postTransform = entity->GetLocalTransform();

        if (state.wasOverridden)
        {
            BoxedValue preValue;

            if (overrideSystem->GetSwatchOverrideValue(entity, activeSwatch, NAME("LocalTransform"), preValue))
            {
                state.preTransform = preValue.Get<Transform>();
            }
            else
            {
                state.wasOverridden = false;
            }
        }

        if (overrideMode)
        {
            overrideSystem->SetSwatchOverrideValue(entity, activeSwatch, NAME("LocalTransform"), BoxedValue(state.postTransform));
        }

        result.PushBack(std::move(state));
    }

    return result;
}

// Shared execute/revert loops for gizmo undo actions
static void ExecuteSwatchOverrideTransformEdits(const Array<SwatchOverrideTransformEditState>& states)
{
    for (const SwatchOverrideTransformEditState& state : states)
    {
        if (!state.entity.IsValid() || (!state.routeToOverride && !state.wasOverridden))
        {
            continue;
        }

        if (SwatchOverrideSystem* overrideSystem = GetSwatchOverrideSystemFor(state.entity.Get()))
        {
            overrideSystem->SetSwatchOverrideValue(state.entity.Get(), state.swatch, NAME("LocalTransform"), BoxedValue(state.postTransform));
        }
    }
}

static void RevertSwatchOverrideTransformEdits(const Array<SwatchOverrideTransformEditState>& states)
{
    for (const SwatchOverrideTransformEditState& state : states)
    {
        if (!state.entity.IsValid() || (!state.routeToOverride && !state.wasOverridden))
        {
            continue;
        }

        SwatchOverrideSystem* overrideSystem = GetSwatchOverrideSystemFor(state.entity.Get());

        if (!overrideSystem)
        {
            continue;
        }

        if (state.wasOverridden)
        {
            overrideSystem->SetSwatchOverrideValue(state.entity.Get(), state.swatch, NAME("LocalTransform"), BoxedValue(state.preTransform));
        }
        else
        {
            overrideSystem->RemoveSwatchOverrideValue(state.entity.Get(), state.swatch, NAME("LocalTransform"));
        }
    }
}

#pragma endregion Swatch Overrides

void TranslateEditorGizmo::OnDragEnd(const Handle<Camera>& camera, const MouseEvent& mouseEvent)
{
    EditorGizmoBase::OnDragEnd(camera, mouseEvent);

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

            project->GetActionStack()->PushAction(MakeHandle<FunctionalEditorAction>(
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

    const bool snapToGrid = GetEditorSubsystem() && GetEditorSubsystem()->IsSnapToGridEnabled();

    Vec3f translation;

    if (m_dragData->axisDirection == Vec3f::Zero())
    {
        Vec3f delta = planeRayHit.hitpoint - m_dragData->hitpointOrigin;

        if (snapToGrid)
        {
        delta = MathUtil::Round(delta);
        }

        translation = m_dragData->nodeOrigin + delta;
    }
    else
    {
        float t = (planeRayHit.hitpoint - m_dragData->hitpointOrigin).Dot(m_dragData->axisDirection);

        if (snapToGrid)
        {
            t = MathUtil::Round(t);
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

#pragma endregion TranslateEditorGizmo

#pragma region RotateEditorGizmo

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
    dragData.axis = focusedNode->GetWorldRotation().RotateVector(dragData.axis).Normalize();

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
        Array<Handle<Node>> selectedNodes = subsystem->GetSelectedNodes();

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

    if (Handle<EditorProject> project = GetCurrentProject(); project.IsValid())
    {
        if (Handle<Node> focusedNode = m_focusedNode.Lock(); focusedNode.IsValid())
        {
            const Quat4f finalRotation = m_dragData->currentRotation;
            const Quat4f originRotation = m_dragData->startRotation;
            const Quat4f deltaRotation = finalRotation * originRotation.Inverse();

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

            project->GetActionStack()->PushAction(MakeHandle<FunctionalEditorAction>(
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

                                selectedNode->SetWorldRotation(deltaRotation * pair.second);
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
    const float angle = std::atan2(sinAngle, cosAngle);

    Quat4f deltaRotation = Quat4f::AxisAngles(m_dragData->axis, angle).Inverse();
    Quat4f newRotation = deltaRotation * m_dragData->startRotation;

    m_dragData->currentRotation = newRotation;

    Handle<Node> focusedNode = m_focusedNode.Lock();

    if (!focusedNode.IsValid())
    {
        return false;
    }

    NodeUnlockTransformScope unlockTransformScope(*focusedNode);
    focusedNode->SetWorldRotation(newRotation);

    // Apply the same delta rotation to all selected nodes
    deltaRotation = newRotation * m_dragData->startRotation.Inverse();

    for (const auto& pair : m_selectedNodes)
    {
        const Handle<Node>& selectedNode = pair.first;

        if (!selectedNode.IsValid())
        {
            continue;
        }

        selectedNode->SetWorldRotation(deltaRotation * pair.second);
    }

    return true;
}

bool RotateEditorGizmo::OnKeyPress(const Handle<Camera>& camera, const KeyboardEvent& keyboardEvent, const Handle<Node>& node)
{
    return false;
}

#pragma endregion RotateEditorGizmo

#pragma region ScaleEditorGizmo

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

    const Vec3f axisDirections[3] = { Vec3f(1.0f, 0.0f, 0.0f), Vec3f(0.0f, 1.0f, 0.0f), Vec3f(0.0f, 0.0f, 1.0f) };

    Vec3f axisDirection;
    Vec3f planeNormal;

    if (axis >= 0)
    {
        axisDirection = axisDirections[axis];

        if (axis == 1)
        {
            planeNormal = axisDirection.Cross(camera->GetSideVector()).Normalize();
        }
        else
        {
            planeNormal = axisDirection.Cross(camera->GetUpVector()).Normalize();
        }
    }
    else
    {
        axisDirection = Vec3f::Zero();
        planeNormal = -cameraDirection;
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
        Array<Handle<Node>> selectedNodes = subsystem->GetSelectedNodes();

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

            project->GetActionStack()->PushAction(MakeHandle<FunctionalEditorAction>(
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
        const Vec3f axisDirections[3] = { Vec3f(1.0f, 0.0f, 0.0f), Vec3f(0.0f, 1.0f, 0.0f), Vec3f(0.0f, 0.0f, 1.0f) };
        const Vec3f& axisDir = axisDirections[m_dragData->axis];

        const float initialProj = (m_dragData->hitpointOrigin - m_dragData->nodeOrigin).Dot(axisDir);
        const float currentProj = (planeRayHit.hitpoint - m_dragData->nodeOrigin).Dot(axisDir);

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

#pragma endregion ScaleEditorGizmo

#pragma region VolumeEditorGizmo

enum VolumeEditorFace : int
{
    VEF_PosX, // +X face (max.x side)
    VEF_NegX, // -X face (min.x side)
    VEF_PosY, // +Y face (max.y side)
    VEF_NegY, // -Y face (min.y side)
    VEF_PosZ, // +Z face (max.z side)
    VEF_NegZ, // -Z face (min.z side)

    VEF_Max
};

static constexpr Vec3f GetFaceNormal(int faceIndex)
{
    constexpr Vec3f FaceNormals[VEF_Max] = {
        Vec3f(1.0f, 0.0f, 0.0f),
        Vec3f(-1.0f, 0.0f, 0.0f),
        Vec3f(0.0f, 1.0f, 0.0f),
        Vec3f(0.0f, -1.0f, 0.0f),
        Vec3f(0.0f, 0.0f, 1.0f),
        Vec3f(0.0f, 0.0f, -1.0f)
    };

    return FaceNormals[faceIndex];
}

VolumeEditorGizmo::VolumeEditorGizmo()
    : EditorGizmoBase(),
      m_currentBounds(BoundingBox::Zero())
{
}

void VolumeEditorGizmo::UpdateFaceGeometry(const BoundingBox& localBounds, const Vec3f& worldTranslation)
{
    if (!m_node.IsValid())
    {
        return;
    }

    const Vec3f center = localBounds.GetCenter() - worldTranslation;
    const Vec3f extent = localBounds.GetExtent();
    const Vec3f halfExtent = extent * 0.5f;

    for (int i = 0; i < VEF_Max; i++)
    {
        Node* faceNode = m_node->FindChildByName(StringHash(HYP_FORMAT("VolumeFace_{}", i)));

        if (!faceNode)
        {
            continue;
        }

        const Vec3f normal = GetFaceNormal(i);
        Vec3f faceCenter = center + normal * halfExtent[i / 2];
        Vec3f faceScale;

        switch (i)
        {
        case VEF_PosX: // fallthrough
        case VEF_NegX:
            // Face in YZ plane
            faceScale = Vec3f(halfExtent.z, halfExtent.y, 1.0f);
            break;
        case VEF_PosY: // fallthrough
        case VEF_NegY:
            // Face in XZ plane
            faceScale = Vec3f(halfExtent.x, halfExtent.z, 1.0f);
            break;
        case VEF_PosZ: // fallthrough
        case VEF_NegZ:
            // Face in XY plane
            faceScale = Vec3f(halfExtent.x, halfExtent.y, 1.0f);
            break;
        }

        faceNode->UnlockTransform();
        faceNode->SetLocalTranslation(faceCenter);
        faceNode->SetLocalScale(faceScale);
    }

    m_node->UnlockTransform();
    m_node->SetWorldTranslation(worldTranslation);
}

Handle<Node> VolumeEditorGizmo::Load_Internal() const
{
    GlobalContextScope assetRegistryScope { AssetRegistryContext { GetEditorAssetRegistry() } };

    if (Handle<Prefab> prefab = GetCurrentAssetRegistry()->GetAsset<Prefab>(AssetBuckets::Prefabs, "VolumeEditGizmo"_sh); prefab.IsValid())
    {
        Handle<Node> rootNode = prefab->GetRoot();

        if (rootNode.IsValid())
        {
            for (const Handle<Node>& child : rootNode->GetChildren())
            {
                if (Entity* entity = DynamicCast<Entity>(child.Get()))
                {
                    if (VisibilityStateComponent* visibilityState = entity->TryGetComponent<VisibilityStateComponent>())
                    {
                        visibilityState->flags |= VisibilityStateFlags::ALWAYS_VISIBLE;
                    }
                    else
                    {
                        entity->AddComponent<VisibilityStateComponent>(VisibilityStateComponent { VisibilityStateFlags::ALWAYS_VISIBLE });
                    }
                }
            }
        }

        return rootNode;
    }

    return Handle<Node>::Null();
}

void VolumeEditorGizmo::SetFocusedNode(const Handle<Node>& focusedNode)
{
    EditorGizmoBase::SetFocusedNode(focusedNode);

    if (!focusedNode.IsValid() || !m_node.IsValid())
    {
        return;
    }

    m_currentBounds = focusedNode->GetWorldBounds();

    if (!m_currentBounds.IsValid() || !m_currentBounds.IsFinite() || m_currentBounds.IsZero())
    {
        // unbounded volumes (e.g. sky probe) cannot be reshaped
        m_focusedNode.Reset();

        return;
    }

    UpdateFaceGeometry(m_currentBounds, focusedNode->GetWorldTranslation());
}

void VolumeEditorGizmo::OnDragStart(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node, const Vec3f& hitpoint)
{
    EditorGizmoBase::OnDragStart(camera, mouseEvent, node, hitpoint);

    m_dragData.Unset();

    Entity* entity = DynamicCast<Entity>(node);
    if (!entity)
    {
        return;
    }

    const NodeTag& faceTag = node->GetTag("VolumeFaceIndex"_sh);

    if (!faceTag)
    {
        return;
    }

    const int faceIndex = faceTag.data.TryGet<int>(-1);

    if (faceIndex < 0 || faceIndex >= VEF_Max)
    {
        return;
    }

    Handle<Node> focusedNode = m_focusedNode.Lock();

    if (!focusedNode.IsValid())
    {
        return;
    }

    const Vec3f faceNormal = GetFaceNormal(faceIndex);

    Vec3f planeNormal;

    if (faceIndex / 2 == 1) // y axis
    {
        planeNormal = faceNormal.Cross(camera->GetSideVector()).Normalize();
    }
    else
    {
        planeNormal = faceNormal.Cross(camera->GetUpVector()).Normalize();
    }

    if (planeNormal.LengthSquared() < MathUtil::epsilonF)
    {
        planeNormal = -camera->GetDirection();
    }

    DragData dragData {};
    dragData.faceIndex = faceIndex;
    dragData.faceNormal = faceNormal;
    dragData.planePoint = hitpoint;
    dragData.planeNormal = planeNormal;
    dragData.hitOffset = (hitpoint - focusedNode->GetWorldTranslation()).Dot(faceNormal);
    dragData.originalBounds = m_currentBounds;

    m_dragData = dragData;
}

void VolumeEditorGizmo::OnDragEnd(const Handle<Camera>& camera, const MouseEvent& mouseEvent)
{
    EditorGizmoBase::OnDragEnd(camera, mouseEvent);

    // @TODO we should show a "Commit" ui button, and when clicked, that will actually set the

    if (Handle<EditorProject> project = GetCurrentProject(); project.IsValid())
    {
        if (Handle<Node> focusedNode = m_focusedNode.Lock(); focusedNode.IsValid())
        {
            if (m_dragData)
            {
                const BoundingBox finalBounds = m_currentBounds;
                const BoundingBox originalBounds = m_dragData->originalBounds;

                project->GetActionStack()->PushAction(MakeHandle<FunctionalEditorAction>(
                    "Edit Volume Shape",
                    [manipulationMode = GetManipulationMode(), focusedNode, finalBounds, originalBounds]() -> EditorActionFunctions
                    {
                        return {
                            [focusedNode, finalBounds, manipulationMode](EditorSubsystem* editorSubsystem, EditorProject*)
                            {
                                BoundingBox finalBoundsLocal = finalBounds;
                                finalBoundsLocal = focusedNode->GetWorldMatrix().Inverse() * finalBoundsLocal;

                                focusedNode->SetLocalBounds(finalBoundsLocal);

                                editorSubsystem->SetSelectedManipulationMode(manipulationMode);
                                editorSubsystem->SetFocusedNode(focusedNode, true);
                            },
                            [focusedNode, originalBounds, manipulationMode](EditorSubsystem* editorSubsystem, EditorProject*)
                            {
                                BoundingBox originalBoundsLocal = originalBounds;
                                originalBoundsLocal = focusedNode->GetWorldMatrix().Inverse() * originalBoundsLocal;

                                focusedNode->SetLocalBounds(originalBoundsLocal);

                                editorSubsystem->SetSelectedManipulationMode(manipulationMode);
                                editorSubsystem->SetFocusedNode(focusedNode, true);
                            }
                        };
                    }));
            }
        }
    }

    m_dragData.Unset();
}

bool VolumeEditorGizmo::OnMouseHover(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node)
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
    newParameters.albedo = Vec4f(0.7f, 0.35f, 0.0f, 0.35f);
    meshComponent->material->SetParameters(newParameters);

    return true;
}

bool VolumeEditorGizmo::OnMouseLeave(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node)
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

    // Restore original face color
    if (const NodeTag& tag = node->GetTag("TransformWidgetElementColor"_sh))
    {
        MaterialParameters newParameters = meshComponent->material->GetParameters();
        newParameters.albedo = tag.data.TryGet<Vec4f>(Vec4f::Zero());

        meshComponent->material->SetParameters(newParameters);
    }

    return true;
}

bool VolumeEditorGizmo::OnMouseMove(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node)
{
    if (!mouseEvent.mouseButtons[MouseButtonState::LEFT])
    {
        return false;
    }

    if (!m_dragData)
    {
        return false;
    }

    Handle<Node> focusedNode = m_focusedNode.Lock();

    if (!focusedNode.IsValid())
    {
        return false;
    }

    AssertDebug(mouseEvent.baseEvent->GetWindow() != nullptr);

    InputManager* inputMgr = mouseEvent.baseEvent->GetWindow()->GetInputManager();
    AssertDebug(inputMgr != nullptr);

    const Ray ray = camera->GetPickRay(inputMgr->GetVirtualMousePositionNormalized());

    RayHit planeRayHit;

    if (Optional<RayHit> planeRayHitOpt = ray.TestPlane(m_dragData->planePoint, m_dragData->planeNormal))
    {
        planeRayHit = *planeRayHitOpt;
    }
    else
    {
        return true;
    }

    const Vec3f worldOffset = planeRayHit.hitpoint - m_dragData->planePoint;
    const int faceIndex = m_dragData->faceIndex;
    const int axis = faceIndex / 2; // 0=X, 1=Y, 2=Z
    Vec3f axisDirection = Vec3f::Zero();
    axisDirection[axis] = 1.0f;
    const float displacement = worldOffset.Dot(axisDirection);

    BoundingBox newBounds = m_dragData->originalBounds;

    if (faceIndex % 2 == 0)
    {
        // positive face
        newBounds.max[axis] = m_dragData->originalBounds.max[axis] + displacement;

        // clamp
        if (newBounds.max[axis] < newBounds.min[axis] + MathUtil::epsilonF)
        {
            newBounds.max[axis] = newBounds.min[axis] + MathUtil::epsilonF;
        }
    }
    else // negative
    {
        newBounds.min[axis] = m_dragData->originalBounds.min[axis] + displacement;

        // clamp
        if (newBounds.min[axis] > newBounds.max[axis] - MathUtil::epsilonF)
        {
            newBounds.min[axis] = newBounds.max[axis] - MathUtil::epsilonF;
        }
    }

    m_currentBounds = newBounds;

    // set new bounds
    // const BoundingBox newBoundsLocal = focusedNode->GetWorldMatrix().Inverse() * newBounds;
    // focusedNode->SetLocalBounds(newBoundsLocal);

    UpdateFaceGeometry(newBounds, focusedNode->GetWorldTranslation());

    return true;
}

bool VolumeEditorGizmo::OnKeyPress(const Handle<Camera>& camera, const KeyboardEvent& keyboardEvent, const Handle<Node>& node)
{
    return false;
}

#pragma endregion VolumeEditorGizmo

#pragma region Terrain

Handle<EditorTerrainState> EditorSubsystem::GetTerrainState()
{
    if (!m_terrainSculpting.IsValid())
    {
        m_terrainSculpting = MakeHandle<EditorTerrainState>();
        InitObject(m_terrainSculpting);

        m_terrainSculpting->Initialize(this);
    }

    return m_terrainSculpting;
}

#pragma endregion Terrain

bool EditorSubsystem::IsMeshEditModeEnabled() const
{
    AssertOnThread(g_simThread);

    return m_meshEditState.enabled;
}

Node* EditorSubsystem::ResolveMeshEditTarget(MeshComponent** outMeshComponent) const
{
    Handle<Node> node = m_meshEditState.enabled
        ? m_meshEditState.targetNode.Lock()
        : m_focusedNode.Lock();

    if (!node.IsValid())
    {
        return nullptr;
    }

    Entity* entity = DynamicCast<Entity>(node.Get());

    if (!entity)
    {
        return nullptr;
    }

    MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>();

    if (!meshComponent || !meshComponent->mesh.IsValid())
    {
        return nullptr;
    }

    if (outMeshComponent)
    {
        *outMeshComponent = meshComponent;
    }

    return node.Get();
}

bool EditorSubsystem::CanEnableMeshEditMode() const
{
    if (m_meshEditState.enabled)
    {
        return true;
    }

    if (!m_currentProject.IsValid() || IsSimulating())
    {
        return false;
    }

    return ResolveMeshEditTarget() != nullptr;
}

Node* EditorSubsystem::GetMeshEditTargetNode() const
{
    if (!m_meshEditState.enabled)
    {
        return nullptr;
    }

    return m_meshEditState.targetNode.Lock().Get();
}

bool EditorSubsystem::HasMeshEditFaceSelected() const
{
    return m_meshEditState.selectedFace.HasValue();
}

bool EditorSubsystem::IsMeshEditDragActive() const
{
    return m_meshEditState.dragData.HasValue();
}

int EditorSubsystem::GetMeshEditLockedAxis() const
{
    if (!m_meshEditState.dragData)
    {
        return -1;
    }

    return m_meshEditState.dragData->lockedAxis;
}

bool EditorSubsystem::HasPendingMeshEdits() const
{
    return m_meshEditState.baselinePositions.Any();
}

EditorActionStack* EditorSubsystem::GetActiveActionStack() const
{
    if (m_meshEditState.enabled && m_meshEditState.actionStack.IsValid())
    {
        return m_meshEditState.actionStack.Get();
    }

    if (!m_currentProject.IsValid())
    {
        return nullptr;
    }

    return m_currentProject->GetActionStack().Get();
}

bool EditorSubsystem::IsSimulating() const
{
    // If the pre-sim project is set, we're in simulation mode
    return m_preSimulationProject.IsValid();
}

bool EditorSubsystem::IsSnapToGridEnabled() const
{
    //AssertOnThread(g_simThread);

    return m_snapToGridEnabled;
}

void EditorSubsystem::SetSnapToGridEnabled(bool snapToGrid)
{
    //AssertOnThread(g_simThread);

    m_snapToGridEnabled = snapToGrid;
}

#pragma region Entity Swatch Overrides

Array<Name> EditorSubsystem::GetEntitySwatchOverrideSets(Entity* entity) const
{
    if (SwatchOverrideSystem* overrideSystem = GetSwatchOverrideSystemFor(entity))
    {
        return overrideSystem->GetSetSwatchNames(entity);
    }

    return {};
}

bool EditorSubsystem::EntityHasSwatchOverrideSet(Entity* entity, Name swatchName) const
{
    SwatchOverrideSystem* overrideSystem = GetSwatchOverrideSystemFor(entity);

    return overrideSystem && overrideSystem->HasSwatchOverrideSet(entity, swatchName);
}

bool EditorSubsystem::EntityHasSwatchOverrideValues(Entity* entity, Name swatchName) const
{
    SwatchOverrideSystem* overrideSystem = GetSwatchOverrideSystemFor(entity);

    return overrideSystem && overrideSystem->HasAnyOverriddenProperty(entity, swatchName);
}

void EditorSubsystem::EntityAddSwatchOverrideSet(Entity* entity, Name swatchName) const
{
    if (SwatchOverrideSystem* overrideSystem = GetSwatchOverrideSystemFor(entity))
    {
        overrideSystem->AddSwatchOverrideSet(entity, swatchName);
    }
}

bool EditorSubsystem::EntityRemoveSwatchOverrideSet(Entity* entity, Name swatchName) const
{
    SwatchOverrideSystem* overrideSystem = GetSwatchOverrideSystemFor(entity);

    return overrideSystem && overrideSystem->RemoveSwatchOverrideSet(entity, swatchName);
}

bool EditorSubsystem::IsEntityPropertyOverridden(Entity* entity, Name swatchName, Name propertyName) const
{
    SwatchOverrideSystem* overrideSystem = GetSwatchOverrideSystemFor(entity);

    return overrideSystem && overrideSystem->IsPropertyOverriddenInSwatch(entity, swatchName, propertyName);
}

bool EditorSubsystem::EntityRemoveSwatchOverrideValue(Entity* entity, Name swatchName, Name propertyName) const
{
    SwatchOverrideSystem* overrideSystem = GetSwatchOverrideSystemFor(entity);

    return overrideSystem && overrideSystem->RemoveSwatchOverrideValue(entity, swatchName, propertyName);
}

Name EditorSubsystem::GetEntityAppliedOverrideSwatch(Entity* entity) const
{
    if (SwatchOverrideSystem* overrideSystem = GetSwatchOverrideSystemFor(entity))
    {
        return overrideSystem->GetAppliedOverrideSwatch(entity);
    }

    return Name::Invalid();
}

void EditorSubsystem::EntityApplySwatchOverrides(Entity* entity, Name swatchName) const
{
    if (SwatchOverrideSystem* overrideSystem = GetSwatchOverrideSystemFor(entity))
    {
        overrideSystem->ApplyOverrides(entity, swatchName);
    }
}

void EditorSubsystem::EntityRevertSwatchOverrides(Entity* entity) const
{
    if (SwatchOverrideSystem* overrideSystem = GetSwatchOverrideSystemFor(entity))
    {
        overrideSystem->RevertOverrides(entity);
    }
}

#pragma endregion Entity Swatch Overrides

#pragma region MeshEditMode

static Vec3f ReadMeshVertexPosition(const VertexArrayView& vertexView, size_t vertexSizeInFloats, uint32 vertexIndex)
{
    const TVertexPacket<VT_Position>* packet = reinterpret_cast<const TVertexPacket<VT_Position>*>(
        vertexView.floatData + (vertexIndex * vertexSizeInFloats));

    return Vec3f(packet->posX, packet->posY, packet->posZ);
}

static Optional<uint32> FindCoplanarAdjacentTriangle(
    Mesh* mesh,
    uint8 lodIndex,
    uint32 triangleIndex,
    Span<const uint32> triangleVertexIndices)
{
    auto readScope = mesh->GetReadScope();

    const VertexArrayView vertexView = mesh->GetVertexData(lodIndex);
    const size_t vertexSizeInFloats = vertexView.layoutDesc.VertexSize() / sizeof(float);

    const Vec3f trianglePositions[3] = {
        ReadMeshVertexPosition(vertexView, vertexSizeInFloats, triangleVertexIndices[0]),
        ReadMeshVertexPosition(vertexView, vertexSizeInFloats, triangleVertexIndices[1]),
        ReadMeshVertexPosition(vertexView, vertexSizeInFloats, triangleVertexIndices[2])
    };

    const Vec3f triangleNormal = (trianglePositions[1] - trianglePositions[0])
                                      .Cross(trianglePositions[2] - trianglePositions[0])
                                      .Normalized();

    const Span<const ubyte> indexData = mesh->GetIndexData(lodIndex);
    const Span<const uint32> indices(reinterpret_cast<const uint32*>(indexData.Data()), indexData.Size() / sizeof(uint32));

    const uint32 numTriangles = uint32(indices.Size() / 3);

    // Vertex-normal deviation allowed for a neighboring triangle to still be considered part of the same quad.
    constexpr float coplanarNormalEpsilon = 0.01f;

    for (uint32 candidateTriangleIndex = 0; candidateTriangleIndex < numTriangles; candidateTriangleIndex++)
    {
        if (candidateTriangleIndex == triangleIndex)
        {
            continue;
        }

        const uint32 candidateVertexIndices[3] = {
            indices[candidateTriangleIndex * 3 + 0],
            indices[candidateTriangleIndex * 3 + 1],
            indices[candidateTriangleIndex * 3 + 2]
        };

        uint32 sharedVertexCount = 0;

        for (uint32 candidateVertexIndex : candidateVertexIndices)
        {
            if (std::find(triangleVertexIndices.Begin(), triangleVertexIndices.End(), candidateVertexIndex) != triangleVertexIndices.End())
            {
                sharedVertexCount++;
            }
        }

        // A quad neighbor shares exactly one edge (two vertices) with the picked triangle.
        if (sharedVertexCount != 2)
        {
            continue;
        }

        const Vec3f candidatePositions[3] = {
            ReadMeshVertexPosition(vertexView, vertexSizeInFloats, candidateVertexIndices[0]),
            ReadMeshVertexPosition(vertexView, vertexSizeInFloats, candidateVertexIndices[1]),
            ReadMeshVertexPosition(vertexView, vertexSizeInFloats, candidateVertexIndices[2])
        };

        const Vec3f candidateNormal = (candidatePositions[1] - candidatePositions[0])
                                           .Cross(candidatePositions[2] - candidatePositions[0])
                                           .Normalized();

        if (candidateNormal.Dot(triangleNormal) >= 1.0f - coplanarNormalEpsilon)
        {
            return candidateTriangleIndex;
        }
    }

    return {};
}

static Array<uint32, EditorAllocator> FindWeldedVertexIndices(
    Mesh* mesh,
    uint8 lodIndex,
    Span<const uint32> faceVertexIndices)
{
    auto readScope = mesh->GetReadScope();

    const VertexArrayView vertexView = mesh->GetVertexData(lodIndex);
    const size_t vertexSizeInFloats = vertexView.layoutDesc.VertexSize() / sizeof(float);

    Array<Vec3f, EditorAllocator> faceVertexPositions;
    faceVertexPositions.Reserve(faceVertexIndices.Size());

    for (uint32 vertexIndex : faceVertexIndices)
    {
        faceVertexPositions.PushBack(ReadMeshVertexPosition(vertexView, vertexSizeInFloats, vertexIndex));
    }

    Array<uint32, EditorAllocator> affectedVertexIndices = faceVertexIndices;

    static constexpr float WeldDistanceSquared = 0.0001f * 0.0001f;

    for (uint32 candidateVertexIndex = 0; candidateVertexIndex < vertexView.vertexCount; candidateVertexIndex++)
    {
        if (affectedVertexIndices.Contains(candidateVertexIndex))
        {
            continue;
        }

        const Vec3f candidatePosition = ReadMeshVertexPosition(vertexView, vertexSizeInFloats, candidateVertexIndex);

        for (const Vec3f& faceVertexPosition : faceVertexPositions)
        {
            if ((candidatePosition - faceVertexPosition).LengthSquared() <= WeldDistanceSquared)
            {
                affectedVertexIndices.PushBack(candidateVertexIndex);
                break;
            }
        }
    }

    return affectedVertexIndices;
}

static void ApplyMeshEditVertexPositions(
    const Handle<Node>& node,
    uint8 lodIndex,
    const Array<uint32, EditorAllocator>& vertexIndices,
    const Array<Vec3f, EditorAllocator>& localPositions,
    bool recomputeDerivedData)
{
    Entity* entity = DynamicCast<Entity>(node.Get());
    MeshComponent* meshComponent = entity ? entity->TryGetComponent<MeshComponent>() : nullptr;

    if (!meshComponent || !meshComponent->mesh.IsValid())
    {
        return;
    }

    Mesh* mesh = meshComponent->mesh;

    {
        Array<float, EditorAllocator> mutableVertexData;
        Array<ubyte, EditorAllocator> indexData;

        size_t vertexSizeInFloats;
        VertexInputLayoutDesc layoutDesc;

        { // read scope needed to access the data.
            auto readScope = mesh->GetReadScope();

            const VertexArrayView vertexView = mesh->GetVertexData(lodIndex);

            layoutDesc = vertexView.layoutDesc;
            vertexSizeInFloats = vertexView.layoutDesc.VertexSize() / sizeof(float);

            mutableVertexData = Array<float, EditorAllocator>(vertexView.floatData, vertexView.vertexCount * vertexSizeInFloats);
            indexData = mesh->GetIndexData(lodIndex);
        }

        Assert(vertexSizeInFloats != 0);

        auto writeScope = mesh->GetWriteScope();

        for (size_t i = 0; i < vertexIndices.Size(); i++)
        {
            TVertexPacket<VT_Position>* packet = reinterpret_cast<TVertexPacket<VT_Position>*>(
                mutableVertexData.Data() + (vertexIndices[i] * vertexSizeInFloats));

            packet->posX = localPositions[i].x;
            packet->posY = localPositions[i].y;
            packet->posZ = localPositions[i].z;
        }

        VertexArrayView newVertexView {};
        newVertexView.floatData = mutableVertexData.Data();
        newVertexView.vertexCount = mutableVertexData.Size() / vertexSizeInFloats;
        newVertexView.layoutDesc = layoutDesc;

        mesh->SetVertexData(lodIndex, newVertexView);

        // We set index data anyway, since it'll need to be resident in memory,
        // which is not guaranteed since we only have a write scope active.
        // Hacky solution, but it works.
        mesh->SetIndexData(lodIndex, indexData);

        const BoundingBox meshBounds = mesh->CalculateAABB();

        if (recomputeDerivedData)
        {
            mesh->SetAABB(meshBounds);

            BVHNode bvh;
            mesh->BuildBVH(bvh);

            mesh->SetBVH(std::move(bvh));
        }

        entity->SetLocalBounds(meshBounds);
    }

    // Update editor pick cache, so we don't test against old verts
    g_editorState->GetPickCache().PutEntry(mesh, /* invalidate */ true);

    mesh->UploadGpuData();
}

static bool ReadAllMeshVertexPositions(Mesh* mesh, uint8 lodIndex, Array<Vec3f, EditorAllocator>& outPositions)
{
    if (!mesh)
    {
        return false;
    }

    auto readScope = mesh->GetReadScope();

    if (!readScope)
    {
        return false;
    }

    const VertexArrayView vertexView = mesh->GetVertexData(lodIndex);
    const size_t vertexSizeInFloats = vertexView.layoutDesc.VertexSize() / sizeof(float);

    outPositions.Clear();
    outPositions.Reserve(vertexView.vertexCount);

    for (uint32 vertexIndex = 0; vertexIndex < vertexView.vertexCount; vertexIndex++)
    {
        outPositions.PushBack(ReadMeshVertexPosition(vertexView, vertexSizeInFloats, vertexIndex));
    }

    return true;
}

static void WriteAllMeshVertexPositions(
    const Handle<Node>& node,
    uint8 lodIndex,
    const Array<Vec3f, EditorAllocator>& positions)
{
    Entity* entity = DynamicCast<Entity>(node.Get());
    MeshComponent* meshComponent = entity ? entity->TryGetComponent<MeshComponent>() : nullptr;

    if (!meshComponent || !meshComponent->mesh.IsValid())
    {
        return;
    }

    Array<uint32, EditorAllocator> vertexIndices;
    vertexIndices.Reserve(positions.Size());

    for (uint32 i = 0; i < uint32(positions.Size()); i++)
    {
        vertexIndices.PushBack(i);
    }

    ApplyMeshEditVertexPositions(node, lodIndex, vertexIndices, positions, /* recomputeDerivedData */ true);
}

void EditorSubsystem::EnterMeshEditMode()
{
    AssertOnThread(g_simThread);

    if (m_meshEditState.enabled)
    {
        return;
    }

    if (!m_currentProject.IsValid())
    {
        return;
    }

    Node* target = ResolveMeshEditTarget();

    if (!target)
    {
        HYP_LOG(Editor, Warning, "Cannot enter mesh edit mode: the focused node has no editable mesh");

        return;
    }

    m_meshEditState.targetNode = MakeWeakRef(target);

    m_meshEditState.manipulationModeBeforeMeshEdit = m_selectedManipulationMode;

    m_meshEditState.enabled = true;

    m_meshEditState.actionStack = MakeHandle<EditorActionStack>(m_currentProject.ToWeak());

    m_meshEditState.baselinePositions.Clear();
    m_meshEditState.baselineMesh.Reset();

    m_meshEditState.isChanging = true;
    SetSelectedManipulationMode(EditorManipulationMode::None);
    m_meshEditState.isChanging = false;

    OnMeshEditStateChanged();
}

void EditorSubsystem::ExitMeshEditMode(bool saveEdits)
{
    AssertOnThread(g_simThread);

    if (!m_meshEditState.enabled)
    {
        return;
    }

    EndMeshEditDrag(/* saveEdits */ true);

    if (saveEdits)
    {
        CommitMeshEdits();
    }
    else
    {
        DiscardMeshEdits();
    }

    m_meshEditState.enabled = false;

    SetSelectedMeshEditFace({});

    m_meshEditState.hoveredFace.Unset();
    m_meshEditState.targetNode.Reset();
    m_meshEditState.actionStack.Reset();

    m_meshEditState.isChanging = true;
    SetSelectedManipulationMode(m_meshEditState.manipulationModeBeforeMeshEdit);
    m_meshEditState.isChanging = false;

    OnMeshEditStateChanged();
}

bool EditorSubsystem::BackOutOfMeshEditState()
{
    if (!m_meshEditState.enabled)
    {
        return false;
    }

    if (IsMeshEditDragActive())
    {
        EndMeshEditDrag(false);

        return true;
    }

    if (m_meshEditState.selectedFace)
    {
        SetSelectedMeshEditFace({});

        return true;
    }

    ExitMeshEditMode(/* saveEdits */ true);

    return true;
}

MeshEditFaceMode EditorSubsystem::GetMeshEditFaceMode() const
{
    AssertOnThread(g_simThread);

    return m_meshEditState.faceMode;
}

void EditorSubsystem::SetMeshEditFaceMode(MeshEditFaceMode faceMode)
{
    AssertOnThread(g_simThread);

    if (faceMode == m_meshEditState.faceMode)
    {
        return;
    }

    m_meshEditState.faceMode = faceMode;

    // The existing selection was built for the other mode's vertex count, so it can't carry over.
    SetSelectedMeshEditFace({});
    m_meshEditState.hoveredFace.Unset();
}

bool EditorSubsystem::IsMeshEditAlignToNormal() const
{
    AssertOnThread(g_simThread);

    return m_meshEditState.alignToNormal;
}

void EditorSubsystem::SetMeshEditAlignToNormal(bool alignToNormal)
{
    AssertOnThread(g_simThread);

    if (alignToNormal == m_meshEditState.alignToNormal)
    {
        return;
    }

    m_meshEditState.alignToNormal = alignToNormal;

    OnMeshEditStateChanged();
}

void EditorSubsystem::CaptureMeshEditBaseline()
{
    AssertOnThread(g_simThread);

    if (m_meshEditState.baselinePositions.Any())
    {
        return;
    }

    MeshComponent* meshComponent = nullptr;
    Node* target = ResolveMeshEditTarget(&meshComponent);

    if (!target || !meshComponent)
    {
        return;
    }

    Mesh* mesh = meshComponent->mesh;

    if (!ReadAllMeshVertexPositions(mesh, /* lodIndex */ 0, m_meshEditState.baselinePositions))
    {
        HYP_LOG(Editor, Warning, "Failed to capture mesh edit baseline for {}; edits will not be undoable as a unit", target->GetName());

        return;
    }

    m_meshEditState.baselineMesh = MakeWeakRef(mesh);
}

void EditorSubsystem::SyncBoxPhysicsShapeToLocalBounds(Entity* entity)
{
    if (entity == nullptr)
    {
        return;
    }

    RigidBodyComponent* rigidBodyComponent = entity->TryGetComponent<RigidBodyComponent>();

    if (rigidBodyComponent == nullptr)
    {
        return;
    }

    if (!rigidBodyComponent->shape.IsValid() || rigidBodyComponent->shape->GetType() != PhysicsShapeType::Box)
    {
        return;
    }

    // Clone the shape first to prevent stomping something used by another
    Handle<PhysicsShape> uniqueShape = EnsureUniquePhysicsShape(entity);
    Handle<BoxPhysicsShape> boxShape = DynamicCast<BoxPhysicsShape>(uniqueShape);

    if (!boxShape.IsValid())
    {
        return;
    }

    boxShape->SetAABB(entity->GetLocalBounds());
    boxShape->Invalidate();

    entity->AddTag<EntityTag::UpdatePhysicsShape>();
}

void EditorSubsystem::CommitMeshEdits()
{
    AssertOnThread(g_simThread);

    if (!HasPendingMeshEdits())
    {
        return;
    }

    Handle<EditorProject> project = GetCurrentProject();
    Handle<Node> node = m_meshEditState.targetNode.Lock();
    Handle<Mesh> baselineMesh = m_meshEditState.baselineMesh.Lock();

    Array<Vec3f, EditorAllocator> baselinePositions = std::move(m_meshEditState.baselinePositions);

    m_meshEditState.baselinePositions.Clear();
    m_meshEditState.baselineMesh.Reset();

    if (!project.IsValid() || !node.IsValid() || !baselineMesh.IsValid())
    {
        return;
    }

    Array<Vec3f, EditorAllocator> finalPositions;

    if (!ReadAllMeshVertexPositions(baselineMesh.Get(), /* lodIndex */ 0, finalPositions))
    {
        HYP_LOG(Editor, Warning, "Failed to read final mesh state for {}; mesh edits will not be undoable", node->GetName());

        return;
    }

    if (finalPositions.Size() != baselinePositions.Size())
    {
        HYP_LOG(Editor, Warning, "Mesh {} changed vertex count during editing; cannot record an undoable action", node->GetName());

        return;
    }

    if (finalPositions == baselinePositions)
    {
        return;
    }

    project->GetActionStack()->PushAction(MakeHandle<FunctionalEditorAction>(
        "Apply Mesh Edits",
        [nodeWeak = node.ToWeak(), baselinePositions, finalPositions]() -> EditorActionFunctions
        {
            return {
                [nodeWeak, finalPositions](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                {
                    if (Handle<Node> node = nodeWeak.Lock(); node.IsValid())
                    {
                        WriteAllMeshVertexPositions(node, /* lodIndex */ 0, finalPositions);
                        editorSubsystem->SyncBoxPhysicsShapeToLocalBounds(DynamicCast<Entity>(node.Get()));
                    }
                },
                [nodeWeak, baselinePositions](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                {
                    if (Handle<Node> node = nodeWeak.Lock(); node.IsValid())
                    {
                        WriteAllMeshVertexPositions(node, /* lodIndex */ 0, baselinePositions);
                        editorSubsystem->SyncBoxPhysicsShapeToLocalBounds(DynamicCast<Entity>(node.Get()));
                    }
                }
            };
        }));
}

void EditorSubsystem::DiscardMeshEdits()
{
    AssertOnThread(g_simThread);

    if (!HasPendingMeshEdits())
    {
        return;
    }

    Handle<Node> node = m_meshEditState.targetNode.Lock();

    Array<Vec3f, EditorAllocator> baselinePositions = std::move(m_meshEditState.baselinePositions);

    m_meshEditState.baselinePositions.Clear();
    m_meshEditState.baselineMesh.Reset();

    if (!node.IsValid())
    {
        return;
    }

    WriteAllMeshVertexPositions(node, /* lodIndex */ 0, baselinePositions);
}

static void EnsureUniqueMeshEditTarget(const Handle<Node>& node, MeshComponent* meshComponent)
{
    if (node->GetTag("HYP_MeshEditUniqueMesh"_sh))
    {
        return;
    }

    Mesh* sourceMesh = meshComponent->mesh;
    Assert(sourceMesh != nullptr);

    if (!sourceMesh)
    {
        return;
    }

    Handle<Mesh> clonedMesh = MakeHandle<Mesh>();
    clonedMesh->SetName(sourceMesh->GetName()); // Will be unique'd anyway

    {
        // read scope needed to access the data.
        auto readScope = sourceMesh->GetReadScope();

        const MeshDesc meshDesc = sourceMesh->GetMeshDesc();
        const VertexArrayView vertexData = sourceMesh->GetVertexData(0);
        const Span<const ubyte> indexData = sourceMesh->GetIndexData(0);

        MeshDataView meshData {};
        meshData.vertices[0] = vertexData;
        meshData.indices[0] = ConstByteView(indexData.Data(), indexData.Data() + indexData.Size());

        clonedMesh->SetMeshData(meshDesc, meshData);

        readScope.Reset();

        BVHNode bvh;
        clonedMesh->BuildBVH(bvh);

        clonedMesh->SetBVH(std::move(bvh));
    }

    GetCurrentAssetRegistry()->PutAssetUnique(clonedMesh);

    clonedMesh->UploadGpuData();

    meshComponent->mesh = clonedMesh;

    node->AddTag(NodeTag(NAME("HYP_MeshEditUniqueMesh"), int(1)));

    if (Entity* entity = DynamicCast<Entity>(node.Get()))
    {
        entity->AddTag<EntityTag::UpdateRenderProxy>();
    }
}

bool EditorSubsystem::TryPickMeshEditFace(const Ray& ray, MeshEditFaceSelection& outSelection, bool ensureUniqueMesh)
{
    AssertOnThread(g_simThread);

    MeshComponent* meshComponent = nullptr;
    Node* targetNodeRaw = ResolveMeshEditTarget(&meshComponent);

    if (!targetNodeRaw || !meshComponent)
    {
        return false;
    }

    Handle<Node> targetNode = MakeStrongRef(targetNodeRaw);

    // TEMP DIAGNOSTIC: dump BVH/mesh state for this pick attempt.
    {
        Mesh* diagMesh = meshComponent->mesh;
        const BVHNode& diagBvh = diagMesh->GetBVH();
        auto diagReadScope = diagMesh->GetReadScope();
        const VertexArrayView diagVertexData = diagMesh->GetVertexData(0);
        const Span<const ubyte> diagIndexData = diagMesh->GetIndexData(0);

        HYP_LOG(Editor, Warning,
            "[MeshEditPickDiag] ray.pos={} ray.dir={} bvh.valid={} bvh.aabb=({} - {}) vertexCount={} indexCount={}",
            ray.position, ray.direction,
            diagBvh.IsValid(), diagBvh.aabb.GetMin(), diagBvh.aabb.GetMax(),
            diagVertexData.vertexCount, diagIndexData.Size() / sizeof(uint32));
    }

    RayTestResults results;

    const bool hadAnyHit = targetNode->TestRay(ray, results, RayTestFlags::TestBVH | RayTestFlags::EditorPick);

    HYP_LOG(Editor, Warning, "[MeshEditPickDiag] TestRay returned {} with {} total result(s)", hadAnyHit, results.Size());

    if (!hadAnyHit)
    {
        return false;
    }

    const RayHit* closestHit = nullptr;
    uint32 numHitsForTargetNode = 0;

    for (const RayHit& hit : results)
    {
        if (hit.node != targetNode.Get() || hit.triangleIndex == ~0u)
        {
            continue;
        }

        numHitsForTargetNode++;

        // // Reject backfaces.
        // if (hit.normal.Dot(-ray.direction) < FLT_EPSILON)
        // {
        //     continue;
        // }

        if (closestHit && hit.distance >= closestHit->distance)
        {
            continue;
        }

        closestHit = &hit;
    }

    HYP_LOG(Editor, Warning, "[MeshEditPickDiag] {} hit(s) matched target node; closestHit={} triangleIndex={} distance={}",
        numHitsForTargetNode,
        closestHit != nullptr,
        closestHit ? closestHit->triangleIndex : ~0u,
        closestHit ? closestHit->distance : -1.0f);

    if (!closestHit)
    {
        return false;
    }

    if (ensureUniqueMesh)
    {
        EnsureUniqueMeshEditTarget(targetNode, meshComponent);
    }

    Mesh* mesh = meshComponent->mesh;
    const uint8 lodIndex = 0;

    auto readScope = mesh->GetReadScope();

    const Span<const ubyte> indexData = mesh->GetIndexData(lodIndex);
    const Span<const uint32> indices(reinterpret_cast<const uint32*>(indexData.Data()), indexData.Size() / sizeof(uint32));

    const uint32 triangleIndex = closestHit->triangleIndex;

    Array<uint32, EditorAllocator> vertexIndices {
        indices[triangleIndex * 3 + 0],
        indices[triangleIndex * 3 + 1],
        indices[triangleIndex * 3 + 2]
    };

    if (m_meshEditState.faceMode == MeshEditFaceMode::Quad)
    {
        if (Optional<uint32> adjacentTriangleIndex = FindCoplanarAdjacentTriangle(mesh, lodIndex, triangleIndex, vertexIndices))
        {
            for (uint32 i = 0; i < 3; i++)
            {
                const uint32 vertexIndex = indices[(*adjacentTriangleIndex) * 3 + i];

                if (!vertexIndices.Contains(vertexIndex))
                {
                    vertexIndices.PushBack(vertexIndex);
                }
            }
        }
    }

    outSelection.node = targetNode.ToWeak();
    outSelection.vertexIndices = std::move(vertexIndices);
    outSelection.lodIndex = lodIndex;

    return true;
}

void EditorSubsystem::SetSelectedMeshEditFace(Optional<MeshEditFaceSelection> selection)
{
    AssertOnThread(g_simThread);

    EndMeshEditDrag(true);

    m_meshEditState.selectedFace = std::move(selection);

    OnMeshEditSelectionChanged();
    OnMeshEditStateChanged();
}

void EditorSubsystem::UpdateHoveredMeshEditFace(const Ray& ray)
{
    AssertOnThread(g_simThread);

    MeshEditFaceSelection hoveredFace;

    if (TryPickMeshEditFace(ray, hoveredFace, /* ensureUniqueMesh */ false))
    {
        m_meshEditState.hoveredFace = hoveredFace;
    }
    else
    {
        m_meshEditState.hoveredFace.Unset();
    }
}

// Mesh edit overlays draw *through* the geometry they annotate. Depth-testing them against the very
// surface they sit on makes the highlight z-fight and flicker, which reads as the tool being broken
// rather than as a depth artifact - so the overlay is drawn on top and alpha blended instead.
static RenderableAttributeSet MeshEditOverlayAttributes(FillMode fillMode)
{
    RenderableAttributeSet attributes;

    MeshAttributes& meshAttributes = attributes.GetMeshAttributes();
    meshAttributes.inputLayout = StaticVertexInputLayout<VT_Simple>;
    meshAttributes.topology = Topology::Triangles;

    MaterialAttributes& materialAttributes = attributes.GetMaterialAttributes();
    materialAttributes.bucket = RenderBucket::Debug;
    materialAttributes.fillMode = fillMode;
    materialAttributes.blendFunction = BlendFunction::AlphaBlending();
    materialAttributes.flags = MAF_NONE;

    return attributes;
}

// Axis-lock feedback reuses the conventional X=red / Y=green / Z=blue mapping, so a locked drag is
// identifiable at a glance without reading the status bar.
static Color MeshEditAxisColor(int axis)
{
    switch (axis)
    {
    case 0:
        return Color(1.0f, 0.25f, 0.3f, 1.0f);
    case 1:
        return Color(0.4f, 1.0f, 0.35f, 1.0f);
    case 2:
        return Color(0.3f, 0.55f, 1.0f, 1.0f);
    default:
        return Color(1.0f, 0.7f, 0.1f, 1.0f);
    }
}

// Draws a face as a translucent fill plus a solid wireframe. The wireframe is what actually makes
// the face readable - a flat fill against a lit surface of similar colour is easy to miss entirely.
static void DrawMeshEditFace(
    DebugDrawCommandList& debugDrawCommandList,
    Span<const Vec3f> worldPositions,
    const Color& fillColor,
    const Color& outlineColor)
{
    if (worldPositions.Size() < 3)
    {
        return;
    }

    static const RenderableAttributeSet fillAttributes = MeshEditOverlayAttributes(FillMode::Fill);
    static const RenderableAttributeSet outlineAttributes = MeshEditOverlayAttributes(FillMode::Line);

    debugDrawCommandList.triangle(worldPositions[0], worldPositions[1], worldPositions[2], fillColor, fillAttributes);
    debugDrawCommandList.triangle(worldPositions[0], worldPositions[1], worldPositions[2], outlineColor, outlineAttributes);

    if (worldPositions.Size() == 4)
    {
        debugDrawCommandList.triangle(worldPositions[0], worldPositions[2], worldPositions[3], fillColor, fillAttributes);
        debugDrawCommandList.triangle(worldPositions[0], worldPositions[2], worldPositions[3], outlineColor, outlineAttributes);
    }
}

// Resolves a face selection's vertices to world space. When \p localDelta is non-null the positions
// are offset by it in local space first, which is how the in-progress drag previews its result
// without having written anything back to the mesh yet.
static bool BuildMeshEditFaceWorldPositions(
    const Handle<Node>& node,
    const MeshEditFaceSelection& selection,
    const Vec3f* localDelta,
    const Array<Vec3f, EditorAllocator>* overrideLocalPositions,
    Array<Vec3f, EditorAllocator>& outWorldPositions)
{
    Entity* entity = DynamicCast<Entity>(node.Get());
    MeshComponent* meshComponent = entity ? entity->TryGetComponent<MeshComponent>() : nullptr;

    if (!meshComponent || !meshComponent->mesh.IsValid())
    {
        return false;
    }

    const Mat4f& worldMatrix = node->GetWorldMatrix();
    const uint32 faceVertexCount = uint32(selection.vertexIndices.Size());

    outWorldPositions.Clear();
    outWorldPositions.Reserve(faceVertexCount);

    const auto PushWorldPosition = [&](const Vec3f& localPosition)
    {
        Vec4f transformedPosition = worldMatrix.TransformVector(Vec4f(localPosition, 1.0f));
        transformedPosition /= transformedPosition.w;

        outWorldPositions.PushBack(transformedPosition.GetXYZ());
    };

    if (overrideLocalPositions)
    {
        if (overrideLocalPositions->Size() < faceVertexCount)
        {
            return false;
        }

        for (uint32 i = 0; i < faceVertexCount; i++)
        {
            PushWorldPosition((*overrideLocalPositions)[i] + (localDelta ? *localDelta : Vec3f::Zero()));
        }

        return true;
    }

    Mesh* mesh = meshComponent->mesh;

    auto readScope = mesh->GetReadScope();

    if (!readScope)
    {
        return false;
    }

    const VertexArrayView vertexView = mesh->GetVertexData(selection.lodIndex);
    const size_t vertexSizeInFloats = vertexView.layoutDesc.VertexSize() / sizeof(float);

    for (uint32 vertexIndex : selection.vertexIndices)
    {
        PushWorldPosition(ReadMeshVertexPosition(vertexView, vertexSizeInFloats, vertexIndex)
            + (localDelta ? *localDelta : Vec3f::Zero()));
    }

    return true;
}

void EditorSubsystem::DebugDrawMeshEditSelection(DebugDrawCommandList& debugDrawCommandList)
{
    Array<Vec3f, EditorAllocator> worldPositions;

    // Hover: faint, cool-toned, and only when it isn't the already-selected face - otherwise the
    // hover tint sits on top of the selection and washes out the distinction between the two.
    if (m_meshEditState.hoveredFace
        && !IsMeshEditDragActive()
        && (!m_meshEditState.selectedFace
            || m_meshEditState.hoveredFace->node != m_meshEditState.selectedFace->node
            || m_meshEditState.hoveredFace->vertexIndices != m_meshEditState.selectedFace->vertexIndices))
    {
        if (Handle<Node> hoveredNode = m_meshEditState.hoveredFace->node.Lock(); hoveredNode.IsValid())
        {
            if (BuildMeshEditFaceWorldPositions(hoveredNode, *m_meshEditState.hoveredFace, nullptr, nullptr, worldPositions))
            {
                DrawMeshEditFace(
                    debugDrawCommandList,
                    worldPositions.ToSpan(),
                    Color(0.45f, 0.75f, 1.0f, 0.18f),
                    Color(0.6f, 0.85f, 1.0f, 0.9f));
            }
        }
    }

    if (!m_meshEditState.selectedFace)
    {
        return;
    }

    Handle<Node> node = m_meshEditState.selectedFace->node.Lock();

    if (!node.IsValid())
    {
        return;
    }

    if (m_meshEditState.dragData)
    {
        const Color axisColor = MeshEditAxisColor(m_meshEditState.dragData->lockedAxis);

        // Ghost of where the face started, so the drag distance is visible rather than something
        // you have to infer from memory of where the face used to be.
        if (BuildMeshEditFaceWorldPositions(node, *m_meshEditState.selectedFace, nullptr, &m_meshEditState.dragData->vertexOriginalPositions, worldPositions))
        {
            DrawMeshEditFace(
                debugDrawCommandList,
                worldPositions.ToSpan(),
                Color(1.0f, 1.0f, 1.0f, 0.06f),
                Color(1.0f, 1.0f, 1.0f, 0.35f));
        }

        if (BuildMeshEditFaceWorldPositions(node, *m_meshEditState.selectedFace, &m_meshEditState.dragData->currentLocalDelta, &m_meshEditState.dragData->vertexOriginalPositions, worldPositions))
        {
            DrawMeshEditFace(
                debugDrawCommandList,
                worldPositions.ToSpan(),
                Color(axisColor.GetRed(), axisColor.GetGreen(), axisColor.GetBlue(), 0.35f),
                axisColor);
        }

        return;
    }

    if (BuildMeshEditFaceWorldPositions(node, *m_meshEditState.selectedFace, nullptr, nullptr, worldPositions))
    {
        DrawMeshEditFace(
            debugDrawCommandList,
            worldPositions.ToSpan(),
            Color(1.0f, 0.7f, 0.1f, 0.3f),
            Color(1.0f, 0.8f, 0.15f, 1.0f));
    }
}

static Vec3f ComputeMeshEditDragPlaneNormal(const Handle<Camera>& camera, const Vec3f& axisDirection)
{
    if (axisDirection.LengthSquared() < MathUtil::epsilonF)
    {
        return -camera->GetDirection();
    }

    const Vec3f crossUp = axisDirection.Cross(camera->GetUpVector());
    const Vec3f crossSide = axisDirection.Cross(camera->GetSideVector());

    return (crossUp.LengthSquared() > crossSide.LengthSquared() ? crossUp : crossSide).Normalized();
}

void EditorSubsystem::StartMeshEditDrag(const Handle<Camera>& camera, const MouseEvent& mouseEvent)
{
    if (!m_meshEditState.selectedFace)
    {
        return;
    }

    Handle<Node> node = m_meshEditState.selectedFace->node.Lock();

    if (!node.IsValid())
    {
        return;
    }

    Entity* entity = DynamicCast<Entity>(node.Get());
    MeshComponent* meshComponent = entity ? entity->TryGetComponent<MeshComponent>() : nullptr;

    if (!meshComponent || !meshComponent->mesh.IsValid())
    {
        return;
    }

    Mesh* mesh = meshComponent->mesh;
    const uint8 lodIndex = m_meshEditState.selectedFace->lodIndex;

    const Array<uint32, EditorAllocator> affectedVertexIndices = FindWeldedVertexIndices(mesh, lodIndex, m_meshEditState.selectedFace->vertexIndices);

    MeshEditDragData dragData;
    dragData.affectedVertexIndices = affectedVertexIndices;
    dragData.vertexOriginalPositions.Reserve(affectedVertexIndices.Size());

    {
        auto readScope = mesh->GetReadScope();

        const VertexArrayView vertexView = mesh->GetVertexData(lodIndex);
        const size_t vertexSizeInFloats = vertexView.layoutDesc.VertexSize() / sizeof(float);

        for (uint32 vertexIndex : affectedVertexIndices)
        {
            dragData.vertexOriginalPositions.PushBack(ReadMeshVertexPosition(vertexView, vertexSizeInFloats, vertexIndex));
        }
    }

    Vec3f centroidLocalPosition = Vec3f::Zero();

    for (uint32 i = 0; i < m_meshEditState.selectedFace->vertexIndices.Size(); i++)
    {
        centroidLocalPosition += dragData.vertexOriginalPositions[i];
    }

    centroidLocalPosition /= float(m_meshEditState.selectedFace->vertexIndices.Size());

    const Mat4f& worldMatrix = node->GetWorldMatrix();
    Vec4f transformedCentroid = worldMatrix.TransformVector(Vec4f(centroidLocalPosition, 1.0f));
    transformedCentroid /= transformedCentroid.w;

    dragData.faceCentroidWorldOrigin = transformedCentroid.GetXYZ();
    dragData.currentLocalDelta = Vec3f::Zero();

    if (m_meshEditState.alignToNormal)
    {
        const Vec3f faceNormalLocal = (dragData.vertexOriginalPositions[1] - dragData.vertexOriginalPositions[0])
                                           .Cross(dragData.vertexOriginalPositions[2] - dragData.vertexOriginalPositions[0])
                                           .Normalized();

        const Mat4f normalMatrix = worldMatrix.Transpose().Inverse();
        dragData.defaultAxisDirection = normalMatrix.TransformVector(Vec4f(faceNormalLocal, 0.0f)).GetXYZ().Normalized();
    }
    else
    {
        dragData.defaultAxisDirection = Vec3f::Zero();
    }

    dragData.axisDirection = dragData.defaultAxisDirection;
    dragData.planeNormal = ComputeMeshEditDragPlaneNormal(camera, dragData.axisDirection);

    const Ray ray = camera->GetPickRay(mouseEvent.relativePos);

    if (Optional<RayHit> planeHit = ray.TestPlane(dragData.faceCentroidWorldOrigin, dragData.planeNormal))
    {
        dragData.hitpointOrigin = planeHit->hitpoint;
    }
    else
    {
        dragData.hitpointOrigin = dragData.faceCentroidWorldOrigin;
    }

    m_meshEditState.dragData = dragData;

    OnMeshEditStateChanged();
}

void EditorSubsystem::UpdateMeshEditDrag(const Handle<Camera>& camera, const MouseEvent& mouseEvent)
{
    if (!m_meshEditState.dragData || !m_meshEditState.selectedFace)
    {
        return;
    }

    Handle<Node> node = m_meshEditState.selectedFace->node.Lock();

    if (!node.IsValid())
    {
        return;
    }

    const Ray ray = camera->GetPickRay(mouseEvent.relativePos);

    Optional<RayHit> planeHit = ray.TestPlane(m_meshEditState.dragData->faceCentroidWorldOrigin, m_meshEditState.dragData->planeNormal);

    if (!planeHit)
    {
        return;
    }

    Vec3f worldDelta;

    if (m_meshEditState.dragData->axisDirection.LengthSquared() < MathUtil::epsilonF)
    {
        worldDelta = planeHit->hitpoint - m_meshEditState.dragData->hitpointOrigin;

        if (m_snapToGridEnabled)
        {
            worldDelta = MathUtil::Round(worldDelta);
        }
    }
    else
    {
        float t = (planeHit->hitpoint - m_meshEditState.dragData->hitpointOrigin).Dot(m_meshEditState.dragData->axisDirection);

        if (m_snapToGridEnabled)
        {
            t = MathUtil::Round(t);
        }

        worldDelta = m_meshEditState.dragData->axisDirection * t;
    }

    const Mat4f inverseWorldMatrix = node->GetWorldMatrix().Inverse();

    m_meshEditState.dragData->currentLocalDelta = inverseWorldMatrix.TransformVector(Vec4f(worldDelta, 0.0f)).GetXYZ();
}

void EditorSubsystem::EndMeshEditDrag(bool saveEdits)
{
    Handle<EditorProject> project = GetCurrentProject();
    if (!project.IsValid())
    {
        return;
    }

    if (!m_meshEditState.dragData)
    {
        return;
    }

    HYP_DEFER({ OnMeshEditStateChanged(); });

    if (!m_meshEditState.selectedFace)
    {
        m_meshEditState.dragData.Unset();
        return;
    }

    Handle<Node> node = m_meshEditState.selectedFace->node.Lock();

    if (!node.IsValid())
    {
        m_meshEditState.dragData.Unset();
        return;
    }

    Entity* entity = DynamicCast<Entity>(node.Get());
    MeshComponent* meshComponent = entity ? entity->TryGetComponent<MeshComponent>() : nullptr;

    if (!meshComponent || !meshComponent->mesh.IsValid())
    {
        m_meshEditState.dragData.Unset();
        return;
    }

    const uint8 lodIndex = m_meshEditState.selectedFace->lodIndex;

    if (m_meshEditState.dragData->currentLocalDelta == Vec3f::Zero())
    {
        m_meshEditState.dragData.Unset();
        return;
    }

    if (saveEdits && m_meshEditState.actionStack.IsValid())
    {
        CaptureMeshEditBaseline();

        Array<Vec3f, EditorAllocator> updatedLocalPositions;
        updatedLocalPositions.Reserve(m_meshEditState.dragData->vertexOriginalPositions.Size());

        for (const Vec3f& originalPosition : m_meshEditState.dragData->vertexOriginalPositions)
        {
            updatedLocalPositions.PushBack(originalPosition + m_meshEditState.dragData->currentLocalDelta);
        }

        Array<uint32, EditorAllocator> vertexIndices = m_meshEditState.dragData->affectedVertexIndices;
        Array<Vec3f, EditorAllocator> originalPositions = m_meshEditState.dragData->vertexOriginalPositions;

        m_meshEditState.actionStack->PushAction(MakeHandle<FunctionalEditorAction>(
            "Move Face",
            [nodeWeak = node.ToWeak(), lodIndex, vertexIndices, originalPositions, updatedLocalPositions]() -> EditorActionFunctions
            {
                return {
                    [nodeWeak, lodIndex, vertexIndices, updatedLocalPositions](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                    {
                        if (Handle<Node> node = nodeWeak.Lock(); node.IsValid())
                        {
                            ApplyMeshEditVertexPositions(node, lodIndex, vertexIndices, updatedLocalPositions, /* recomputeDerivedData */ true);
                        }
                    },
                    [nodeWeak, lodIndex, vertexIndices, originalPositions](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                    {
                        if (Handle<Node> node = nodeWeak.Lock(); node.IsValid())
                        {
                            ApplyMeshEditVertexPositions(node, lodIndex, vertexIndices, originalPositions, /* recomputeDerivedData */ true);
                        }
                    }
                };
            }));
    }

    m_meshEditState.dragData.Unset();
}

void EditorSubsystem::SetMeshEditDragLockedAxis(const Handle<Camera>& camera, const KeyboardEvent& keyboardEvent, int axis)
{
    if (!m_meshEditState.dragData)
    {
        return;
    }

    Vec3f worldAxisDirection = Vec3f::Zero();
    worldAxisDirection[axis] = 1.0f;

    // Pressing the same axis again releases the constraint, so the key acts as a toggle.
    const bool alreadyLockedToThisAxis = m_meshEditState.dragData->lockedAxis == axis;

    m_meshEditState.dragData->lockedAxis = alreadyLockedToThisAxis ? -1 : axis;
    m_meshEditState.dragData->axisDirection = alreadyLockedToThisAxis ? m_meshEditState.dragData->defaultAxisDirection : worldAxisDirection;

    m_meshEditState.dragData->planeNormal = ComputeMeshEditDragPlaneNormal(camera, m_meshEditState.dragData->axisDirection);

    AssertDebug(keyboardEvent.inputManager != nullptr);
    
    // Needs re-anchoring
    const Ray ray = camera->GetPickRay(keyboardEvent.inputManager->GetVirtualMousePositionNormalized());

    if (Optional<RayHit> planeHit = ray.TestPlane(m_meshEditState.dragData->faceCentroidWorldOrigin, m_meshEditState.dragData->planeNormal))
    {
        m_meshEditState.dragData->hitpointOrigin = planeHit->hitpoint;
    }

    OnMeshEditStateChanged();
}

#pragma endregion MeshEditMode

#pragma region EditorSubsystem Gizmos

void EditorSubsystem::InitializeGizmos()
{
    AssertOnThread(g_simThread);

    for (const Handle<EditorGizmoBase>& gizmo : m_gizmos)
    {
        gizmo->SetEditorSubsystem(this);
        gizmo->SetCurrentProject(m_currentProject);

        InitObject(gizmo);
    }
}

void EditorSubsystem::ShutdownGizmos()
{
    AssertOnThread(g_simThread);

    if (m_selectedManipulationMode != EditorManipulationMode::None)
    {
        m_selectedManipulationMode = EditorManipulationMode::None;

        OnSelectedGizmoChanged(
            m_gizmos.At(EditorManipulationMode::None),
            m_gizmos.At(m_selectedManipulationMode));
    }

    for (auto& it : m_gizmos)
    {
        it->Shutdown();
    }
}

EditorManipulationMode EditorSubsystem::GetSelectedManipulationMode() const
{
    //AssertOnThread(g_simThread);

    return m_selectedManipulationMode;
}

void EditorSubsystem::SetSelectedManipulationMode(EditorManipulationMode mode)
{
    AssertOnThread(g_simThread);

    if (!m_meshEditState.isChanging)
    {
        ExitMeshEditMode(/* saveEdits */ true);
    }

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

    OnSelectedGizmoChanged(newGizmo, prevGizmo);
}

EditorGizmoBase* EditorSubsystem::GetSelectedGizmo() const
{
    AssertOnThread(g_simThread);

    return m_gizmos.At(m_selectedManipulationMode);
}

EditorGizmoBase* EditorSubsystem::GetGizmo(EditorManipulationMode mode) const
{
    AssertOnThread(g_simThread);

    if (!m_gizmos.Contains(mode))
    {
        return nullptr;
    }

    return m_gizmos.At(mode);
}

const EditorSubsystem::EditorGizmoSet& EditorSubsystem::GetGizmos() const
{
    AssertOnThread(g_simThread);

    return m_gizmos;
}

void EditorSubsystem::UpdateGizmoProximityVisibility()
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

    if (!gizmoNode.IsValid() || !m_editorScene.IsValid())
    {
        return;
    }

    EditorViewport* activeViewport = GetActiveViewport();

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
        m_editorScene->GetRoot()->AddChild(gizmoNode);
    }
}

#pragma endregion EditorSubsystem Gizmos

#pragma region EditorSubsystem

#ifdef HYP_EDITOR

EditorSubsystem::EditorSubsystem()
    : m_selectedManipulationMode(EditorManipulationMode::None),
      m_playNetMode(EditorPlayNetMode::Standalone),
      m_playNetHost("127.0.0.1"),
      m_playNetPort(NetGlobals::GetGameServerPort()),
      m_playNetAutoLaunchServer(true),
      m_playNetCachePort(8081),
      m_activeNetMode(EditorPlayNetMode::Standalone),
      m_activeAutoLaunchServer(false),
      m_playNetState(EditorPlayNetState::None),
      m_snapToGridEnabled(false),
      m_swatchOverrideMode(false),
      m_gizmosHiddenByProximity(false),
      m_editorCameraEnabled(false),
      m_shouldCancelNextClick(false)
{
    m_gizmos.Insert(MakeHandle<NullEditorGizmo>());
    m_gizmos.Insert(MakeHandle<TranslateEditorGizmo>());
    m_gizmos.Insert(MakeHandle<RotateEditorGizmo>());
    m_gizmos.Insert(MakeHandle<ScaleEditorGizmo>());
    m_gizmos.Insert(MakeHandle<VolumeEditorGizmo>());

    m_editorDelegates = new EditorDelegates();

    // Create eagerly so the managed side can always fetch it, regardless of the calling thread.
    GetTerrainState();

    m_bakeStatusUpdateTimer = ClockTimer { 0.5f };

    LoadPlayNetSettings();

    OnSelectedGizmoChanged
        .Bind(this, [this](EditorGizmoBase* newGizmo, EditorGizmoBase* prevGizmo)
              {
                  SetHoveredGizmo(MouseEvent {}, nullptr, Handle<Node>::Null());

                  if (prevGizmo && prevGizmo->GetManipulationMode() != EditorManipulationMode::None)
                  {
                      if (prevGizmo->GetNode().IsValid())
                      {
                          prevGizmo->GetNode()->Remove();
                      }

                      prevGizmo->SetFocusedNode(Handle<Node>::Null());
                  }

                  if (newGizmo && newGizmo->GetManipulationMode() != EditorManipulationMode::None)
                  {
                      newGizmo->SetFocusedNode(m_focusedNode.Lock());

                      if (!newGizmo->GetNode().IsValid())
                      {
                          HYP_LOG(Editor, Warning, "Gizmo has no valid node; cannot attach to scene");

                          return;
                      }

                      m_editorScene->GetRoot()->AddChild(newGizmo->GetNode());
                  }
              })
        .Detach();
}

EditorSubsystem::~EditorSubsystem()
{
    if (m_currentProject)
    {
        g_editorState->SetCurrentProject(nullptr, /* isSimulationStateChange */ false);

        m_currentProject->SetEditorSubsystem(nullptr);
        m_currentProject->Close();

        m_currentProject.Reset();
    }

    delete m_editorDelegates;
}

const Handle<World>& EditorSubsystem::GetProjectWorld() const
{
    if (m_currentProject.IsValid())
    {
        return m_currentProject->GetWorld();
    }

    return Handle<World>::Null();
}

void EditorSubsystem::OnAddedToWorld()
{
    World* world = Subsystem::GetWorld();

    if (!world->GetSubsystem<UISubsystem>())
    {
        HYP_FAIL("EditorSubsystem requires UISubsystem to be initialized");
    }

    world->AddSystemT<EditorSpriteSystem>();

    m_editorScene = MakeHandle<Scene>(NAME("EditorScene"), SceneFlags::FOREGROUND | SceneFlags::EDITOR);
    m_editorScene->SetIsTransient(true);
    world->AddScene(m_editorScene);

    InitViewport();

    if (Handle<AssetCollector> baseAssetCollector = g_assetManager->GetBaseAssetCollector(); baseAssetCollector.IsValid())
    {
        baseAssetCollector->StartWatching();
    }

    g_assetManager->OnAssetCollectorAdded
        .Bind([](const Handle<AssetCollector>& assetCollector)
              {
                  assetCollector->StartWatching();
              })
        .Detach();

    g_assetManager->OnAssetCollectorRemoved
        .Bind([](const Handle<AssetCollector>& assetCollector)
              {
                  assetCollector->StopWatching();
              })
        .Detach();

    NewProject();
}

void EditorSubsystem::OnRemovedFromWorld()
{
    for (const Handle<EditorViewport>& vp : m_editorViewports)
    {
        vp->OnSceneRemoved(m_editorScene);
    }
    
    World* world = Subsystem::GetWorld();

    if (EditorSpriteSystem* spriteSystem = world->GetSystem<EditorSpriteSystem>())
    {
        world->RemoveSystem(spriteSystem);
    }

    world->RemoveScene(m_editorScene);

    if (m_currentProject)
    {
        g_editorState->SetCurrentProject(nullptr, /* isSimulationStateChange */ false);
        
        ShutdownProjectWorld(m_currentProject);
        OnProjectClosing(m_currentProject);

        m_currentProject->Close();
        m_currentProject.Reset();
    }

    m_editorViewports.Clear();
}

bool EditorSubsystem::IsPhysicsDebugDrawEnabled() const
{
    return s_cvDebugDrawPhysics.Get();
}

void EditorSubsystem::SetPhysicsDebugDrawEnabled(bool enabled)
{
    s_cvDebugDrawPhysics.Set(enabled);
}

// Deep-copy a PhysicsShape asset (there is no reflection-based clone, so switch on the concrete type).
static Handle<PhysicsShape> ClonePhysicsShape(const Handle<PhysicsShape>& source)
{
    if (!source.IsValid())
    {
        return nullptr;
    }

    const Name name = source->GetName();

    switch (source->GetType())
    {
    case PhysicsShapeType::Box:
        return MakeHandle<BoxPhysicsShape>(name, static_cast<BoxPhysicsShape*>(source.Get())->GetAABB());
    case PhysicsShapeType::Sphere:
        return MakeHandle<SpherePhysicsShape>(name, static_cast<SpherePhysicsShape*>(source.Get())->GetSphere());
    case PhysicsShapeType::Capsule:
    {
        const CapsulePhysicsShape* capsule = static_cast<CapsulePhysicsShape*>(source.Get());
        return MakeHandle<CapsulePhysicsShape>(name, capsule->GetRadius(), capsule->GetHeight());
    }
    case PhysicsShapeType::Plane:
        return MakeHandle<PlanePhysicsShape>(name, static_cast<PlanePhysicsShape*>(source.Get())->GetPlane());
    case PhysicsShapeType::ConvexHull:
    {
        const ConvexHullPhysicsShape* hull = static_cast<ConvexHullPhysicsShape*>(source.Get());
        VertexArrayView view {};
        view.floatData = hull->GetVertexData();
        view.vertexCount = hull->NumVertices();
        view.layoutDesc = StaticVertexInputLayout<VT_Position>;
        return MakeHandle<ConvexHullPhysicsShape>(name, view);
    }
    default:
        return nullptr;
    }
}

bool EditorSubsystem::CanFitPhysicsShapeToMesh() const
{
    if (!m_currentProject.IsValid() || IsSimulating())
    {
        return false;
    }

    Handle<Node> focusedNode = m_focusedNode.Lock();

    if (!focusedNode.IsValid())
    {
        return false;
    }

    Entity* entity = DynamicCast<Entity>(focusedNode.Get());

    if (entity == nullptr)
    {
        return false;
    }

    MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>();
    RigidBodyComponent* rigidBodyComponent = entity->TryGetComponent<RigidBodyComponent>();

    return meshComponent != nullptr
        && meshComponent->mesh.IsValid()
        && rigidBodyComponent != nullptr
        && DynamicCast<BoxPhysicsShape>(rigidBodyComponent->shape).IsValid();
}

bool EditorSubsystem::IsPhysicsShapeShared(Entity* entity, const Handle<PhysicsShape>& shape) const
{
    if (!shape.IsValid() || !m_currentProject.IsValid())
    {
        return false;
    }

    for (Scene* scene : GetCurrentProject()->GetWorld()->GetScenes())
    {
        for (auto [otherEntity, otherRigidBodyComponent, otherTransformComponent] : scene->GetEntityManager()->GetEntitySet<RigidBodyComponent, TransformComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            (void)otherTransformComponent;

            if (otherEntity != entity && otherRigidBodyComponent.shape == shape)
            {
                return true;
            }
        }
    }

    return false;
}

Handle<PhysicsShape> EditorSubsystem::EnsureUniquePhysicsShape(Entity* entity)
{
    RigidBodyComponent* rigidBodyComponent = entity->TryGetComponent<RigidBodyComponent>();

    if (rigidBodyComponent == nullptr)
    {
        return nullptr;
    }

    Handle<PhysicsShape> shape = rigidBodyComponent->shape;

    if (!shape.IsValid() || !IsPhysicsShapeShared(entity, shape))
    {
        return shape;
    }

    // The shape is shared with at least one other entity; clone it so this entity gets its own copy
    // that we can mutate without affecting the others.
    Handle<PhysicsShape> clone = ClonePhysicsShape(shape);
    clone->SetName(NAME_FMT("{}_{}_PhysicsShape", entity->GetName(), entity->Id().Value()));
    GetCurrentAssetRegistry()->PutAsset(clone);

    rigidBodyComponent->shape = clone;
    entity->AddTag<EntityTag::UpdatePhysicsShape>();

    return clone;
}

void EditorSubsystem::FitPhysicsShapeToMesh()
{
    if (IsSimulating())
    {
        return;
    }

    Handle<EditorProject> project = GetCurrentProject();
    if (!project.IsValid())
    {
        return;
    }

    Handle<Node> focusedNode = m_focusedNode.Lock();

    if (!focusedNode.IsValid())
    {
        return;
    }

    Entity* entity = DynamicCast<Entity>(focusedNode.Get());

    if (entity == nullptr)
    {
        return;
    }

    MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>();
    RigidBodyComponent* rigidBodyComponent = entity->TryGetComponent<RigidBodyComponent>();

    if (meshComponent == nullptr || !meshComponent->mesh.IsValid() || rigidBodyComponent == nullptr)
    {
        return;
    }

    if (!rigidBodyComponent->shape.IsValid() || rigidBodyComponent->shape->GetType() != PhysicsShapeType::Box)
    {
        return;
    }

    // Clone the shape first if any other entity references it, so we never mutate a shared asset.
    Handle<PhysicsShape> uniqueShape = EnsureUniquePhysicsShape(entity);
    Handle<BoxPhysicsShape> boxShape = DynamicCast<BoxPhysicsShape>(uniqueShape);

    if (!boxShape.IsValid())
    {
        return;
    }

    // The box shape lives in the entity's local space, so matching the mesh's local AABB aligns it
    // with the rendered geometry regardless of the entity's world transform.
    const BoundingBox meshAabb = meshComponent->mesh->GetAABB();
    const BoundingBox oldAabb = boxShape->GetAABB();

    project->GetActionStack()->PushAction(MakeHandle<FunctionalEditorAction>(
        "Fit Physics Shape to Mesh",
        [boxShape, entity = MakeStrongRef(entity), meshAabb, oldAabb]() -> EditorActionFunctions
        {
            return {
                [boxShape, entity, meshAabb](EditorSubsystem*, EditorProject*)
                {
                    if (boxShape.IsValid())
                    {
                        boxShape->SetAABB(meshAabb);

                        boxShape->Invalidate();
                    }

                    if (entity.IsValid())
                    {
                        entity->AddTag<EntityTag::UpdatePhysicsShape>();
                    }
                },
                [boxShape, entity, oldAabb](EditorSubsystem*, EditorProject*)
                {
                    if (boxShape.IsValid())
                    {
                        boxShape->SetAABB(oldAabb);
                        boxShape->Invalidate();
                    }

                    if (entity.IsValid())
                    {
                        entity->AddTag<EntityTag::UpdatePhysicsShape>();
                    }
                }
            };
        }));
}

// Wireframe attributes for physics shape visualization: depth-tested against scene geometry so shapes
// sit correctly in the world, but not depth-written so nearby shapes don't occlude one another.
static RenderableAttributeSet PhysicsWireframeAttributes()
{
    RenderableAttributeSet attributes;

    MeshAttributes& meshAttributes = attributes.GetMeshAttributes();
    meshAttributes.inputLayout = StaticVertexInputLayout<VT_Simple>;
    meshAttributes.topology = Topology::Triangles;

    MaterialAttributes& materialAttributes = attributes.GetMaterialAttributes();
    materialAttributes.bucket = RenderBucket::Debug;
    materialAttributes.fillMode = FillMode::Line;
    materialAttributes.blendFunction = BlendFunction::None();
    materialAttributes.flags = MAF_DEPTH_TEST;

    return attributes;
}

void EditorSubsystem::DebugDrawPhysicsShapes(DebugDrawCommandList& debugDrawCommandList)
{
    if (!s_cvDebugDrawPhysics.Get())
    {
        return;
    }

    static const RenderableAttributeSet wireframeAttributes = PhysicsWireframeAttributes();

    if (!m_currentProject.IsValid())
    {
        return;
    }

    static constexpr auto BoxPhysicsShapeTypeId = CONSTEXPR_TYPE_ID(BoxPhysicsShape);
    static constexpr auto SpherePhysicsShapeTypeId = CONSTEXPR_TYPE_ID(SpherePhysicsShape);
    static constexpr auto PlanePhysicsShapeTypeId = CONSTEXPR_TYPE_ID(PlanePhysicsShape);
    static constexpr auto CapsulePhysicsShapeTypeId = CONSTEXPR_TYPE_ID(CapsulePhysicsShape);
    static constexpr auto ConvexHullPhysicsShapeTypeId = CONSTEXPR_TYPE_ID(ConvexHullPhysicsShape);

    static constexpr float PlaneDebugHalfExtent = 5.0f;

    for (Scene* scene : GetCurrentProject()->GetWorld()->GetScenes())
    {
        for (auto [entity, rigidBodyComponent, transformComponent] : scene->GetEntityManager()->GetEntitySet<RigidBodyComponent, TransformComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            (void)transformComponent;

            PhysicsShape* shape = rigidBodyComponent.shape.Get();

            if (!shape)
            {
                continue;
            }

            const bool selected = IsNodeSelected(MakeStrongRef(static_cast<Node*>(entity)));
            const Color color = selected ? Color::Yellow() : Color::Green();

            const Transform entityWorldTransform(entity->GetWorldTranslation(), entity->GetWorldScale(), entity->GetWorldRotation());
            const Mat4f& entityWorldMatrix = entity->GetWorldMatrix();
            const Vec3f entityWorldScale = entity->GetWorldScale();
            const float maxEntityScale = MathUtil::Max(MathUtil::Max(entityWorldScale.x, entityWorldScale.y), entityWorldScale.z);

            switch (shape->InstanceClass()->GetTypeId().Value())
            {
            case BoxPhysicsShapeTypeId:
            {
                const BoundingBox& aabb = static_cast<BoxPhysicsShape*>(shape)->GetAABB();
                const Transform boxWorldTransform = entityWorldTransform * Transform(aabb.GetCenter(), aabb.GetExtent() * 0.5f, Quat4f::Identity());
                debugDrawCommandList.box(boxWorldTransform, color, wireframeAttributes);
                break;
            }
            case SpherePhysicsShapeTypeId:
            {
                const BoundingSphere& sphere = static_cast<SpherePhysicsShape*>(shape)->GetSphere();
                const Vec3f worldCenter = entityWorldMatrix.TransformVector(Vec4f(sphere.GetCenter(), 1.0f)).GetXYZ();
                debugDrawCommandList.sphere(worldCenter, sphere.GetRadius() * maxEntityScale, color, wireframeAttributes);
                break;
            }
            case CapsulePhysicsShapeTypeId:
            {
                const float radius = static_cast<CapsulePhysicsShape*>(shape)->GetRadius();
                const float height = static_cast<CapsulePhysicsShape*>(shape)->GetHeight(); // cylindrical part (Bullet convention)
                // Capsule is Y-axis aligned in local space. Unit cylinder mesh has radius 1 and height 1.
                const Transform cylinderWorldTransform = entityWorldTransform * Transform(Vec3f::Zero(), Vec3f(radius, height, radius), Quat4f::Identity());
                debugDrawCommandList.cylinder(cylinderWorldTransform, color, wireframeAttributes);
                const float worldRadius = radius * maxEntityScale;
                const Vec3f topWorld = entityWorldMatrix.TransformVector(Vec4f(Vec3f(0.0f, height * 0.5f, 0.0f), 1.0f)).GetXYZ();
                const Vec3f bottomWorld = entityWorldMatrix.TransformVector(Vec4f(Vec3f(0.0f, -height * 0.5f, 0.0f), 1.0f)).GetXYZ();
                debugDrawCommandList.sphere(topWorld, worldRadius, color, wireframeAttributes);
                debugDrawCommandList.sphere(bottomWorld, worldRadius, color, wireframeAttributes);
                break;
            }
            case PlanePhysicsShapeTypeId:
            {
                // Planes are infinite; draw a finite wireframe quad at the entity origin oriented to the plane normal.
                const Vec4f& plane = static_cast<PlanePhysicsShape*>(shape)->GetPlane();
                Vec3f normal(plane.x, plane.y, plane.z);
                if (normal.Length() < MathUtil::epsilonF)
                {
                    normal = Vec3f::UnitY();
                }
                else
                {
                    normal.Normalize();
                }
                const Vec3f reference = MathUtil::Abs(normal.y) < 0.99f ? Vec3f::UnitY() : Vec3f::UnitX();
                const Vec3f tangent = (reference - normal * normal.Dot(reference)).Normalize();
                const Vec3f bitangent = normal.Cross(tangent).Normalize();
                const float e = PlaneDebugHalfExtent;
                const Vec3f c0 = (-tangent - bitangent) * e;
                const Vec3f c1 = ( tangent - bitangent) * e;
                const Vec3f c2 = ( tangent + bitangent) * e;
                const Vec3f c3 = (-tangent + bitangent) * e;
                const Mat4f& m = entityWorldMatrix;
                const FixedArray<Vec3f, 4> worldCorners = {
                    m.TransformVector(Vec4f(c0, 1.0f)).GetXYZ(),
                    m.TransformVector(Vec4f(c1, 1.0f)).GetXYZ(),
                    m.TransformVector(Vec4f(c2, 1.0f)).GetXYZ(),
                    m.TransformVector(Vec4f(c3, 1.0f)).GetXYZ()
                };
                debugDrawCommandList.plane(worldCorners, color, wireframeAttributes);
                break;
            }
            case ConvexHullPhysicsShapeTypeId:
            {
                // Without recomputing the hull, render the local AABB of the hull points as an oriented box.
                const ConvexHullPhysicsShape* hull = static_cast<ConvexHullPhysicsShape*>(shape);
                const size_t numVertices = hull->NumVertices();
                const float* vertexData = hull->GetVertexData();

                if (numVertices == 0 || vertexData == nullptr)
                {
                    break;
                }

                BoundingBox hullAabb(Vec3f(vertexData[0], vertexData[1], vertexData[2]), Vec3f(vertexData[0], vertexData[1], vertexData[2]));
                for (size_t i = 1; i < numVertices; i++)
                {
                    const Vec3f v(vertexData[i * 3 + 0], vertexData[i * 3 + 1], vertexData[i * 3 + 2]);
                    hullAabb = hullAabb.Union(v);
                }

                const Transform hullWorldTransform = entityWorldTransform * Transform(hullAabb.GetCenter(), hullAabb.GetExtent() * 0.5f, Quat4f::Identity());
                debugDrawCommandList.box(hullWorldTransform, color, wireframeAttributes);
                break;
            }
            default:
                break;
            }
        }
    }
}

void EditorSubsystem::Update(float delta)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    m_editorDelegates->Update();

    if (g_appContext.IsValid() && g_appContext->GetMainWindow() != nullptr)
    {
        const EnumFlags<MouseButtonState> buttonStates = g_appContext->GetMainWindow()->GetInputManager()->GetButtonStates();

        if (!(buttonStates & (MouseButtonState::LEFT | MouseButtonState::RIGHT)))
        {
            if (EditorGizmoBase* gizmo = GetSelectedGizmo(); gizmo != nullptr && gizmo->IsDragging())
            {
                EditorViewport* activeViewport = GetActiveViewport();

                gizmo->OnDragEnd(activeViewport != nullptr ? activeViewport->GetCamera() : Handle<Camera>(), MouseEvent {});
            }

            if (IsMeshEditDragActive())
            {
                EndMeshEditDrag(/* saveEdits */ true);
            }
        }
    }

    UpdateGizmoProximityVisibility();

    GetTerrainState()->Update();
    UpdateBakeStatus();
    UpdatePlayNetState();

    DebugDrawCommandList& dbg = DebugDrawer::GetInstance().CreateCommandList();

    DebugDrawMeshEditSelection(dbg);
    DebugDrawPhysicsShapes(dbg);
    GetTerrainState()->DebugDrawCursor(dbg);

    if (m_currentProject.IsValid())
    {
        const Handle<World>& world = m_currentProject->GetWorld();

        // World might be invalid if simulation is starting and the project is loading.
        if (world.IsValid())
        {
            // Debug draw probes
            for (Scene* scene : world->GetScenes())
            {
                for (auto [probe] : scene->GetEntityManager()->GetEntitySet<EntityType<EnvProbe>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
                {
                    static constexpr auto ReflectionProbeTypeId = CONSTEXPR_TYPE_ID(ReflectionProbe);
                    static constexpr auto SkyProbeTypeId = CONSTEXPR_TYPE_ID(SkyProbe);
                    static constexpr auto IrradianceProbeTypeId = CONSTEXPR_TYPE_ID(IrradianceProbe);

                    switch (probe->InstanceClass()->GetTypeId().Value())
                    {
                    case ReflectionProbeTypeId:
                        dbg.reflectionProbe(probe->GetWorldTranslation(), 1.0f, *probe);
                        break;
                    case SkyProbeTypeId:
                        // dbg.reflectionProbe(probe->GetWorldTranslation(), 1.0f, *probe);
                        break;
                    case IrradianceProbeTypeId:
                        dbg.ambientProbe(probe->GetWorldTranslation(), 1.0f, *probe);
                        break;
                    default:
                        HYP_LOG_ONCE(Editor, Warning, "Unknown probe type class: {}", probe->InstanceClass()->GetName());
                        break;
                    }
                }
            }
        }
    }

    if (!m_selectedNodes.Empty())
    {
        for (const Handle<Node>& node : m_selectedNodes)
        {
            if (node.IsValid())
            {
                const BoundingBox worldBounds = node->GetWorldBounds();

                if (worldBounds.IsFinite())
                {
                    dbg.box(worldBounds.GetCenter(), worldBounds.GetExtent() * 0.5f + Vec3f(FLT_EPSILON), Color::Cyan());
                }
            }
        }
    }

    RenderProxyList& pickRpl = g_editorState->GetPickCache().GetRenderProxyList();
    pickRpl.GetMeshes().Advance();

    if (m_currentProject.IsValid())
    {
        for (const Handle<EditorViewport>& vp : m_editorViewports)
        {
            const Handle<View>& view = vp->GetView();
            AssertDebug(view != nullptr);

            if (!view)
            {
                continue;
            }

            if (!(view->GetViewDesc().flags & ViewFlags::GBUFFER))
            {
                continue; // skip non-primary views
            }

            for (Mesh* mesh : view->GetRenderProxyList(GetRingIndex())->GetMeshes())
            {
                pickRpl.GetMeshes().Track(mesh->Id(), mesh);
            }
        }

        /// @TODO : Prioritize meshes based on distance from camera
        for (Mesh* mesh : pickRpl.GetMeshes())
        {
            g_editorState->GetPickCache().PutEntry(mesh);
        }
    }
}

void EditorSubsystem::OnSceneAttached(const Handle<Scene>& scene)
{
}

void EditorSubsystem::OnSceneDetached(Scene* scene)
{
}

bool EditorSubsystem::StartSimulation()
{
    if (!m_currentProject.IsValid())
    {
        return false;
    }

    // Save the edits to meshes before simulating.
    ExitMeshEditMode(/* saveEdits */ true);

    const GameState& gameState = m_currentProject->GetGame()->GetGameState();

    const bool isSimulatingOrPaused = gameState.mode == GameStateMode::SIMULATING
        || gameState.mode == GameStateMode::PAUSED;

    if (isSimulatingOrPaused)
    {
        // already simulating, unpause if paused, otherwise do nothing.
        if (gameState.mode == GameStateMode::PAUSED)
        {
            m_currentProject->GetGame()->StartSimulating();
        }

        return true;
    }

    const EditorPlayNetMode netMode = m_playNetMode;

    if (netMode == EditorPlayNetMode::Client && g_gameClient == nullptr)
    {
        HYP_LOG(Editor, Error, "Cannot Play As Client: no GameClient instance exists in this process");

        return false;
    }

    bool isServerStarted = false;
    bool isSimulationStarted = false;

    HYP_DEFER({
        if (isServerStarted && !isSimulationStarted)
        {
            g_gameServer->Stop();
        }
    });

    if (netMode == EditorPlayNetMode::DedicatedServer)
    {
        if (g_gameServer == nullptr)
        {
            HYP_LOG(Editor, Error, "Cannot Play As Dedicated Server: no GameServer instance exists in this process");

            return false;
        }

        // Must be listening before the snapshot world initializes, PlayerSystem binds to the server as it's added
        if (Result listenResult = g_gameServer->Start(uint16(m_playNetPort)); listenResult.HasError())
        {
            HYP_LOG(Editor, Error, "Play As Dedicated Server: could not listen on port {}: {}", m_playNetPort, listenResult.GetError().GetMessage());

            return false;
        }

        isServerStarted = true;
    }

    // Save the current project state as a snapshot to restore from when simulation ends.
    if (Result saveResult = m_currentProject->Save(); saveResult.HasError())
    {
        HYP_LOG(Editor, Error, "Failed to save project snapshot before simulation: {}", saveResult.GetError().GetMessage());

        return false;
    }

    m_preSimulationProject = m_currentProject;
    m_simulationSnapshotPath = m_preSimulationProject->GetFilePath();

    // Keep the world alive, we need it to persist otherwise we'd need to load it again and undo/redo history would be lost.
    CloseProject(/* shutdownWorld */ false);

    FilePath snapshotPath = std::move(m_simulationSnapshotPath);

    TResult<Handle<EditorProject>> loadResult = EditorProject::Load(snapshotPath);

    if (loadResult.HasError())
    {
        HYP_LOG(Editor, Error, "Failed to load project when starting simulation!! Error was: {}", loadResult.GetError().GetMessage());

        return false;
    }

    if (netMode == EditorPlayNetMode::DedicatedServer)
    {
        (*loadResult)->GetGame()->SetIsServerGame(true);
    }

    // Open project to start simulating
    OpenProject(*loadResult);

    Game* gameInstance = m_currentProject->GetGame();
    Assert(gameInstance != nullptr);

    Assert(gameInstance->GetWorld().IsValid());
    Assert(m_currentProject.IsValid() && m_currentProject->GetWorld().IsValid());
    
#if 0
    Camera* primaryCamera = nullptr;

    for (Scene* scene : m_currentProject->GetWorld()->GetScenes())
    {
        if (!scene)
        {
            continue;
        }

        if ((scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) != SceneFlags::FOREGROUND)
        {
            continue;
        }

        if (Camera* camera = scene->GetPrimaryCamera())
        {
            primaryCamera = camera;
            break;
        }
    }

    if (primaryCamera)
    {
        ViewDesc viewDesc {};
        viewDesc.flags = ViewFlags::DEFAULT | ViewFlags::GBUFFER | ViewFlags::MATCH_CAMERA_DIMENSIONS;
        viewDesc.framebufferDesc.extent = Vec2u(primaryCamera->GetDimensions());
        viewDesc.camera = primaryCamera;

        m_simulationView = MakeHandle<View>(viewDesc);
        m_simulationView->SetName(NAME("SimulationView"));
        InitObject(m_simulationView);

        for (Scene* scene : m_currentProject->GetWorld()->GetScenes())
        {
            if (!scene)
            {
                continue;
            }

            if ((scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) != SceneFlags::FOREGROUND)
            {
                continue;
            }

            m_simulationView->AddScene(scene);
        }

        m_currentProject->GetWorld()->AddView(m_simulationView);
    }
    else
    {
        HYP_LOG(Editor, Warning, "no primary camera found!");

        // Show a messagebox since this can be really annoying and frustrating if this happens
        // should make it easier to narrow down the issue at least.

        SystemMessageBox(MessageBoxType::WARNING)
            .Title("No primary camera found")
            .Text("No primary camera was found in any foreground scene. Simulation requires a primary camera in order to properly visualize the scene, without this you will just see a blank / not updating screen in your viewport. Ensure a camera exists with the PrimaryCamera EntityTag set!")
            .Show();
    }
#endif

    gameInstance->StartSimulating();

    if (UISubsystem* uiSubsystem = Subsystem::GetWorld()->GetSubsystem<UISubsystem>())
    {
        uiSubsystem->SetDebugOverlaysSuppressed(true);
    }

    // Keep simulating at full rate while another window (e.g. a connected client) has focus.
    // Pushed on the render thread since that's where the throttling is checked and global contexts are per-thread.
    GetThreadById(g_renderThread)->GetScheduler().Enqueue(
        []()
        {
            PushGlobalContext(SuppressIdleThrottlingContext {});
        },
        TaskEnqueueFlags::FIRE_AND_FORGET);

    m_activeNetMode = netMode;
    m_activeAutoLaunchServer = netMode == EditorPlayNetMode::Client && m_playNetAutoLaunchServer && IsLoopbackHost(m_playNetHost);

    isSimulationStarted = true;

    if (m_activeNetMode != EditorPlayNetMode::Standalone && !(m_currentProject->GetWorld()->GetWorldFlags() & WorldFlags::IsReplicated))
    {
        HYP_LOG(Editor, Warning, "World '{}' is not flagged IsReplicated, so nothing will be replicated in this session", m_currentProject->GetWorld()->GetName());
    }

    if (m_activeNetMode == EditorPlayNetMode::Client && m_activeAutoLaunchServer)
    {
        // The editor launches the server process now that the snapshot is saved, then calls OnPlayNetServerReady()
        HYP_LOG(Editor, Info, "Play As Client: waiting for local server on port {}", m_playNetPort);

        SetPlayNetState(EditorPlayNetState::StartingServer);
    }
    else if (m_activeNetMode == EditorPlayNetMode::Client)
    {
        // Connect only after the snapshot world has launched. Game::ConnectToServer isn't usable here:
        // before launch it swaps in a temp loading world, and after launch its connected state re-runs Launch().
        ConnectPlayNetClient();
    }
    else if (m_activeNetMode == EditorPlayNetMode::DedicatedServer)
    {
        HYP_LOG(Editor, Info, "Play As Dedicated Server: listening on port {}", m_playNetPort);

        SetPlayNetState(EditorPlayNetState::Hosting);
    }

    return true;
}

bool EditorSubsystem::StopSimulation()
{
    if (m_currentProject.IsValid())
    {
        Game* gameInstance = m_currentProject->GetGame();
        Assert(gameInstance != nullptr);

        AssertOnThread(g_simThread);
        DebugDrawer::GetInstance().DiscardPendingCommands();

        GetThreadById(g_renderThread)->GetScheduler().Enqueue(
            []()
            {
                DebugDrawer::GetInstance().ClearCommands();
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);

        gameInstance->StopSimulating();

        Assert(IsSimulating());

        OpenProject(m_preSimulationProject);

        m_preSimulationProject.Reset();

        // Only once the simulation world is gone, so its PlayerSystem / ReplicationSystem no longer use the connection
        if (m_activeNetMode == EditorPlayNetMode::Client && g_gameClient != nullptr)
        {
            g_gameClient->Disconnect();
        }
        else if (m_activeNetMode == EditorPlayNetMode::DedicatedServer && g_gameServer != nullptr)
        {
            g_gameServer->Stop();

            HYP_LOG(Editor, Info, "Play As Dedicated Server: stopped listening on port {}", m_playNetPort);
        }

        m_activeNetMode = EditorPlayNetMode::Standalone;
        m_activeAutoLaunchServer = false;
        SetPlayNetState(EditorPlayNetState::None);

        if (UISubsystem* uiSubsystem = Subsystem::GetWorld()->GetSubsystem<UISubsystem>())
        {
            uiSubsystem->SetDebugOverlaysSuppressed(false);
        }

        GetThreadById(g_renderThread)->GetScheduler().Enqueue(
            []()
            {
                if (IsGlobalContextActive<SuppressIdleThrottlingContext>())
                {
                    PopGlobalContext<SuppressIdleThrottlingContext>();
                }
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);

        return true;
    }

    return true;
}

bool EditorSubsystem::PauseSimulation()
{
    if (m_currentProject.IsValid())
    {
        m_currentProject->GetGame()->PauseSimulation();

        return true;
    }

    return false;
}

static constexpr const char* s_playNetModeConfigKey = "PlayInEditor.NetMode";
static constexpr const char* s_playNetHostConfigKey = "PlayInEditor.Host";
static constexpr const char* s_playNetPortConfigKey = "PlayInEditor.Port";
static constexpr const char* s_playNetAutoLaunchServerConfigKey = "PlayInEditor.AutoLaunchServer";
static constexpr const char* s_playNetCachePortConfigKey = "PlayInEditor.CachePort";

void EditorSubsystem::LoadPlayNetSettings()
{
    EditorConfig config;

    // A missing file just means nothing has been saved yet
    if (!config.Load())
    {
        return;
    }

    if (const ConfigValue& modeValue = config.Get(s_playNetModeConfigKey); modeValue.IsString())
    {
        const String modeString = modeValue.ToString();

        if (modeString == "Client")
        {
            m_playNetMode = EditorPlayNetMode::Client;
        }
        else if (modeString == "DedicatedServer")
        {
            m_playNetMode = EditorPlayNetMode::DedicatedServer;
        }
        else
        {
            m_playNetMode = EditorPlayNetMode::Standalone;
        }
    }

    if (const ConfigValue& hostValue = config.Get(s_playNetHostConfigKey); hostValue.IsString() && hostValue.ToString().Any())
    {
        m_playNetHost = hostValue.ToString();
    }

    if (const ConfigValue& portValue = config.Get(s_playNetPortConfigKey); portValue.IsNumber())
    {
        const uint32 port = portValue.ToUInt32();

        if (port > 0 && port <= MathUtil::MaxSafeValue<uint16>())
        {
            m_playNetPort = port;
        }
    }

    if (const ConfigValue& autoLaunchValue = config.Get(s_playNetAutoLaunchServerConfigKey); autoLaunchValue.IsBool())
    {
        m_playNetAutoLaunchServer = autoLaunchValue.ToBool();
    }

    if (const ConfigValue& cachePortValue = config.Get(s_playNetCachePortConfigKey); cachePortValue.IsNumber())
    {
        const uint32 port = cachePortValue.ToUInt32();

        if (port > 0 && port <= MathUtil::MaxSafeValue<uint16>())
        {
            m_playNetCachePort = port;
        }
    }
}

void EditorSubsystem::SavePlayNetSettings()
{
    EditorConfig config;
    config.Load();

    const char* modeString = "Standalone";

    switch (m_playNetMode)
    {
    case EditorPlayNetMode::Client:
        modeString = "Client";
        break;
    case EditorPlayNetMode::DedicatedServer:
        modeString = "DedicatedServer";
        break;
    default:
        break;
    }

    config.Set(s_playNetModeConfigKey, ConfigValue(String(modeString)));
    config.Set(s_playNetHostConfigKey, ConfigValue(m_playNetHost));
    config.Set(s_playNetPortConfigKey, ConfigValue(m_playNetPort));
    config.Set(s_playNetAutoLaunchServerConfigKey, ConfigValue(m_playNetAutoLaunchServer));
    config.Set(s_playNetCachePortConfigKey, ConfigValue(m_playNetCachePort));

    if (!config.Save())
    {
        HYP_LOG(Editor, Warning, "Failed to save Play In Editor network settings");
    }
}

void EditorSubsystem::SetPlayNetMode(EditorPlayNetMode mode)
{
    if (mode == m_playNetMode)
    {
        return;
    }

    m_playNetMode = mode;

    SavePlayNetSettings();
}

void EditorSubsystem::SetPlayNetHost(const String& host)
{
    const String trimmedHost = host.Trimmed();

    if (trimmedHost.Empty() || trimmedHost == m_playNetHost)
    {
        return;
    }

    m_playNetHost = trimmedHost;

    SavePlayNetSettings();
}

void EditorSubsystem::SetPlayNetPort(uint32 port)
{
    if (port == 0 || port > MathUtil::MaxSafeValue<uint16>())
    {
        HYP_LOG(Editor, Warning, "Ignoring invalid Play In Editor port {}", port);

        return;
    }

    if (port == m_playNetPort)
    {
        return;
    }

    m_playNetPort = port;

    SavePlayNetSettings();
}

void EditorSubsystem::SetPlayNetAutoLaunchServer(bool autoLaunchServer)
{
    if (autoLaunchServer == m_playNetAutoLaunchServer)
    {
        return;
    }

    m_playNetAutoLaunchServer = autoLaunchServer;

    SavePlayNetSettings();
}

void EditorSubsystem::SetPlayNetCachePort(uint32 port)
{
    if (port == 0 || port > MathUtil::MaxSafeValue<uint16>())
    {
        HYP_LOG(Editor, Warning, "Ignoring invalid Play In Editor cache server port {}", port);

        return;
    }

    if (port == m_playNetCachePort)
    {
        return;
    }

    m_playNetCachePort = port;

    SavePlayNetSettings();
}

String EditorSubsystem::GetPlayNetProjectDirectory() const
{
    if (!m_preSimulationProject.IsValid())
    {
        return String::empty;
    }

    return m_preSimulationProject->GetFilePath().BasePath();
}

void EditorSubsystem::OnPlayNetServerReady()
{
    if (m_activeNetMode != EditorPlayNetMode::Client || m_playNetState != EditorPlayNetState::StartingServer)
    {
        return;
    }

    ConnectPlayNetClient();
}

void EditorSubsystem::OnPlayNetServerFailed()
{
    if (m_activeNetMode != EditorPlayNetMode::Client || m_playNetState != EditorPlayNetState::StartingServer)
    {
        return;
    }

    SetPlayNetState(EditorPlayNetState::Failed);
}

void EditorSubsystem::ConnectPlayNetClient()
{
    Assert(g_gameClient != nullptr);

    HYP_LOG(Editor, Info, "Play As Client: connecting to {}:{}", m_playNetHost, m_playNetPort);

    if (Result connectResult = g_gameClient->Connect(m_playNetHost.ToAnsi(), uint16(m_playNetPort)); connectResult.HasError())
    {
        HYP_LOG(Editor, Error, "Play As Client: failed to connect to {}:{}: {}", m_playNetHost, m_playNetPort, connectResult.GetError().GetMessage());

        SetPlayNetState(EditorPlayNetState::Failed);

        return;
    }

    SetPlayNetState(EditorPlayNetState::Connecting);
}

void EditorSubsystem::UpdatePlayNetState()
{
    if (m_activeNetMode != EditorPlayNetMode::Client || g_gameClient == nullptr)
    {
        return;
    }

    switch (g_gameClient->GetConnectionState())
    {
    case NetClientConnectionState::Connected:
        if (m_playNetState != EditorPlayNetState::Connected)
        {
            HYP_LOG(Editor, Info, "Play As Client: connected to {}:{}", m_playNetHost, m_playNetPort);

            SetPlayNetState(EditorPlayNetState::Connected);
        }

        break;
    case NetClientConnectionState::Disconnected:
        if (m_playNetState == EditorPlayNetState::Connecting)
        {
            const Result lastError = g_gameClient->GetLastError();

            HYP_LOG(Editor, Error, "Play As Client: could not connect to {}:{}: {}", m_playNetHost, m_playNetPort,
                lastError.HasError() ? lastError.GetError().GetMessage() : "unknown error");

            SetPlayNetState(EditorPlayNetState::Failed);
        }
        else if (m_playNetState == EditorPlayNetState::Connected)
        {
            HYP_LOG(Editor, Warning, "Play As Client: lost connection to {}:{}", m_playNetHost, m_playNetPort);

            SetPlayNetState(EditorPlayNetState::Disconnected);
        }

        break;
    default:
        break;
    }
}

void EditorSubsystem::SetPlayNetState(EditorPlayNetState state)
{
    if (state == m_playNetState)
    {
        return;
    }

    m_playNetState = state;

    OnPlayNetStateChanged(state);
}

void EditorSubsystem::InitViewport()
{
    for (const Handle<EditorViewport>& vp : m_editorViewports)
    {
        vp->OnRemoved(this);
    }
    m_editorViewports.Clear();

    UISubsystem* uiSubsystem = Subsystem::GetWorld()->GetSubsystem<UISubsystem>();
    Assert(uiSubsystem != nullptr);

    Handle<UIPanel> backdropPanel = uiSubsystem->GetUIStage()->CreateUIObject<UIPanel>(NAME("Editor_BackdropPanel"), Vec2i::Zero(), UIObjectSize(100, UIObjectSize::PERCENT));
    Assert(backdropPanel != nullptr);

    backdropPanel->SetBackgroundColor(Color::Transparent());
    uiSubsystem->GetUIStage()->AddChildUIObject(backdropPanel);

    uiSubsystem->GetUIStage()->UpdateSize(true);

    backdropPanel->OnClick.RemoveAllFromSet(m_delegateHandlers);
    m_delegateHandlers.Add(backdropPanel->OnClick.Bind(
        backdropPanel.Get(),
        [this](const MouseEvent& event)
        {
            if (m_shouldCancelNextClick)
            {
                return UIEventHandlerResult::STOP_BUBBLING;
            }

            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
            }

            // if (m_camera->GetCameraController()->GetInputHandler()->OnClick(event))
            // {
            //     return UIEventHandlerResult::STOP_BUBBLING;
            // }

            if (GetTerrainState()->IsEnabled())
            {
                // Strokes are applied from OnMouseDown / OnMouseDrag / the per-frame update;
                // clicking just shouldn't fall through to scene picking.
                return UIEventHandlerResult::STOP_BUBBLING;
            }

            if (m_meshEditState.enabled)
            {
                const Ray ray = activeViewport->GetCamera()->GetPickRay(event.relativePos);

                MeshEditFaceSelection faceSelection;

                if (TryPickMeshEditFace(ray, faceSelection, /* ensureUniqueMesh */ true))
                {
                    SetSelectedMeshEditFace(faceSelection);
                }
                else
                {
                    SetSelectedMeshEditFace({});
                }

                return UIEventHandlerResult::STOP_BUBBLING;
            }

            if (IsHoveringGizmo())
            {
                return UIEventHandlerResult::STOP_BUBBLING;
            }

            const Ray ray = activeViewport->GetCamera()->GetPickRay(event.relativePos);

            RayTestResults results;

            bool hasHits = false;
            for (const Handle<EditorViewport>& vp : m_editorViewports)
            {
                if (vp->GetView()->TestRay(ray, results, RayTestFlags::TestBVH | RayTestFlags::EditorPick))
                {
                    hasHits = true;
                }
            }

            if (hasHits)
            {
                for (const RayHit& hit : results)
                {
                    if (hit.node != nullptr)
                    {
                        Handle<Node> nodeStrong = MakeStrongRef(hit.node);

                        bool shouldMutateSelection = false;

                        InputManager* inputManager = g_appContext->GetMainWindow()->GetInputManager();

                        // If CTRL key is down, add/remove from current selection.
                        if (inputManager->IsCtrlDown())
                        {
                            shouldMutateSelection = true;
                        }

                        if (shouldMutateSelection)
                        {
                            // If already in selection, remove, otherwise add
                            if (m_selectedNodes.Contains(nodeStrong))
                            {
                                m_selectedNodes.Erase(nodeStrong);
                                // Don't set focused node if deselecting this node.
                            }
                            else
                            {
                                m_selectedNodes.Add(nodeStrong);
                                SetFocusedNode(nodeStrong, true);
                            }
                        }
                        else
                        {
                            m_selectedNodes = { nodeStrong };
                            SetFocusedNode(nodeStrong, true);
                        }

                        OnSelectionChanged();

                        break;
                    }
                }

                return UIEventHandlerResult::STOP_BUBBLING;
            }

            return UIEventHandlerResult::OK;
        }));

    backdropPanel->OnMouseLeave.RemoveAllFromSet(m_delegateHandlers);
    m_delegateHandlers.Add(backdropPanel->OnMouseLeave.Bind(
        backdropPanel.Get(),
        [this](const MouseEvent& event)
        {
            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
            }

            if (IsHoveringGizmo())
            {
                SetHoveredGizmo(event, nullptr, Handle<Node>::Null());
            }

            CameraController* controller = activeViewport->GetCamera()->GetCameraController();

            if (controller != nullptr)
            {
                InputHandlerBase* inputHandler = controller->GetInputHandler();

                if (inputHandler != nullptr)
                {
                    inputHandler->OnMouseLeave(event);
                }
            }

            return UIEventHandlerResult::OK;
        }));

    backdropPanel->OnMouseDrag.RemoveAllFromSet(m_delegateHandlers);
    m_delegateHandlers.Add(backdropPanel->OnMouseDrag.Bind(
        backdropPanel.Get(),
        [this, uiStage = uiSubsystem->GetUIStage().Get()](const MouseEvent& event)
        {
            // prevent click being triggered on release once mouse has been dragged
            m_shouldCancelNextClick = true;

            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
            }

            if (GetTerrainState()->IsEnabled() && event.mouseButtons[MouseButtonState::LEFT])
            {
                InputManager* inputManager = g_appContext->GetMainWindow()->GetInputManager();

                GetTerrainState()->UpdateStroke(event.relativePos, /* invert */ inputManager->IsShiftDown());

                return UIEventHandlerResult::STOP_BUBBLING;
            }

            if (IsMeshEditDragActive())
            {
                UpdateMeshEditDrag(activeViewport->GetCamera(), event);

                return UIEventHandlerResult::STOP_BUBBLING;
            }

            if (IsHoveringGizmo())
            {
                // If the mouse is currently over a gizmo, don't allow camera to handle the event
                Handle<EditorGizmoBase> gizmo = m_hoveredGizmo.Lock();
                Handle<Node> node = m_hoveredGizmoNode.Lock();

                if (!gizmo || !node)
                {
                    HYP_LOG(Editor, Warning, "Failed to lock hovered gizmo or node");

                    return UIEventHandlerResult::ERR;
                }

                if (gizmo->OnMouseMove(activeViewport->GetCamera(), event, node))
                {
                    return UIEventHandlerResult::STOP_BUBBLING;
                }
            }

            CameraController* controller = activeViewport->GetCamera()->GetCameraController();
            
            if (controller != nullptr)
            {
                InputHandlerBase* inputHandler = controller->GetInputHandler();

                if (inputHandler != nullptr)
                {
                    if (inputHandler->OnMouseDrag(event))
                    {
                        return UIEventHandlerResult::STOP_BUBBLING;
                    }
                }
            }

            return UIEventHandlerResult::OK;
        }));

    backdropPanel->OnMouseMove.RemoveAllFromSet(m_delegateHandlers);
    m_delegateHandlers.Add(backdropPanel->OnMouseMove.Bind(
        backdropPanel.Get(),
        [this, uiStage = uiSubsystem->GetUIStage().Get()](const MouseEvent& event)
        {
            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
            }

            if (GetTerrainState()->IsEnabled())
            {
                GetTerrainState()->UpdateHover(event.relativePos);

                if (GetTerrainState()->IsStroking() && event.mouseButtons[MouseButtonState::LEFT])
                {
                    InputManager* inputManager = g_appContext->GetMainWindow()->GetInputManager();

                    GetTerrainState()->UpdateStroke(event.relativePos, /* invert */ inputManager->IsShiftDown());

                    return UIEventHandlerResult::STOP_BUBBLING;
                }

                if (!event.mouseButtons[MouseButtonState::LEFT])
                {
                    return UIEventHandlerResult::STOP_BUBBLING;
                }
            }

            if (m_meshEditState.enabled && !event.mouseButtons[MouseButtonState::LEFT] && !IsMeshEditDragActive())
            {
                const Ray ray = activeViewport->GetCamera()->GetPickRay(event.relativePos);

                UpdateHoveredMeshEditFace(ray);

                return UIEventHandlerResult::STOP_BUBBLING;
            }

            // Hover over a gizmo when mouse is not down
            if (!event.mouseButtons[MouseButtonState::LEFT]
                && GetSelectedManipulationMode() != EditorManipulationMode::None
                && !AreGizmosHiddenByProximity())
            {
                // Ray test the gizmo

                const Ray ray = activeViewport->GetCamera()->GetPickRay(event.relativePos);

                RayTestResults results;

                EditorGizmoBase* gizmo = GetSelectedGizmo();

                bool testRayReturnedHit = gizmo && gizmo->GetNode()->TestRay(ray, results, RayTestFlags::TestBVH | RayTestFlags::EditorPick);

                if (testRayReturnedHit)
                {
                    for (const RayHit& rayHit : results)
                    {
                        if (!rayHit.node)
                            continue;

                        if (rayHit.node == m_hoveredGizmoNode.GetUnsafe())
                        {
                            return UIEventHandlerResult::STOP_BUBBLING;
                        }

                        Handle<Node> nodeHandle = MakeStrongRef(rayHit.node);

                        if (gizmo->OnMouseHover(activeViewport->GetCamera(), event, nodeHandle))
                        {
                            SetHoveredGizmo(event, gizmo, nodeHandle);

                            return UIEventHandlerResult::STOP_BUBBLING;
                        }
                    }
                }

                SetHoveredGizmo(event, nullptr, Handle<Node>::Null());
            }

            CameraController* controller = activeViewport->GetCamera()->GetCameraController();
            
            if (controller != nullptr)
            {
                InputHandlerBase* inputHandler = controller->GetInputHandler();

                if (inputHandler != nullptr)
                {
                    if (inputHandler->OnMouseMove(event))
                    {
                        return UIEventHandlerResult::STOP_BUBBLING;
                    }
                }
            }

            return UIEventHandlerResult::OK;
        }));

    backdropPanel->OnMouseDown.RemoveAllFromSet(m_delegateHandlers);
    m_delegateHandlers.Add(backdropPanel->OnMouseDown.Bind(
        backdropPanel.Get(),
        [this, uiStageWeak = uiSubsystem->GetUIStage().ToWeak()](const MouseEvent& event)
        {
            m_shouldCancelNextClick = false;

            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
            }

            if (GetTerrainState()->IsEnabled())
            {
                InputManager* inputManager = g_appContext->GetMainWindow()->GetInputManager();

                GetTerrainState()->BeginStroke(event.relativePos, /* invert */ inputManager->IsShiftDown());

                return UIEventHandlerResult::STOP_BUBBLING;
            }

            if (m_meshEditState.enabled && m_meshEditState.selectedFace)
            {
                StartMeshEditDrag(activeViewport->GetCamera(), event);

                return UIEventHandlerResult::STOP_BUBBLING;
            }

            if (IsHoveringGizmo())
            {
                Handle<EditorGizmoBase> gizmo = m_hoveredGizmo.Lock();
                Handle<Node> node = m_hoveredGizmoNode.Lock();

                if (gizmo && node && !gizmo->IsDragging())
                {
                    const Ray ray = activeViewport->GetCamera()->GetPickRay(event.relativePos);

                    RayTestResults results;

                    if (node->TestRay(ray, results, RayTestFlags::TestBVH | RayTestFlags::EditorPick))
                    {
                        for (const RayHit& rayHit : results)
                        {
                            gizmo->OnDragStart(activeViewport->GetCamera(), event, node, rayHit.hitpoint);

                            return UIEventHandlerResult::STOP_BUBBLING;
                        }
                    }
                }
            }

            CameraController* controller = activeViewport->GetCamera()->GetCameraController();
            
            if (controller != nullptr)
            {
                InputHandlerBase* inputHandler = controller->GetInputHandler();

                if (inputHandler != nullptr)
                {
                    if (inputHandler->OnMouseDown(event))
                    {
                        return UIEventHandlerResult::STOP_BUBBLING;
                    }
                }
            }

            return UIEventHandlerResult::OK;
        }));

    backdropPanel->OnMouseUp.RemoveAllFromSet(m_delegateHandlers);
    m_delegateHandlers.Add(backdropPanel->OnMouseUp.Bind(
        backdropPanel.Get(),
        [this](const MouseEvent& event)
        {
            m_shouldCancelNextClick = false;

            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
            }

            if (GetTerrainState()->IsEnabled())
            {
                GetTerrainState()->EndStroke();
            }

            CameraController* controller = activeViewport->GetCamera()->GetCameraController();
            
            if (controller != nullptr)
            {
                InputHandlerBase* inputHandler = controller->GetInputHandler();

                if (inputHandler != nullptr)
                {
                    inputHandler->OnMouseUp(event);
                }
            }

            if (EditorGizmoBase* gizmo = GetSelectedGizmo(); gizmo && gizmo->IsDragging())
            {
                gizmo->OnDragEnd(activeViewport->GetCamera(), event);
            }

            if (IsMeshEditDragActive())
            {
                EndMeshEditDrag(true);
            }

            return UIEventHandlerResult::OK;
        }));

    backdropPanel->OnKeyDown.RemoveAllFromSet(m_delegateHandlers);
    m_delegateHandlers.Add(backdropPanel->OnKeyDown.Bind(
        backdropPanel.Get(),
        [this](const KeyboardEvent& event)
        {
            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
            }

            if (event.keyCode == KeyCode::KEY_ESCAPE && m_meshEditState.enabled)
            {
                if (BackOutOfMeshEditState())
                {
                    return UIEventHandlerResult::STOP_BUBBLING;
                }
            }

            ///Keyboard shortcuts
            if (g_appContext.IsValid()
                && g_appContext->GetMainWindow() != nullptr
                && g_appContext->GetMainWindow()->GetInputManager()->IsCtrlDown()
                && !IsSimulating()
                && !IsMeshEditDragActive())
            {
                const bool isShiftDown = g_appContext->GetMainWindow()->GetInputManager()->IsShiftDown();

                Name commandName;

                switch (event.keyCode)
                {
                case KeyCode::KEY_Z:
                    commandName = isShiftDown ? NAME("EditorCommandRedo") : NAME("EditorCommandUndo");
                    break;
                case KeyCode::KEY_Y:
                    commandName = NAME("EditorCommandRedo");
                    break;
                case KeyCode::KEY_C:
                    commandName = NAME("EditorCommandCopy");
                    break;
                case KeyCode::KEY_V:
                    commandName = NAME("EditorCommandPaste");
                    break;
                case KeyCode::KEY_A:
                    commandName = NAME("EditorCommandSelectAll");
                    break;
                default:
                    break;
                }

                if (commandName.IsValid())
                {
                    ExecuteCommandByName(commandName, String::empty);

                    return UIEventHandlerResult::STOP_BUBBLING;
                }
            }

            if (IsMeshEditDragActive())
            {
                int lockedAxis = -1;

                switch (event.keyCode)
                {
                case KeyCode::KEY_X:
                    lockedAxis = 0;
                    break;
                case KeyCode::KEY_Y:
                    lockedAxis = 1;
                    break;
                case KeyCode::KEY_Z:
                    lockedAxis = 2;
                    break;
                default:
                    break;
                }

                if (lockedAxis != -1)
                {
                    SetMeshEditDragLockedAxis(activeViewport->GetCamera(), event, lockedAxis);

                    return UIEventHandlerResult::STOP_BUBBLING;
                }
            }

            CameraController* controller = activeViewport->GetCamera()->GetCameraController();

            if (controller != nullptr)
            {
                InputHandlerBase* inputHandler = controller->GetInputHandler();

                if (inputHandler != nullptr)
                {
                    if (inputHandler->OnKeyDown(event))
                    {
                        return UIEventHandlerResult::STOP_BUBBLING;
                    }
                }
            }

            return UIEventHandlerResult::OK;
        }));

    backdropPanel->OnKeyUp.RemoveAllFromSet(m_delegateHandlers);
    m_delegateHandlers.Add(backdropPanel->OnKeyUp.Bind(
        backdropPanel.Get(),
        [this](const KeyboardEvent& event)
        {
            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
            }

            CameraController* controller = activeViewport->GetCamera()->GetCameraController();
            
            if (controller != nullptr)
            {
                InputHandlerBase* inputHandler = controller->GetInputHandler();

                if (inputHandler != nullptr)
                {
                    inputHandler->OnKeyUp(event);
                }
            }

            return UIEventHandlerResult::OK;
        }));

    backdropPanel->OnGainFocus.RemoveAllFromSet(m_delegateHandlers);
    m_delegateHandlers.Add(backdropPanel->OnGainFocus.Bind(
        backdropPanel.Get(),
        [this](const MouseEvent& event)
        {
            m_editorCameraEnabled = true;

            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
            }

            CameraController* controller = activeViewport->GetCamera()->GetCameraController();
            
            if (controller != nullptr)
            {
                InputHandlerBase* inputHandler = controller->GetInputHandler();

                if (inputHandler != nullptr)
                {
                    if (inputHandler->OnGainFocus(event))
                    {
                        return UIEventHandlerResult::STOP_BUBBLING;
                    }
                }
            }

            return UIEventHandlerResult::OK;
        }));

    backdropPanel->OnLoseFocus.RemoveAllFromSet(m_delegateHandlers);
    m_delegateHandlers.Add(backdropPanel->OnLoseFocus.Bind(
        backdropPanel.Get(),
        [this](const MouseEvent& event)
        {
            m_editorCameraEnabled = false;

            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
            }

            CameraController* controller = activeViewport->GetCamera()->GetCameraController();
            
            if (controller != nullptr)
            {
                InputHandlerBase* inputHandler = controller->GetInputHandler();

                if (inputHandler != nullptr)
                {
                    inputHandler->OnLoseFocus(event);
                }
            }

            return UIEventHandlerResult::OK;
        }));
}

void EditorSubsystem::SetSelectedBucket(uint32 bucketIndex)
{
    if (m_selectedBucketIndex == bucketIndex)
    {
        return;
    }

    m_selectedBucketIndex = bucketIndex;

    OnSelectedBucketChanged(bucketIndex);
}

bool EditorSubsystem::ExecuteCommand(const Handle<EditorCommandBase>& command)
{
    if (!command)
    {
        return false;
    }

    if (IsSimulating() && !command->AllowedWhileSimulating())
    {
        HYP_LOG(Editor, Warning, "Cannot execute command '{}' while simulation is active", command->InstanceClass()->GetName());

        return false;
    }

    if (IsOnThread(g_simThread))
    {
        command->Execute(this);
    }
    else
    {
        GetThreadById(g_simThread)->GetScheduler().Enqueue(
            [this, weakThis = MakeWeakRef(this), command = command]()
            {
                Handle<EditorSubsystem> strongThis = weakThis.Lock();
                if (!strongThis)
                {
                    HYP_LOG(Editor, Error, "Failed to lock EditorSubsystem from weak reference in ExecuteCommand");
                    return;
                }

                command->Execute(this);
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);
    }

    return true;
}

bool EditorSubsystem::ExecuteCommandByName(Name name, const String& args)
{
    if (!name.IsValid())
    {
        return false;
    }

    const Class* commandClass = ClassRegistry::GetInstance().GetClass(name);
    if (!commandClass || !commandClass->IsDerivedFrom(EditorCommandBase::StaticClass()))
    {
        String nameStr = name.ToString();
        if (!nameStr.EndsWith("Commandlet"))
        {
            // try again with "Commandlet" appended to the name
            String nameWithCommandlet = nameStr + "Commandlet";
            commandClass = ClassRegistry::GetInstance().GetClass(nameWithCommandlet);
        }

        if (!commandClass || !commandClass->IsDerivedFrom(EditorCommandBase::StaticClass()))
        {
            HYP_LOG(Editor, Error, "Invalid command class: {}", name);
            return false;
        }
    }

    BoxedValue instanceData;
    if (!commandClass->CreateInstance(instanceData))
    {
        HYP_LOG(Editor, Error, "Failed to construct command instance: {}", name);
        return false;
    }

    Handle<EditorCommandBase>& command = instanceData.Get<Handle<EditorCommandBase>>();
    AssertDebug(command != nullptr);

    command->SetArguments(args.Split(' '));

    return ExecuteCommand(command);
}

void EditorSubsystem::NewProject()
{
    Handle<EditorProject> project = EditorProject::CreateNew();

    Handle<Scene> mainScene = MakeHandle<Scene>();
    mainScene->SetName(NAME("MainScene"));
    mainScene->SetSceneFlags(SceneFlags::DEFAULT & ~SceneFlags::STREAMED);
    project->AddScene(mainScene);

    Handle<DirectionalLight> sun = MakeHandle<DirectionalLight>();
    sun->SetName(NAME("SunLight"));
    sun->SetDirection(Vec3f(0.0f, 0.3f, 0.9f).Normalize());
    sun->SetColor(Color(Vec4f(1.0f, 0.9f, 0.8f, 1.0f)));
    sun->SetIntensity(18.0f);
    InitObject(sun);

    mainScene->GetRoot()->AddChild(sun);

    // Add player entity
    Handle<Entity> playerEntity = MakeHandle<Entity>();
    playerEntity->SetName(NAME("Player"));
    playerEntity->SetWorldTranslation(Vec3f(0.0f, 1.0f, -5.0f));
    playerEntity->SetIsDynamic(true);
    InitObject(playerEntity);

    Handle<CapsulePhysicsShape> capsuleShape = MakeHandle<CapsulePhysicsShape>();
    capsuleShape->SetName(NAME_FMT("{}CapsuleShape", playerEntity->GetName()));
    InitObject(capsuleShape);
    GetCurrentAssetRegistry()->PutAssetUnique(capsuleShape);

    Handle<Camera> camera = MakeHandle<Camera>();
    camera->SetDimensions(Vec2i(1920, 1080));
    camera->SetName(NAME("Camera"));
    camera->SetLocalTranslation(Vec3f(0.0f, 1.6f, 0.0f));
    camera->SetCameraFlags(CameraFlags::MatchWindowSize | CameraFlags::HasStreamingVolume);
    camera->SetFarClip(3000.0f);
    camera->SetNearClip(0.1f);
    camera->AddTag<EntityTag::PrimaryCamera>();

    Handle<FirstPersonCameraController> firstPersonController = MakeHandle<FirstPersonCameraController>();
    camera->AddCameraController(firstPersonController);

    InitObject(camera);

    mainScene->GetRoot()->AddChild(playerEntity);

    CharacterControllerComponent characterControllerComponent;
    characterControllerComponent.shape = capsuleShape;
    playerEntity->AddComponent<CharacterControllerComponent>(characterControllerComponent);

    playerEntity->AddTag<EntityTag::Player>();

    playerEntity->AddChild(camera);

    // Handle<Scene> streamedScene = MakeHandle<Scene>();
    // streamedScene->SetName(NAME("StreamedScene"));
    // streamedScene->SetSceneFlags(SceneFlags::DEFAULT);
    // project->AddScene(streamedScene);

    // add dynamic skybox
    project->GetWorld()->AddSystemT<DynamicSkySystem>();

    OpenProject(project);
}

void EditorSubsystem::CloseProject(bool shutdownWorld)
{
    AssertOnThread(g_simThread);

    if (m_currentProject)
    {
        ShutdownProjectWorld(m_currentProject, /* shutdownWorld */ shutdownWorld);
        OnProjectClosing(m_currentProject);

        m_currentProject->SetEditorSubsystem(WeakHandle<EditorSubsystem>::Null());
        m_currentProject->Close(/* shutdownWorld */ shutdownWorld);

        m_currentProject.Reset();
    }

    RenderProxyList& pickRpl = g_editorState->GetPickCache().GetRenderProxyList();

    pickRpl.BeginWrite();
    pickRpl.ClearAll();
    pickRpl.EndWrite();

    g_editorState->GetPickCache().Clear();
}

void EditorSubsystem::OpenProject(const Handle<EditorProject>& project)
{
    AssertOnThread(g_simThread);

    if (project == m_currentProject)
    {
        return;
    }

    const bool isSimulationStateChange = IsSimulating();
    const bool isStartSimulation = isSimulationStateChange && project != m_preSimulationProject;

    CloseProject(/* shutdownWorld*/ true);

    if (!project.IsValid())
    {
        return;
    }

    project->SetEditorSubsystem(MakeWeakRef(this));

    m_currentProject = project;

    InitializeProjectWorld(m_currentProject, isStartSimulation);

    OnProjectOpened(m_currentProject);

    g_editorState->SetCurrentProject(m_currentProject, isSimulationStateChange);
}

void EditorSubsystem::ShowImportContentDialog()
{
    ShowOpenFileDialog(
        "Select the file(s) to import into the project",
        EngineGlobals::GetDataDirectory(),
        { "obj", "fbx", "jpg", "jpeg", "png", "tga", "bmp", "ogre.xml" },
        /* allowMultiple */ true, /* allowDirectories */ false,
        [](TResult<Array<FilePath>>&& result)
        {
            if (result.HasError())
            {
                HYP_LOG(Editor, Error, "Failed to select files to import: {}", result.GetError().GetMessage());

                return;
            }

            // Create identifier based on the common folder of the assets
            String identifier = "Unknown";

            if (result.GetValue().Any())
            {
                identifier = result.GetValue()[0].BasePath().Basename();
            }

            // Queue up an asset batch
            AssetBatch* batch = AssetManager::GetInstance()->CreateBatch(identifier);

            for (const FilePath& file : result.GetValue())
            {
                batch->Add(file.Basename(), FilePath::Relative(file, CoreApi::GetExecutablePath()));
            }

            batch->OnComplete
                .Bind([](AssetMap& results)
                      {
                          HYP_LOG(Editor, Verbose, "{} assets loaded.", results.Size());

                          /// \todo Open folder the assets ended up in
                      })
                .Detach();

            batch->LoadAsync();

            // Note: The batch will be destroyed automatically by AssetManager when complete
        });
}

void EditorSubsystem::SetFocusedNode(const Handle<Node>& focusedNode, bool shouldSelectInOutline)
{
    if (focusedNode == m_focusedNode)
    {
        return;
    }

    const Handle<Node> previousFocusedNode = m_focusedNode.Lock();

    m_focusedNode = focusedNode;

    if (m_meshEditState.enabled && focusedNode != m_meshEditState.targetNode)
    {
        Entity* entity = DynamicCast<Entity>(focusedNode.Get());
        MeshComponent* meshComponent = entity ? entity->TryGetComponent<MeshComponent>() : nullptr;

        if (meshComponent && meshComponent->mesh.IsValid())
        {
            EndMeshEditDrag(/* saveEdits */ true);
            CommitMeshEdits();

            m_meshEditState.targetNode = focusedNode.ToWeak();

            m_meshEditState.actionStack = MakeHandle<EditorActionStack>(m_currentProject.ToWeak());

            SetSelectedMeshEditFace({});
            m_meshEditState.hoveredFace.Unset();

            OnMeshEditStateChanged();
        }
        else
        {
            ExitMeshEditMode(/* saveEdits */ true);
        }
    }

    if (Handle<Node> focusedNode = m_focusedNode.Lock(); focusedNode.IsValid())
    {
        if (focusedNode->GetScene() != nullptr)
        {
            if (Entity* entity = DynamicCast<Entity>(focusedNode))
            {
                entity->AddTag<EntityTag::FocusedInEditor>();
            }
        }

        HYP_LOG(Editor, Verbose, "Set focused node: {}\t{}\t is static ? {}", focusedNode->GetName(), focusedNode->GetWorldTranslation(),
                focusedNode->IsStatic());

        if (!m_meshEditState.enabled)
        {
            if (focusedNode->IsA<VolumeBase>() && StaticCast<VolumeBase>(focusedNode)->useVolumeEditTool)
            {
                SetSelectedManipulationMode(EditorManipulationMode::ReshapeVolume);
            }
            else if (GetSelectedManipulationMode() == EditorManipulationMode::None
                     || GetSelectedManipulationMode() == EditorManipulationMode::ReshapeVolume)
            {
                SetSelectedManipulationMode(EditorManipulationMode::Translate);
            }
        }

        EditorGizmoBase* gizmo = GetSelectedGizmo();

        if (gizmo)
        {
            gizmo->SetFocusedNode(focusedNode);
        }
    }

    if (previousFocusedNode != nullptr)
    {
        if (Entity* entity = DynamicCast<Entity>(previousFocusedNode))
        {
            entity->RemoveTag<EntityTag::FocusedInEditor>();
        }
    }

    // So, we now use SelectedNodes for multi-select, but for backwards compatibility and some other stuff that needs a single
    // 'focused' node (i.e where to place the gizmo? or maybe it should use avg position?) 
    // We check if the selection includes the focused node.
    //  - if it does, we do nothing,
    //  - otherwise, clear the selection, and set selection to be *just* the focused node.
    if (!m_selectedNodes.Contains(focusedNode))
    {
        SetSelectedNodes({ focusedNode });
    }

    OnFocusedNodeChanged(focusedNode, previousFocusedNode, shouldSelectInOutline);
}

Handle<Scene> EditorSubsystem::GetActiveScene() const
{
    AssertOnThread(g_simThread);
    return m_activeScene.Lock();
}

String EditorSubsystem::GetCodeEditor() const
{
    return String(g_cvCodeEditor.Get());
}

Array<Name> EditorSubsystem::GetAvailableWorldGridLayerClassNames() const
{
    Array<Name> result;

    auto functor =
        [&result](const Class* cls)
        {
            if (cls == nullptr || cls->IsAbstract() || !cls->IsDerivedFrom(WorldGridLayer::StaticClass()))
            {
                return IterationResult::CONTINUE;
            }

            result.PushBack(cls->GetName());

            return IterationResult::CONTINUE;
        };

    ClassRegistry::GetInstance().ForEachClass(functor);

    return result;
}

Handle<DynamicSkySystem> EditorSubsystem::GetDynamicSkySystem() const
{
    AssertOnThread(g_simThread);

    const Handle<World>& world = GetProjectWorld();

    if (!world.IsValid())
    {
        return Handle<DynamicSkySystem>::empty;
    }

    DynamicSkySystem* dynamicSkySystem = world->GetSystem<DynamicSkySystem>();

    if (!dynamicSkySystem)
    {
        return Handle<DynamicSkySystem>::empty;
    }

    return MakeStrongRef(dynamicSkySystem);
}

Handle<Node> EditorSubsystem::GetFocusedNode() const
{
    AssertOnThread(g_simThread);
    return m_focusedNode.Lock();
}

void EditorSubsystem::AddToSelection(const Handle<Node>& node)
{
    AssertOnThread(g_simThread);

    if (!node.IsValid())
    {
        return;
    }

    auto result = m_selectedNodes.Insert(node);

    if (result.second)
    {
        OnSelectionChanged();
    }
}

void EditorSubsystem::RemoveFromSelection(const Handle<Node>& node)
{
    AssertOnThread(g_simThread);

    if (!node.IsValid())
    {
        return;
    }

    auto it = m_selectedNodes.Find(node);

    if (it != m_selectedNodes.End())
    {
        m_selectedNodes.Erase(it);

        OnSelectionChanged();
    }
}

void EditorSubsystem::ClearSelection()
{
    AssertOnThread(g_simThread);

    if (m_selectedNodes.Empty())
    {
        return;
    }

    m_selectedNodes.Clear();

    OnSelectionChanged();
}

bool EditorSubsystem::IsNodeSelected(const Handle<Node>& node) const
{
    AssertOnThread(g_simThread);

    if (!node.IsValid())
    {
        return false;
    }

    return m_selectedNodes.Find(node) != m_selectedNodes.End();
}

void EditorSubsystem::SetSelectedNodes(const Array<Handle<Node>>& nodes)
{
    AssertOnThread(g_simThread);

    if (nodes.Empty())
    {
        ClearSelection();

        return;
    }

    m_selectedNodes = Set<Handle<Node>, EditorAllocator>(nodes.Begin(), nodes.End());

    OnSelectionChanged();
}

Array<Handle<Node>> EditorSubsystem::GetSelectedNodes() const
{
    AssertOnThread(g_simThread);

    Array<Handle<Node>> result;

    for (const Handle<Node>& node : m_selectedNodes)
    {
        result.PushBack(node);
    }

    return result;
}

Vec3f EditorSubsystem::CalculateSceneInsertionPoint(float desiredDistance, float offsetFromSurface) const
{
    EditorViewport* activeViewport = GetActiveViewport();
    if (activeViewport == nullptr)
    {
        return Vec3f::Zero();
    }

    const Vec3f cameraPosition = activeViewport->GetCamera()->GetWorldTranslation();
    const Vec3f cameraDirection = activeViewport->GetCamera()->GetDirection();

    Vec3f insertionPoint = cameraPosition + cameraDirection * desiredDistance;

    const Ray ray { cameraPosition, cameraDirection };

    RayTestResults results;

    Handle<Scene> activeScene = m_activeScene.Lock();
    if (!activeScene)
    {
        return insertionPoint;
    }

    // raytest using scene's octree
    if ((activeScene->GetSceneFlags() & SceneFlags::HAS_OCTREE) && activeScene->GetOctree().TestRay(ray, results, RayTestFlags::TestBVH))
    {
        const RayHit& closestHit = results.Front();

        if (closestHit.distance < desiredDistance)
        {
            // offset the object slightly to avoid clipping
            insertionPoint = closestHit.hitpoint - cameraDirection * offsetFromSurface;

            const float distanceFromCamera = (insertionPoint - cameraPosition).Length();

            // min 1 world unit
            if (distanceFromCamera < 1.0f)
            {
                insertionPoint = cameraPosition + cameraDirection * 1.0f;
            }
        }
    }

    return insertionPoint;
}

void EditorSubsystem::UpdateNormalizedCubeSpherePreview(uint32 numDivisions)
{
    AssertOnThread(g_simThread);

    numDivisions = MathUtil::Max(numDivisions, 1u);

    Handle<Scene> activeScene = GetActiveScene();
    if (!activeScene.IsValid())
    {
        return;
    }

    Handle<Mesh> mesh = MeshBuilder::NormalizedCubeSphere(numDivisions);
    mesh->SetIsTransient(true);
    mesh->SetName(NAME("NormalizedCubeSphereMesh_Preview"));

    GetCurrentAssetRegistry()->PutAssetUnique(mesh);

    InitObject(mesh);

    if (!m_meshPreviewEntity.IsValid())
    {
        const Vec3f insertionPoint = CalculateSceneInsertionPoint(5.0f, 0.5f);

        MaterialAttributes attributes;
        attributes.shaderName = NAME("GeometryPass");

        m_meshPreviewMaterial = MakeHandle<Material>(NAME("NormalizedCubeSpherePreviewMaterial"), attributes);
        m_meshPreviewMaterial->SetIsTransient(true);
        InitObject(m_meshPreviewMaterial);

        m_meshPreviewEntity = MakeHandle<Entity>();
        m_meshPreviewEntity->SetName(NAME("NormalizedCubeSpherePreviewEntity"));
        m_meshPreviewEntity->SetWorldTranslation(insertionPoint);

        activeScene->GetRoot()->AddChild(m_meshPreviewEntity);

        MeshComponent meshComponent;
        meshComponent.mesh = mesh;
        meshComponent.material = m_meshPreviewMaterial;
        m_meshPreviewEntity->AddComponent<MeshComponent>(meshComponent);
    }
    else if (MeshComponent* meshComponent = m_meshPreviewEntity->TryGetComponent<MeshComponent>())
    {
        meshComponent->mesh = mesh;
        m_meshPreviewEntity->AddTag<EntityTag::UpdateRenderProxy>();
    }

    m_meshPreviewEntity->SetLocalBounds(mesh->GetAABB());
}

void EditorSubsystem::CommitMeshPreview()
{
    AssertOnThread(g_simThread);

    if (!m_meshPreviewEntity.IsValid())
    {
        return;
    }

    Handle<Entity> entity = m_meshPreviewEntity;

    m_meshPreviewEntity->Remove();
    m_meshPreviewEntity.Reset();

    m_meshPreviewMaterial.Reset();

    Handle<EditorProject> currentProject = GetCurrentProject();
    if (!currentProject.IsValid())
    {
        HYP_LOG(Editor, Error, "No project loaded; cannot commit mesh preview!");

        entity->Remove();

        return;
    }

    MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>();
    if (!meshComponent || !meshComponent->mesh.IsValid())
    {
        entity->Remove();

        return;
    }

    Handle<Mesh> mesh = meshComponent->mesh;

    // Replace the transient preview material with a proper, non-transient one for the committed entity
    MaterialAttributes attributes;
    attributes.shaderName = NAME("GeometryPass");

    Handle<Material> material = MakeHandle<Material>(NAME("NormalizedCubeSphereMaterial"), attributes);
    InitObject(material);

    meshComponent->material = material;

    entity->SetName(NAME("NormalizedCubeSphereEntity"));

    mesh->SetName(NAME("NormalizedCubeSphereMesh"));

    Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
        "Add Normalized Cube Sphere",
        Proc<EditorActionFunctions()>(
            [entity, mesh, material]() -> EditorActionFunctions
            {
                return EditorActionFunctions {
                    .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                        [entity, mesh, material](EditorSubsystem* subsystem, EditorProject*)
                        {
                            GetCurrentAssetRegistry()->PutAsset(mesh);
                            GetCurrentAssetRegistry()->PutAsset(material);

                            Handle<Scene> activeScene = subsystem->GetActiveScene();
                            if (activeScene.IsValid())
                            {
                                activeScene->GetRoot()->AddChild(entity);
                            }
                        }),
                    .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                        [entity, mesh, material](EditorSubsystem*, EditorProject*)
                        {
                            GetCurrentAssetRegistry()->RemoveAsset(mesh);
                            GetCurrentAssetRegistry()->RemoveAsset(material);

                            entity->Remove();
                        })
                };
            }));

    InitObject(action);

    currentProject->GetActionStack()->PushAction(action);
}

void EditorSubsystem::CancelMeshPreview()
{
    AssertOnThread(g_simThread);

    if (m_meshPreviewEntity.IsValid())
    {
        m_meshPreviewEntity->Remove();
        m_meshPreviewEntity = Handle<Entity>::Null();
    }

    m_meshPreviewMaterial = Handle<Material>::Null();
}

void EditorSubsystem::SetHoveredGizmo(
    const MouseEvent& event,
    EditorGizmoBase* gizmo,
    const Handle<Node>& gizmoNode)
{
    EditorViewport* activeViewport = GetActiveViewport();
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

void EditorSubsystem::SetActiveScene(const Handle<Scene>& scene)
{
    if (scene == m_activeScene)
    {
        return;
    }

    m_activeScene = scene;

    OnActiveSceneChanged(scene);
}

EditorViewport* EditorSubsystem::GetActiveViewport() const
{
    AssertOnThread(g_simThread);

    return m_editorViewports.Empty() ? nullptr : m_editorViewports[0];
}

void EditorSubsystem::SetActiveViewport(EditorViewport* viewport)
{
    AssertOnThread(g_simThread);

    if (!viewport)
    {
        return;
    }

    auto it = m_editorViewports.Find(viewport);
    if (it != m_editorViewports.End())
    {
        Handle<EditorViewport> viewportStrong = MakeStrongRef(viewport);
        m_editorViewports.PushFront(viewportStrong);
        OnActiveViewportChanged(viewportStrong);
        return;
    }

    const size_t idx = m_editorViewports.IndexOf(it);
    AssertDebug(idx != -1);

    if (idx == 0)
    {
        return; // already active VP
    }

    std::swap(m_editorViewports[0], m_editorViewports[idx]);

    OnActiveViewportChanged(MakeStrongRef(viewport));
}

void EditorSubsystem::AddViewport(const Handle<EditorViewport>& viewport)
{
    AssertOnThread(g_simThread);

    Assert(viewport != nullptr);

    if (!viewport)
    {
        return;
    }

    InitObject(viewport);
    Handle<EditorViewport> viewportStrong = MakeStrongRef(viewport);

    viewport->OnAdded(this);
    m_editorViewports.PushBack(viewportStrong);

    // active VP is always the first one in the array
    // if size == 1 it's because we just added the first one
    if (m_editorViewports.Size() == 1)
    {
        OnActiveViewportChanged(viewportStrong);
    }
}

void EditorSubsystem::RemoveViewport(EditorViewport* viewport)
{
    AssertOnThread(g_simThread);

    Assert(viewport != nullptr);

    if (!viewport)
    {
        return;
    }

    auto it = m_editorViewports.Find(viewport);
    if (it != m_editorViewports.End())
    {
        Handle<EditorViewport> viewportStrong;

        const size_t idx = m_editorViewports.IndexOf(it);

        if (idx == 0)
        {
            viewportStrong = MakeStrongRef(*it);
        }

        m_editorViewports.Erase(it);

        if (viewportStrong)
        {
            OnActiveViewportChanged(viewportStrong);
        }

        viewport->OnRemoved(this);
    }
}

void EditorSubsystem::InitializeProjectWorld(const Handle<EditorProject>& project, bool isStartSimulation)
{
    Assert(project != nullptr);
    InitObject(project);
    
    g_editorState->GetPickCache().Clear();

    InitializeGizmos();

    Game* gameInstance = project->GetGame();
    Assert(gameInstance != nullptr);

    const Handle<AssetRegistry>& assetRegistry = gameInstance->GetAssetRegistry();
    Assert(assetRegistry.IsValid());
    PushAssetRegistry(assetRegistry);

    Handle<World> world;

    if (isStartSimulation)
    {
        // Loads the world
        gameInstance->Initialize();

        world = gameInstance->GetWorld();
    }
    else
    {
        world = project->GetWorld();

        if (!world.IsValid())
        {
            if ((world = gameInstance->LoadWorld(Game::s_nameMainWorld)) && world.IsValid())
            {
                world->SetGame(gameInstance);
            }
        }

        project->SetEditWorld(world);

        // This world was kept alive across the simulation transition, but Game::Shutdown() evicted every
        // asset from the registry cache so we need to re-register all nested asset objects
        if (world.IsValid() && !world->IsTransient())
        {
            assetRegistry->Initialize();
            assetRegistry->PutAssetsDeep(world);

            if (const Handle<WorldGrid>& worldGrid = world->GetWorldGrid(); worldGrid.IsValid())
            {
                for (const Handle<WorldGridLayer>& layer : worldGrid->GetLayers())
                {
                    layer->EnsureStreamingObjectsRegistered();
                }
            }
        }
    }

    Assert(world.IsValid());

    g_engineDriver->AddWorld(world);

    if (!isStartSimulation)
    {
        // restarts the grid shut down for the simulation - see ShutdownProjectWorld()
        if (const Handle<WorldGrid>& worldGrid = world->GetWorldGrid(); worldGrid.IsValid())
        {
            worldGrid->Restart();
        }
    }

    Handle<Scene> activeScene;

    for (const Handle<Scene>& scene : world->GetScenes())
    {
        Assert(scene != nullptr);

        HYP_LOG(Editor, Verbose, "Found scene '{}' in project '{}' with flags: {}", *scene->GetName(), *project->GetName(),
                EnumToString(scene->GetSceneFlags()));

        if ((scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) != SceneFlags::FOREGROUND)
        {
            continue;
        }

        if (!activeScene.IsValid())
        {
            activeScene = scene;
        }
    }

    if (!activeScene.IsValid())
    {
        HYP_LOG(Editor, Warning, "No foreground scenes found in project {}!", *project->GetName());
    }

    if (!isStartSimulation)
    {
        for (const Handle<EditorViewport>& vp : m_editorViewports)
        {
            vp->OnAdded(this);
        }
    }

    m_delegateHandlers.Add(world->OnSceneAdded.Bind(
        world.Get(),
        [this, projectWeak = project.ToWeak(), isStartSimulation](World*, const Handle<Scene>& scene)
        {
            Assert(scene != nullptr);
            Assert(scene != m_editorScene);

            if ((scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) != SceneFlags::FOREGROUND)
            {
                return;
            }

            Handle<EditorProject> project = projectWeak.Lock();
            Assert(project != nullptr);

            if (!isStartSimulation)
            {
                // Add scene to all editor views
                for (const Handle<EditorViewport>& vp : m_editorViewports)
                {
                    vp->OnSceneAdded(scene);
                }
            }

            if (!m_activeScene)
            {
                SetActiveScene(scene);
            }
        }));

    m_delegateHandlers.Add(world->OnSceneRemoved.Bind(
        world.Get(),
        [this, projectWeak = project.ToWeak(), isStartSimulation](World*, Scene* scene)
        {
            Assert(scene != nullptr);
            Assert(scene != m_editorScene);

            Handle<EditorProject> project = projectWeak.Lock();
            Assert(project != nullptr);

            scene->OnRootNodeChanged.RemoveAllFromSet(m_delegateHandlers);

            if (!isStartSimulation)
            {
                // remove from all editor views
                for (const Handle<EditorViewport>& vp : m_editorViewports)
                {
                    vp->OnSceneRemoved(scene);
                }
            }

            // StopWatchingNode(scene->GetRoot());

            // GetWorld()->RemoveScene(scene);

            // // reinitialize scene selector on scene remove
            // InitActiveSceneSelection();
        }));

    BakerSubsystem* bakerSubsystem = world->GetSubsystem<BakerSubsystem>();

    if (!bakerSubsystem)
    {
        bakerSubsystem = world->AddSubsystem<BakerSubsystem>();
    }

    SetActiveScene(activeScene);
}

void EditorSubsystem::UpdateBakeStatus()
{
    if (IsSimulating())
    {
        return;
    }

    if (m_bakeStatusUpdateTimer.Waiting())
    {
        return;
    }

    m_bakeStatusUpdateTimer.NextTick();

    static const Name s_bakeStatusMessageKey = NAME("BakeStatus");

    if (!m_messagesOverlay.IsValid())
    {
        return;
    }

    if (!m_currentProject.IsValid())
    {
        m_messagesOverlay->ClearMessage(s_bakeStatusMessageKey);

        return;
    }

    const Handle<World>& world = m_currentProject->GetWorld();

    if (!world.IsValid())
    {
        m_messagesOverlay->ClearMessage(s_bakeStatusMessageKey);

        return;
    }

    // Epochs are computed from the live scene, which only reflects the active swatch's overrides,
    // so other swatches' stored epochs can never match here
    const Handle<Swatch> activeSwatch = world->TryGetSwatch(world->GetActiveSwatchName());

    Array<String, EditorAllocator> lightmapVolumeNames;
    Array<String, EditorAllocator> reflectionProbeNames;
    Array<String, EditorAllocator> irradianceProbeNames;

    for (const Handle<Scene>& scene : world->GetScenes())
    {
        if (!scene)
        {
            continue;
        }

        for (auto [volume] : scene->GetEntityManager()->GetEntitySet<EntityType<LightmapVolume>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            if (!volume->GetAtlasTexture(0, LightmapVolume::IrradianceTexture).IsValid())
            {
                // Not baked yet, but we consider it 'out of date', so we dont have LMVs left unbaked in the scene.
                lightmapVolumeNames.PushBack(*volume->GetName());

                continue;
            }

            if (!activeSwatch.IsValid())
            {
                continue;
            }

            Baking::BakeLayer& bakeLayer = activeSwatch->bakeLayer;

            uint64 storedEpoch;

            // not tracked yet counts as out of date. bake it to track it
            if (!bakeLayer.TryGetAssetEpoch<Baking::BakeLayerCategory::LightReceiver>(*volume, storedEpoch)
                || storedEpoch != Baking::BakeEpoch::ComputeEpoch(*volume, bakeLayer))
            {
                lightmapVolumeNames.PushBack(*volume->GetName());
            }
        }

        for (auto [probe] : scene->GetEntityManager()->GetEntitySet<EntityType<EnvProbe>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            if (!probe->IsBaked())
            {
                // Fine. no realtime probes are considered.
                continue;
            }

            Array<String, EditorAllocator>* outNames;

            if (DynamicCast<ReflectionProbe>(probe))
            {
                outNames = &reflectionProbeNames;
            }
            else if (DynamicCast<IrradianceProbe>(probe))
            {
                outNames = &irradianceProbeNames;
            }
            else
            {
                continue;
            }

            if (!activeSwatch.IsValid())
            {
                continue;
            }

            Baking::BakeLayer& bakeLayer = activeSwatch->bakeLayer;

            uint64 storedEpoch;

            // not tracked yet counts as out of date. bake it to track it
            if (!bakeLayer.TryGetAssetEpoch<Baking::BakeLayerCategory::LightReceiver>(*probe, storedEpoch)
                || storedEpoch != Baking::BakeEpoch::ComputeEpoch(*probe, bakeLayer))
            {
                outNames->PushBack(*probe->GetName());
            }
        }
    }

    if (lightmapVolumeNames.Empty() && reflectionProbeNames.Empty() && irradianceProbeNames.Empty())
    {
        m_messagesOverlay->ClearMessage(s_bakeStatusMessageKey);

        return;
    }

    String text;

    auto appendSection = [&text](ANSIStringView label, Span<const String> names)
    {
        static constexpr uint32 MaxToShow = 5;

        if (names.Size() == 0)
        {
            return;
        }

        if (text.Any())
        {
            text += "\n\n";
        }

        text += HYP_FORMAT("{} {} need a rebake", names.Size(), label);

        const uint32 numToShow = MathUtil::Min(uint32(names.Size()), MaxToShow);

        for (uint32 i = 0; i < numToShow; i++)
        {
            text += "\n";
            text += names[i];
        }

        if (uint32(names.Size()) > numToShow)
        {
            text += HYP_FORMAT("\n... and {} more", uint32(names.Size()) - numToShow);
        }
    };

    appendSection("LightmapVolumes", lightmapVolumeNames);
    appendSection("ReflectionProbes", reflectionProbeNames);
    appendSection("IrradianceProbes", irradianceProbeNames);

    m_messagesOverlay->PutMessage(MessageEntry {
        s_bakeStatusMessageKey,
        text,
        Color(1.0f, 0.7f, 0.1f, 1.0f)
    });
}

void EditorSubsystem::ShutdownProjectWorld(const Handle<EditorProject>& project, bool shutdownWorld)
{
    Assert(project.IsValid());

    Game* gameInstance = project->GetGame();
    Assert(gameInstance != nullptr);

    const Handle<World>& world = project->GetWorld();
    Assert(world.IsValid());

    g_editorState->GetPickCache().Clear();

    // Shutdown to reinitialize gizmos after project is opened
    ShutdownGizmos();

    m_focusedNode.Reset();
    m_selectedNodes.Clear();

    if (m_highlightNode.IsValid())
    {
        m_highlightNode->Remove();
    }

    SetActiveScene(Handle<Scene>::Null());

    // Must run before RemoveWorld() below -- if shutdownWorld is true, World::Shutdown() moves
    // m_scenes out from under the World, so world->GetScenes() would come back empty afterward.
    for (const Handle<Scene>& scene : world->GetScenes())
    {
        if (!scene.IsValid())
        {
            continue;
        }

        scene->OnRootNodeChanged.RemoveAllFromSet(m_delegateHandlers);
    }

    const bool isSimulationProject = IsSimulating() && project != m_preSimulationProject;

    if (!isSimulationProject)
    {
        for (const Handle<EditorViewport>& vp : m_editorViewports)
        {
            vp->OnRemoved(this);
        }
    }


    BakerSubsystem* bakerSubsystem = world->GetSubsystem<BakerSubsystem>();
    if (bakerSubsystem)
    {
        //bakerSubsystem->CancelAllBakes();
    }

    world->OnSceneAdded.RemoveAllFromSet(m_delegateHandlers);
    world->OnSceneRemoved.RemoveAllFromSet(m_delegateHandlers);

    gameInstance->OnGameStateChange.RemoveAllFromSet(m_delegateHandlers);

    // a world kept alive for the simulation would otherwise keep streaming its layers, which store generated data
    // (eg. terrain cells) into the simulation's asset registry
    if (!shutdownWorld)
    {
        if (const Handle<WorldGrid>& worldGrid = world->GetWorldGrid(); worldGrid.IsValid())
        {
            worldGrid->Shutdown();
        }
    }

    g_engineDriver->RemoveWorld(world, shutdownWorld);

    PopAssetRegistry(gameInstance->GetAssetRegistry().Get());
}

#endif

#pragma endregion EditorSubsystem

} // namespace Hyperion
