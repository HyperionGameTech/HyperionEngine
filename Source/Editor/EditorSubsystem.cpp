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

#include <Scene/Systems/Editor/EditorSpriteSystem.hpp>

#include <DotNET/DotNETHost.hpp>

#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/View.hpp>
#include <Scene/Light.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/ProbeVolume.hpp>
#include <Scene/FogVolume.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Prefab.hpp>

#include <Scene/System.hpp>
#include <Scene/Systems/ScriptSystem.hpp>
#include <Scene/Systems/MeshSystem.hpp>

#include <Scene/Sky/DynamicSkySystem.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/VisibilityStateComponent.hpp>
#include <Scene/Components/BoundingBoxComponent.hpp>
#include <Scene/Components/TransformComponent.hpp>

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

#include <Core/IO/ByteWriter.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>

#include <Rendering/Util/MeshBuilder.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/RendererMain.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/RenderInterface.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

// temp
#include <Baking/BakerSubsystem.hpp>
#include <Baking/BakeData.hpp>

// for EnumToString
#include <Core/Reflection/Enum.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Scripting/ScriptingService.hpp>

#include <Framework/Game.hpp>

#include <Framework/EngineDriver.hpp>
#include <Rendering/DebugDrawer.hpp>

#include <HyperionEngine.hpp>

#include <EditorSubsystem.generated.inl>

namespace Hyperion {

HYP_DEFINE_LOG_CHANNEL(Editor);

namespace CoreApi {
CORE_API extern const FilePath& GetExecutablePath();
} // namespace CoreApi

static ShaderPropertyId s_propUniformScaling = InternShaderProperty(ShaderProperty(NAME("UNIFORM_SCALING")));

#pragma region GenerateLightmapsEditorTask

GenerateLightmapsEditorTask::GenerateLightmapsEditorTask(const Handle<LightmapVolume>& volume)
    : GenerateLightmapsEditorTask(Array<Handle<ObjectBase>> { { StaticCast<ObjectBase>(volume) } })
{
}

GenerateLightmapsEditorTask::GenerateLightmapsEditorTask(const Handle<EnvProbe>& probe)
    : GenerateLightmapsEditorTask(Array<Handle<ObjectBase>> { { StaticCast<ObjectBase>(probe) } })
{
}

GenerateLightmapsEditorTask::GenerateLightmapsEditorTask(const Array<Handle<ObjectBase>>& sources)
    : TickableEditorTask(),
      m_sources(sources)
{
    for (auto it = m_sources.Begin(); it != m_sources.End();)
    {
        ObjectBase* source = *it;

        if (!source->IsA(LightmapVolume::StaticClass())
            && !source->IsA(EnvProbe::StaticClass())
            && !source->IsA(FogVolume::StaticClass()))
        {
            HYP_LOG(Editor, Error, "GenerateLightmapsEditorTask source is not a LightmapVolume or EnvProbe: \"{}\"", source->InstanceClass()->GetName());
            it = m_sources.Erase(it);

            continue;
        }

        ++it;
    }
}

void GenerateLightmapsEditorTask::Start()
{
    AssertOnThread(g_simThread);

    if (m_sources.Empty())
    {
        HYP_LOG(Editor, Error, "No valid sources provided for GenerateLightmapsEditorTask");

        return;
    }

    HYP_LOG(Editor, Verbose, "Generating lightmaps");

    if (!m_world.IsValid() || !m_scene.IsValid())
    {
        HYP_LOG(Editor, Error, "World or scene not set for GenerateLightmapsEditorTask");

        return;
    }

    BakerSubsystem* lightmapperSubsystem = m_world->GetSubsystem<BakerSubsystem>();

    if (!lightmapperSubsystem)
    {
        lightmapperSubsystem = m_world->AddSubsystem<BakerSubsystem>();
    }

    for (const Handle<ObjectBase>& source : m_sources)
    {
        Task<void> task;

        if (source->IsA<LightmapVolume>())
        {
            task = lightmapperSubsystem->EnqueueBake(StaticCast<LightmapVolume>(source));
        }
        else if (source->IsA<EnvProbe>())
        {
            task = lightmapperSubsystem->EnqueueBake(StaticCast<EnvProbe>(source));
        }
        else if (source->IsA<FogVolume>())
        {
            task = lightmapperSubsystem->EnqueueBake(StaticCast<FogVolume>(source));
        }

        if (task.IsValid())
        {
            m_tasks.PushBack(std::move(task));
        }
    }
}

void GenerateLightmapsEditorTask::Cancel()
{
    if (m_tasks.Any())
    {
        for (Task<void>& task : m_tasks)
        {
            task.Cancel();
        }
    }
}

bool GenerateLightmapsEditorTask::IsCompleted() const
{
    return m_tasks.Empty() || Every(m_tasks, &Task<void>::IsCompleted);
}

void GenerateLightmapsEditorTask::Tick()
{
    AssertOnThread(g_simThread);

    for (auto it = m_tasks.Begin(); it != m_tasks.End();)
    {
        Task<void>& task = *it;

        if (task.IsCompleted())
        {
            // remove task upon completion
            it = m_tasks.Erase(it);
        }
        else
        {
            ++it;
        }
    }
}

#pragma endregion GenerateLightmapsEditorTask

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
        HYP_LOG(Editor, Warning, "Failed to create manipulation widget node for \"{}\"!", InstanceClass()->GetName());

        // Create default node so we don't crash trying to use it
        m_node = MakeHandle<Node>();
        m_node->SetName(NAME_FMT("{}_FallbackGizmoNode", InstanceClass()->GetName()));
    }

    m_node->UnlockTransform();

    m_node->SetNodeFlags(m_node->GetNodeFlags() | NodeFlags::HideInSceneOutline);
}

void EditorGizmoBase::Shutdown()
{
    if (m_node.IsValid())
    {
        // Remove from scene
        m_node->Remove();
    }

    m_focusedNode.Reset();
}

void EditorGizmoBase::SetFocusedNode(const Handle<Node>& focusedNode)
{
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
                axis = static_cast<int>(value);
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

    const Vec4f mouseWorld = camera->TransformScreenToWorld(mouseEvent.relativePos);
    const Vec4f rayDirection = mouseWorld.Normalized();

    const Ray ray { camera->GetWorldTranslation(), rayDirection.GetXYZ() };

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

            project->GetActionStack()->PushAction(MakeHandle<FunctionalEditorAction>(
                text,
                [focusedNode, node = m_node, focusedFinalPosition, focusedOrigin, nodeData = std::move(nodeData)]() -> EditorActionFunctions
                {
                    auto nodeDataPtr = MakeShared<decltype(nodeData)>(std::move(nodeData));

                    return {
                        [focusedNode, node, focusedFinalPosition, focusedOrigin, nodeDataPtr](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
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

                            if (Node* parent = node->FindParentWithName("TranslateGizmo"))
                            {
                                parent->SetWorldTranslation(focusedFinalPosition);
                            }

                            editorSubsystem->SetFocusedNode(focusedNode, true);
                        },
                        [focusedNode, node, focusedOrigin, nodeDataPtr](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
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

    const Vec4f mouseWorld = camera->TransformScreenToWorld(Vec2f(inputMgr->GetVirtualMousePosition()) / Vec2f(camera->GetDimensions()));
    const Vec4f rayDirection = mouseWorld.Normalized();

    const Ray ray { camera->GetWorldTranslation(), rayDirection.GetXYZ() };

    // const Ray rayViewSpace { camera->GetViewMatrix() * ray.position, (camera->GetViewMatrix() * Vec4f(ray.direction, 0.0f)).GetXYZ() };

    // Vec4f mouseView = camera->GetViewMatrix() * mouseWorld;
    // mouseView /= mouseView.w;

    RayHit planeRayHit;

    if (Optional<RayHit> planeRayHitOpt = ray.TestPlane(m_dragData->nodeOrigin, m_dragData->planeNormal))
    {
        planeRayHit = *planeRayHitOpt;
    }
    else
    {
        return true;
    }

    Vec3f translation;

    if (m_dragData->axisDirection == Vec3f::Zero())
    {
        translation = m_dragData->nodeOrigin + (planeRayHit.hitpoint - m_dragData->hitpointOrigin);
    }
    else
    {
        const float t = (planeRayHit.hitpoint - m_dragData->hitpointOrigin).Dot(m_dragData->axisDirection);
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
                axis = static_cast<int>(value);
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

            project->GetActionStack()->PushAction(MakeHandle<FunctionalEditorAction>(
                nodeData.Size() == 1
                    ? HYP_FORMAT("Rotate {}", nodeData[0].first->GetName())
                    : HYP_FORMAT("Rotate {} nodes", nodeData.Size()),
                [focusedNode, finalRotation, originRotation, deltaRotation, nodeData]() -> EditorActionFunctions
                {
                    auto nodeDataPtr = MakeShared<decltype(nodeData)>(std::move(nodeData));

                    return {
                        [focusedNode, deltaRotation, nodeDataPtr](EditorSubsystem* editorSubsystem, EditorProject*)
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

                            editorSubsystem->SetFocusedNode(focusedNode, true);
                        },
                        [focusedNode, nodeDataPtr](EditorSubsystem* editorSubsystem, EditorProject*)
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

    const Vec4f mouseWorld = camera->TransformScreenToWorld(Vec2f(inputMgr->GetVirtualMousePosition()) / Vec2f(camera->GetDimensions()));
    const Vec4f rayDirection = (mouseWorld - Vec4f(camera->GetWorldTranslation(), 1.0f)).Normalized();

    const Ray ray { camera->GetWorldTranslation(), rayDirection.GetXYZ() };

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
                axis = static_cast<int>(value);
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

            project->GetActionStack()->PushAction(MakeHandle<FunctionalEditorAction>(
                nodeData.Size() == 1
                    ? HYP_FORMAT("Scale {}", nodeData[0].first->GetName())
                    : HYP_FORMAT("Scale {} nodes", nodeData.Size()),
                [focusedNode, finalScale, originScale, scaleFactor, nodeData]() -> EditorActionFunctions
                {
                    auto nodeDataPtr = MakeShared<decltype(nodeData)>(std::move(nodeData));

                    return {
                        [focusedNode, scaleFactor, nodeDataPtr](EditorSubsystem* editorSubsystem, EditorProject*)
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

                            editorSubsystem->SetFocusedNode(focusedNode, true);
                        },
                        [focusedNode, nodeDataPtr](EditorSubsystem* editorSubsystem, EditorProject*)
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

    const Vec4f mouseWorld = camera->TransformScreenToWorld(Vec2f(inputMgr->GetVirtualMousePosition()) / Vec2f(camera->GetDimensions()));
    const Vec4f rayDirection = mouseWorld.Normalized();

    const Ray ray { camera->GetWorldTranslation(), rayDirection.GetXYZ() };

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
        Handle<Node> faceNode = m_node->FindChildByName(StringHash(HYP_FORMAT("VolumeFace_{}", i)));

        if (!faceNode.IsValid())
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

    AssertDebug(m_currentBounds.IsValid() && m_currentBounds.IsFinite() && !m_currentBounds.IsZero());

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

    const Vec4f mouseWorld = camera->TransformScreenToWorld(Vec2f(inputMgr->GetVirtualMousePosition()) / Vec2f(camera->GetDimensions()));
    const Vec4f rayDirection = mouseWorld.Normalized();

    const Ray ray { camera->GetWorldTranslation(), rayDirection.GetXYZ() };

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

#pragma region EditorSubsystem Gizmos

EditorManipulationMode EditorSubsystem::GetSelectedManipulationMode() const
{
    AssertOnThread(g_simThread);

    return m_selectedManipulationMode;
}

void EditorSubsystem::SetSelectedManipulationMode(EditorManipulationMode mode)
{
    AssertOnThread(g_simThread);

    SetMeshEditModeEnabled(false);

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

bool EditorSubsystem::IsMeshEditModeEnabled() const
{
    AssertOnThread(g_simThread);

    return m_meshEditModeEnabled;
}

void EditorSubsystem::SetMeshEditModeEnabled(bool enabled)
{
    AssertOnThread(g_simThread);

    if (enabled == m_meshEditModeEnabled)
    {
        return;
    }

    m_meshEditModeEnabled = enabled;

    if (!enabled)
    {
        EndMeshEditDrag();
        SetSelectedMeshEditFace({});
    }
}

MeshEditFaceMode EditorSubsystem::GetMeshEditFaceMode() const
{
    AssertOnThread(g_simThread);

    return m_meshEditFaceMode;
}

void EditorSubsystem::SetMeshEditFaceMode(MeshEditFaceMode faceMode)
{
    AssertOnThread(g_simThread);

    if (faceMode == m_meshEditFaceMode)
    {
        return;
    }

    m_meshEditFaceMode = faceMode;

    SetSelectedMeshEditFace({});
}

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
    const Array<uint32>& triangleVertexIndices)
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
            if (triangleVertexIndices.Contains(candidateVertexIndex))
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

static Array<uint32> FindWeldedVertexIndices(
    Mesh* mesh,
    uint8 lodIndex,
    const Array<uint32>& faceVertexIndices)
{
    auto readScope = mesh->GetReadScope();

    const VertexArrayView vertexView = mesh->GetVertexData(lodIndex);
    const size_t vertexSizeInFloats = vertexView.layoutDesc.VertexSize() / sizeof(float);

    Array<Vec3f> faceVertexPositions;
    faceVertexPositions.Reserve(faceVertexIndices.Size());

    for (uint32 vertexIndex : faceVertexIndices)
    {
        faceVertexPositions.PushBack(ReadMeshVertexPosition(vertexView, vertexSizeInFloats, vertexIndex));
    }

    Array<uint32> affectedVertexIndices = faceVertexIndices;

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
    const Array<uint32>& vertexIndices,
    const Array<Vec3f>& localPositions,
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
        auto writeScope = mesh->GetWriteScope();

        const VertexArrayView vertexView = mesh->GetVertexData(lodIndex);
        const size_t vertexSizeInFloats = vertexView.layoutDesc.VertexSize() / sizeof(float);

        Array<float> mutableVertexData(vertexView.floatData, vertexView.vertexCount * vertexSizeInFloats);

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
        newVertexView.vertexCount = vertexView.vertexCount;
        newVertexView.layoutDesc = vertexView.layoutDesc;

        mesh->SetVertexData(lodIndex, newVertexView);

        if (recomputeDerivedData)
        {
            mesh->CalculateNormals(true);
            mesh->SetAABB(mesh->CalculateAABB());
        }
    }

    if (recomputeDerivedData)
    {
        mesh->BuildBVH();
    }

    mesh->UploadGpuData();
}

static void EnsureUniqueMeshEditTarget(const Handle<Node>& node, MeshComponent* meshComponent)
{
    if (node->GetTag("HYP_MeshEditUniqueMesh"_sh))
    {
        return;
    }

    Mesh* sourceMesh = meshComponent->mesh;

    Handle<Mesh> clonedMesh = MakeHandle<Mesh>();
    clonedMesh->SetName(NAME("MeshEditClone"));

    {
        auto readScope = sourceMesh->GetReadScope();

        const MeshDesc meshDesc = sourceMesh->GetMeshDesc();
        const VertexArrayView vertexData = sourceMesh->GetVertexData(0);
        const Span<const ubyte> indexData = sourceMesh->GetIndexData(0);

        MeshDataView meshData {};
        meshData.vertices[0] = vertexData;
        meshData.indices[0] = ConstByteView(indexData.Data(), indexData.Data() + indexData.Size());

        clonedMesh->SetMeshData(meshDesc, meshData);
    }

    InitObject(clonedMesh);

    clonedMesh->BuildBVH();
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

    RayTestResults results;

    for (const Handle<EditorViewport>& viewport : m_editorViewports)
    {
        viewport->GetView()->TestRay(ray, results, RayTestFlags::TestBVH | RayTestFlags::EditorPick);
    }

    const RayHit* closestHit = nullptr;
    Handle<Node> hitNode;
    MeshComponent* meshComponent = nullptr;

    for (const RayHit& hit : results)
    {
        if (!hit.node || hit.triangleIndex == ~0u)
        {
            continue;
        }

        Handle<Node> candidateNode = MakeStrongRef(hit.node);
        Entity* candidateEntity = DynamicCast<Entity>(candidateNode.Get());

        if (!candidateEntity)
        {
            continue;
        }

        MeshComponent* candidateMeshComponent = candidateEntity->TryGetComponent<MeshComponent>();

        if (!candidateMeshComponent || !candidateMeshComponent->mesh.IsValid())
        {
            continue;
        }

        closestHit = &hit;
        hitNode = std::move(candidateNode);
        meshComponent = candidateMeshComponent;
        break;
    }

    if (!closestHit)
    {
        return false;
    }

    if (ensureUniqueMesh)
    {
        EnsureUniqueMeshEditTarget(hitNode, meshComponent);
    }

    Mesh* mesh = meshComponent->mesh;
    const uint8 lodIndex = 0;

    auto readScope = mesh->GetReadScope();

    const Span<const ubyte> indexData = mesh->GetIndexData(lodIndex);
    const Span<const uint32> indices(reinterpret_cast<const uint32*>(indexData.Data()), indexData.Size() / sizeof(uint32));

    const uint32 triangleIndex = closestHit->triangleIndex;

    Array<uint32> vertexIndices {
        indices[triangleIndex * 3 + 0],
        indices[triangleIndex * 3 + 1],
        indices[triangleIndex * 3 + 2]
    };

    if (m_meshEditFaceMode == MeshEditFaceMode::Quad)
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

    outSelection.node = hitNode.ToWeak();
    outSelection.vertexIndices = std::move(vertexIndices);
    outSelection.lodIndex = lodIndex;

    return true;
}

void EditorSubsystem::SetSelectedMeshEditFace(Optional<MeshEditFaceSelection> selection)
{
    AssertOnThread(g_simThread);

    EndMeshEditDrag();

    m_selectedMeshEditFace = std::move(selection);

    OnMeshEditSelectionChanged();
}

void EditorSubsystem::UpdateHoveredMeshEditFace(const Ray& ray)
{
    AssertOnThread(g_simThread);

    MeshEditFaceSelection hoveredFace;

    if (TryPickMeshEditFace(ray, hoveredFace, /* ensureUniqueMesh */ false))
    {
        m_hoveredMeshEditFace = hoveredFace;
    }
    else
    {
        m_hoveredMeshEditFace.Unset();
    }
}

static void DrawMeshEditFaceHighlight(
    DebugDrawCommandList& debugDrawCommandList,
    const Handle<Node>& node,
    const Array<uint32>& vertexIndices,
    uint8 lodIndex,
    const Color& color)
{
    Entity* entity = DynamicCast<Entity>(node.Get());
    MeshComponent* meshComponent = entity ? entity->TryGetComponent<MeshComponent>() : nullptr;

    if (!meshComponent || !meshComponent->mesh.IsValid())
    {
        return;
    }

    Mesh* mesh = meshComponent->mesh;

    auto readScope = mesh->GetReadScope();

    const VertexArrayView vertexView = mesh->GetVertexData(lodIndex);
    const size_t vertexSizeInFloats = vertexView.layoutDesc.VertexSize() / sizeof(float);

    const Mat4f& worldMatrix = node->GetWorldMatrix();

    Array<Vec3f> worldPositions;
    worldPositions.Reserve(vertexIndices.Size());

    for (uint32 vertexIndex : vertexIndices)
    {
        const Vec3f localPosition = ReadMeshVertexPosition(vertexView, vertexSizeInFloats, vertexIndex);

        Vec4f transformedPosition = worldMatrix.TransformVector(Vec4f(localPosition, 1.0f));
        transformedPosition /= transformedPosition.w;

        worldPositions.PushBack(transformedPosition.GetXYZ());
    }

    debugDrawCommandList.triangle(worldPositions[0], worldPositions[1], worldPositions[2], color);

    if (worldPositions.Size() == 4)
    {
        debugDrawCommandList.triangle(worldPositions[0], worldPositions[2], worldPositions[3], color);
    }
}

void EditorSubsystem::DebugDrawMeshEditSelection(DebugDrawCommandList& debugDrawCommandList)
{
    if (m_hoveredMeshEditFace
        && (!m_selectedMeshEditFace
            || m_hoveredMeshEditFace->node != m_selectedMeshEditFace->node
            || m_hoveredMeshEditFace->vertexIndices != m_selectedMeshEditFace->vertexIndices))
    {
        if (Handle<Node> hoveredNode = m_hoveredMeshEditFace->node.Lock(); hoveredNode.IsValid())
        {
            const Color hoverColor = Color(1.0f, 1.0f, 1.0f, 0.4f);

            DrawMeshEditFaceHighlight(debugDrawCommandList, hoveredNode, m_hoveredMeshEditFace->vertexIndices, m_hoveredMeshEditFace->lodIndex, hoverColor);
        }
    }
    if (!m_selectedMeshEditFace)
    {
        return;
    }

    Handle<Node> node = m_selectedMeshEditFace->node.Lock();

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

    const Mat4f& worldMatrix = node->GetWorldMatrix();
    const uint32 faceVertexCount = uint32(m_selectedMeshEditFace->vertexIndices.Size());

    Array<Vec3f> worldPositions;
    worldPositions.Reserve(faceVertexCount);

    if (m_meshEditDragData)
    {
        for (uint32 i = 0; i < faceVertexCount; i++)
        {
            const Vec3f localPosition = m_meshEditDragData->vertexOriginalPositions[i] + m_meshEditDragData->currentLocalDelta;

            Vec4f transformedPosition = worldMatrix.TransformVector(Vec4f(localPosition, 1.0f));
            transformedPosition /= transformedPosition.w;

            worldPositions.PushBack(transformedPosition.GetXYZ());
        }
    }
    else
    {
        auto readScope = mesh->GetReadScope();

        const VertexArrayView vertexView = mesh->GetVertexData(m_selectedMeshEditFace->lodIndex);
        const size_t vertexSizeInFloats = vertexView.layoutDesc.VertexSize() / sizeof(float);

        for (uint32 vertexIndex : m_selectedMeshEditFace->vertexIndices)
        {
            const Vec3f localPosition = ReadMeshVertexPosition(vertexView, vertexSizeInFloats, vertexIndex);

            Vec4f transformedPosition = worldMatrix.TransformVector(Vec4f(localPosition, 1.0f));
            transformedPosition /= transformedPosition.w;

            worldPositions.PushBack(transformedPosition.GetXYZ());
        }
    }

    const Color highlightColor = Color(1.0f, 0.8f, 0.0f, 1.0f);

    debugDrawCommandList.triangle(worldPositions[0], worldPositions[1], worldPositions[2], highlightColor);

    if (worldPositions.Size() == 4)
    {
        debugDrawCommandList.triangle(worldPositions[0], worldPositions[2], worldPositions[3], highlightColor);
    }
}

void EditorSubsystem::StartMeshEditDrag(const Handle<Camera>& camera, const MouseEvent& mouseEvent)
{
    if (!m_selectedMeshEditFace)
    {
        return;
    }

    Handle<Node> node = m_selectedMeshEditFace->node.Lock();

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
    const uint8 lodIndex = m_selectedMeshEditFace->lodIndex;

    const Array<uint32> affectedVertexIndices = FindWeldedVertexIndices(mesh, lodIndex, m_selectedMeshEditFace->vertexIndices);

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

    // The picked face's own vertices always come first in affectedVertexIndices (see FindWeldedVertexIndices),
    // so this covers exactly the selected face when computing the drag pivot.
    Vec3f centroidLocalPosition = Vec3f::Zero();

    for (uint32 i = 0; i < m_selectedMeshEditFace->vertexIndices.Size(); i++)
    {
        centroidLocalPosition += dragData.vertexOriginalPositions[i];
    }

    centroidLocalPosition /= float(m_selectedMeshEditFace->vertexIndices.Size());

    const Mat4f& worldMatrix = node->GetWorldMatrix();
    Vec4f transformedCentroid = worldMatrix.TransformVector(Vec4f(centroidLocalPosition, 1.0f));
    transformedCentroid /= transformedCentroid.w;

    dragData.faceCentroidWorldOrigin = transformedCentroid.GetXYZ();
    dragData.planeNormal = -camera->GetDirection();
    dragData.currentLocalDelta = Vec3f::Zero();
    dragData.lockedAxis = -1;

    const Vec4f mouseWorld = camera->TransformScreenToWorld(mouseEvent.relativePos);
    const Vec4f rayDirection = mouseWorld.Normalized();
    const Ray ray { camera->GetWorldTranslation(), rayDirection.GetXYZ() };

    if (Optional<RayHit> planeHit = ray.TestPlane(dragData.faceCentroidWorldOrigin, dragData.planeNormal))
    {
        dragData.hitpointOrigin = planeHit->hitpoint;
    }
    else
    {
        dragData.hitpointOrigin = dragData.faceCentroidWorldOrigin;
    }

    m_meshEditDragData = dragData;
}

void EditorSubsystem::UpdateMeshEditDrag(const Handle<Camera>& camera, const MouseEvent& mouseEvent)
{
    if (!m_meshEditDragData || !m_selectedMeshEditFace)
    {
        return;
    }

    Handle<Node> node = m_selectedMeshEditFace->node.Lock();

    if (!node.IsValid())
    {
        return;
    }

    const Vec4f mouseWorld = camera->TransformScreenToWorld(mouseEvent.relativePos);
    const Vec4f rayDirection = mouseWorld.Normalized();
    const Ray ray { camera->GetWorldTranslation(), rayDirection.GetXYZ() };

    Optional<RayHit> planeHit = ray.TestPlane(m_meshEditDragData->faceCentroidWorldOrigin, m_meshEditDragData->planeNormal);

    if (!planeHit)
    {
        return;
    }

    Vec3f worldDelta;

    if (m_meshEditDragData->lockedAxis == -1)
    {
        worldDelta = planeHit->hitpoint - m_meshEditDragData->hitpointOrigin;
    }
    else
    {
        Vec3f axisDirection = Vec3f::Zero();
        axisDirection[m_meshEditDragData->lockedAxis] = 1.0f;

        const float t = (planeHit->hitpoint - m_meshEditDragData->hitpointOrigin).Dot(axisDirection);
        worldDelta = axisDirection * t;
    }

    const Mat4f inverseWorldMatrix = node->GetWorldMatrix().Inverse();

    m_meshEditDragData->currentLocalDelta = inverseWorldMatrix.TransformVector(Vec4f(worldDelta, 0.0f)).GetXYZ();
}

void EditorSubsystem::EndMeshEditDrag()
{
    if (!m_meshEditDragData)
    {
        return;
    }

    if (!m_selectedMeshEditFace)
    {
        m_meshEditDragData.Unset();
        return;
    }

    Handle<Node> node = m_selectedMeshEditFace->node.Lock();

    if (!node.IsValid())
    {
        m_meshEditDragData.Unset();
        return;
    }

    Entity* entity = DynamicCast<Entity>(node.Get());
    MeshComponent* meshComponent = entity ? entity->TryGetComponent<MeshComponent>() : nullptr;

    if (!meshComponent || !meshComponent->mesh.IsValid())
    {
        m_meshEditDragData.Unset();
        return;
    }

    const uint8 lodIndex = m_selectedMeshEditFace->lodIndex;

    if (m_meshEditDragData->currentLocalDelta == Vec3f::Zero())
    {
        m_meshEditDragData.Unset();
        return;
    }

    Array<Vec3f> updatedLocalPositions;
    updatedLocalPositions.Reserve(m_meshEditDragData->vertexOriginalPositions.Size());

    for (const Vec3f& originalPosition : m_meshEditDragData->vertexOriginalPositions)
    {
        updatedLocalPositions.PushBack(originalPosition + m_meshEditDragData->currentLocalDelta);
    }

    ApplyMeshEditVertexPositions(node, lodIndex, m_meshEditDragData->affectedVertexIndices, updatedLocalPositions, /* recomputeDerivedData */ true);

    if (Handle<EditorProject> project = GetCurrentProject(); project.IsValid())
    {
        Array<uint32> vertexIndices = m_meshEditDragData->affectedVertexIndices;
        Array<Vec3f> originalPositions = m_meshEditDragData->vertexOriginalPositions;

        project->GetActionStack()->PushAction(MakeHandle<FunctionalEditorAction>(
            "Move mesh vertices",
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

    m_meshEditDragData.Unset();
}

void EditorSubsystem::SetMeshEditDragLockedAxis(const Handle<Camera>& camera, const KeyboardEvent& keyboardEvent, int axis)
{
    if (!m_meshEditDragData)
    {
        return;
    }

    const int newLockedAxis = (m_meshEditDragData->lockedAxis == axis) ? -1 : axis;
    m_meshEditDragData->lockedAxis = newLockedAxis;

    if (newLockedAxis == -1)
    {
        m_meshEditDragData->planeNormal = -camera->GetDirection();
    }
    else
    {
        Vec3f axisDirection = Vec3f::Zero();
        axisDirection[newLockedAxis] = 1.0f;

        m_meshEditDragData->planeNormal = newLockedAxis == 1
            ? axisDirection.Cross(camera->GetSideVector()).Normalize()
            : axisDirection.Cross(camera->GetUpVector()).Normalize();
    }

    AssertDebug(keyboardEvent.inputManager != nullptr);
    
    // Needs re-anchoring
    const Vec2f screenPosition = Vec2f(keyboardEvent.inputManager->GetVirtualMousePosition()) / Vec2f(camera->GetDimensions());
    const Vec4f mouseWorld = camera->TransformScreenToWorld(screenPosition);
    const Vec4f rayDirection = mouseWorld.Normalized();
    const Ray ray { camera->GetWorldTranslation(), rayDirection.GetXYZ() };

    if (Optional<RayHit> planeHit = ray.TestPlane(m_meshEditDragData->faceCentroidWorldOrigin, m_meshEditDragData->planeNormal))
    {
        m_meshEditDragData->hitpointOrigin = planeHit->hitpoint;
    }
}

#pragma endregion MeshEditMode

void EditorSubsystem::InitializeGizmos()
{
    AssertOnThread(g_simThread);

    for (const Handle<EditorGizmoBase>& widget : m_gizmos)
    {
        widget->SetEditorSubsystem(this);
        widget->SetCurrentProject(m_currentProject);

        InitObject(widget);
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

#pragma endregion EditorSubsystem Gizmos

#pragma region EditorSubsystem

static constexpr bool ShowOnlyActiveScene = true; /// \todo : Make this configurable

#if HYP_EDITOR

EditorSubsystem::EditorSubsystem()
    : m_selectedManipulationMode(EditorManipulationMode::None),
      m_meshEditModeEnabled(false),
      m_meshEditFaceMode(MeshEditFaceMode::Triangle),
      m_editorCameraEnabled(false),
      m_shouldCancelNextClick(false)
{
    m_gizmos.Insert(MakeHandle<NullEditorGizmo>());
    m_gizmos.Insert(MakeHandle<TranslateEditorGizmo>());
    m_gizmos.Insert(MakeHandle<RotateEditorGizmo>());
    m_gizmos.Insert(MakeHandle<ScaleEditorGizmo>());
    m_gizmos.Insert(MakeHandle<VolumeEditorGizmo>());

    m_editorDelegates = new EditorDelegates();

    OnProjectOpened
        .Bind(this, [this](const Handle<EditorProject>& project)
              {
                  HYP_LOG(Editor, Verbose, "Opening project: {}", *project->GetName());

                  g_editorState->GetPickCache().Clear();

                  InitObject(project);
                  InitializeGizmos();

                  g_engineDriver->AddWorld(project->GetWorld());

                  Handle<Scene> activeScene;

                  for (const Handle<Scene>& scene : project->GetWorld()->GetScenes())
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

                  for (const Handle<EditorViewport>& vp : m_editorViewports)
                  {
                      vp->OnAdded(this);
                  }

                  m_delegateHandlers.Add(project->GetWorld()->OnSceneAdded.Bind(project->GetWorld().Get(), [this, projectWeak = project.ToWeak()](World*, const Handle<Scene>& scene)
                                                                                {
                                                                                    Assert(scene != nullptr);
                                                                                    Assert(scene != m_editorScene);

                                                                                    if ((scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) != SceneFlags::FOREGROUND)
                                                                                    {
                                                                                        return;
                                                                                    }

                                                                                    Handle<EditorProject> project = projectWeak.Lock();
                                                                                    Assert(project != nullptr);

                                                                                    // Add scene to all editor views
                                                                                    for (const Handle<EditorViewport>& vp : m_editorViewports)
                                                                                    {
                                                                                        vp->OnSceneAdded(scene);
                                                                                    }

                                                                                    if (!m_activeScene)
                                                                                    {
                                                                                        SetActiveScene(scene);
                                                                                    }
                                                                                }));

                  m_delegateHandlers.Add(project->GetWorld()->OnSceneRemoved.Bind(project->GetWorld().Get(), [this, projectWeak = project.ToWeak()](World*, Scene* scene)
                                                                                  {
                                                                                      Assert(scene != nullptr);
                                                                                      Assert(scene != m_editorScene);

                                                                                      Handle<EditorProject> project = projectWeak.Lock();
                                                                                      Assert(project != nullptr);

                                                                                      scene->OnRootNodeChanged.RemoveAllFromSet(m_delegateHandlers);

                                                                                      // remove from all editor views
                                                                                      for (const Handle<EditorViewport>& vp : m_editorViewports)
                                                                                      {
                                                                                          vp->OnSceneRemoved(scene);
                                                                                      }

                                                                                      // StopWatchingNode(scene->GetRoot());

                                                                                      // GetWorld()->RemoveScene(scene);

                                                                                      // // reinitialize scene selector on scene remove
                                                                                      // InitActiveSceneSelection();
                                                                                  }));

                  // m_delegateHandlers.Add(project->GetGame()->OnGameStateChange.Bind([this](Game*, GameStateMode previousMode, GameStateMode currentMode)
                  //     {
                  //         const bool wasSimulating = previousMode == GameStateMode::SIMULATING
                  //             || previousMode == GameStateMode::PAUSED;
                  //         const bool isSimulating  = currentMode  == GameStateMode::SIMULATING
                  //             || currentMode == GameStateMode::PAUSED;

                  //        if (isSimulating && !wasSimulating)
                  //        {
                  //            OnBeginSimulation();
                  //        }
                  //        else if (!isSimulating && wasSimulating)
                  //        {
                  //            OnEndSimulation();
                  //        }
                  //    }));

                  SetActiveScene(activeScene);
              })
        .Detach();

    OnProjectClosing
        .Bind(this, [this](const Handle<EditorProject>& project)
              {
                  g_editorState->GetPickCache().Clear();

                  g_engineDriver->RemoveWorld(project->GetWorld());

                  // Shutdown to reinitialize gizmos after project is opened
                  ShutdownGizmos();

                  m_focusedNode.Reset();
                  m_selectedNodes.Clear();

                  if (m_highlightNode.IsValid())
                  {
                      m_highlightNode->Remove();
                  }

                  SetActiveScene(Handle<Scene>::Null());

                  for (const Handle<Scene>& scene : project->GetWorld()->GetScenes())
                  {
                      if (!scene.IsValid())
                      {
                          continue;
                      }

                      scene->OnRootNodeChanged.RemoveAllFromSet(m_delegateHandlers);

                      // StopWatchingNode(scene->GetRoot());
                  }

                  for (const Handle<EditorViewport>& vp : m_editorViewports)
                  {
                      vp->OnRemoved(this);
                  }

                  project->GetWorld()->OnSceneAdded.RemoveAllFromSet(m_delegateHandlers);
                  project->GetWorld()->OnSceneRemoved.RemoveAllFromSet(m_delegateHandlers);
                  project->GetGame()->OnGameStateChange.RemoveAllFromSet(m_delegateHandlers);

                  // if (m_contentBrowserDirectoryList && m_contentBrowserDirectoryList->GetDataSource())
                  // {
                  //     m_contentBrowserDirectoryList->GetDataSource()->Clear();
                  // }

                  // // reinitialize scene selector
                  // InitActiveSceneSelection();
              })
        .Detach();

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

void EditorSubsystem::OnAddedToWorld()
{
    if (!GetWorld()->GetSubsystem<UISubsystem>())
    {
        HYP_FAIL("EditorSubsystem requires UISubsystem to be initialized");
    }

    GetWorld()->AddSystemT<EditorSpriteSystem>();

    m_editorScene = MakeHandle<Scene>(NAME("EditorScene"), SceneFlags::FOREGROUND | SceneFlags::EDITOR);
    m_editorScene->SetIsTransient(true);
    GetWorld()->AddScene(m_editorScene);

    InitViewport();

    CreateHighlightNode();

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

    if (EditorSpriteSystem* spriteSystem = GetWorld()->GetSystem<EditorSpriteSystem>())
    {
        GetWorld()->RemoveSystem(spriteSystem);
    }

    GetWorld()->RemoveScene(m_editorScene);

    if (m_currentProject)
    {
        g_editorState->SetCurrentProject(nullptr, /* isSimulationStateChange */ false);

        OnProjectClosing(m_currentProject);

        m_currentProject->Close();
        m_currentProject.Reset();
    }

    m_editorViewports.Clear();
}

void EditorSubsystem::Update(float delta)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    m_editorDelegates->Update();

    DebugDrawCommandList& dbg = DebugDrawer::GetInstance().CreateCommandList();

    DebugDrawMeshEditSelection(dbg);

    // Debug draw probes
    for (Scene* scene : GetCurrentProject()->GetWorld()->GetScenes())
    {
        for (auto [probe, _] : scene->GetEntityManager()->GetEntitySet<EntityType<EnvProbe>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            static constexpr auto ReflectionProbeTypeId = CONSTEXPR_TYPE_ID(ReflectionProbe);
            static constexpr auto SkyProbeTypeId = CONSTEXPR_TYPE_ID(SkyProbe);
            static constexpr auto IrradianceProbeTypeId = CONSTEXPR_TYPE_ID(IrradianceProbe);

            switch (probe->InstanceClass()->GetTypeId().Value())
            {
            case ReflectionProbeTypeId:
                dbg.reflectionProbe(probe->GetWorldTranslation(), 1.0f, static_cast<EnvProbe&>(*probe));
                break;
            case SkyProbeTypeId:
                dbg.reflectionProbe(probe->GetWorldTranslation(), 1.0f, static_cast<EnvProbe&>(*probe));
                break;
            case IrradianceProbeTypeId:
                dbg.ambientProbe(probe->GetWorldTranslation(), 1.0f, static_cast<EnvProbe&>(*probe));
                break;
            default:
                HYP_LOG_ONCE(Editor, Warning, "Unknown probe type class: {}", probe->InstanceClass()->GetName());
                break;
            }
        }
    }

#if 0
    static const Color tetTriangles[] = {
        Color::Red(),
        Color::Green(),
        Color::Blue(),
        Color::Yellow(),
        Color::Cyan(),
        Color::Magenta(),
        Color::White(),
        Color::Black(),
        // some additional custom colors
        Color(1.0f, 0.647f, 0.0f),
        Color(0.5f, 0.0f, 0.5f),
        Color(0.0f, 1.0f, 1.0f),
        Color(0.5f, 0.5f, 0.5f)
    };

    for (Scene* scene : GetCurrentProject()->GetWorld()->GetScenes())
    {
        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<ProbeVolume>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            ProbeVolume* probeVolume = static_cast<ProbeVolume*>(entity);

            const Span<const Tetrahedron> tets = probeVolume->GetTetrahedra();
            const Span<IrradianceProbe* const> probes = probeVolume->GetProbes();

            if (tets.Size() == 0 || probes.Size() == 0)
            {
                continue;
            }

            size_t tetIndex = 0;

            for (const Tetrahedron& tet : tets)
            {
                const Vec3f p0 = probes[tet.probeIndices[0]]->GetWorldTranslation();
                const Vec3f p1 = probes[tet.probeIndices[1]]->GetWorldTranslation();
                const Vec3f p2 = probes[tet.probeIndices[2]]->GetWorldTranslation();
                const Vec3f p3 = probes[tet.probeIndices[3]]->GetWorldTranslation();

                RenderableAttributeSet triAttrs;
                triAttrs.GetMaterialAttributes().bucket = RenderBucket::Debug;
                triAttrs.GetMaterialAttributes().cullFaces = FCM_NONE;

                Color color = tetTriangles[tetIndex++ % HYP_ARRAY_SIZE(tetTriangles)];
                color.SetAlpha(0.5f);

                dbg.triangle(p0, p1, p2, color, triAttrs);
                dbg.triangle(p0, p2, p3, color, triAttrs);
                dbg.triangle(p0, p3, p1, color, triAttrs);
                dbg.triangle(p1, p3, p2, color, triAttrs);
            }
        }
    }
#endif

    if (!m_selectedNodes.Empty())
    {
        for (const Handle<Node>& node : m_selectedNodes)
        {
            if (node.IsValid())
            {
                dbg.box(node->GetWorldBounds().GetCenter(), node->GetWorldBounds().GetExtent() * 0.5f + Vec3f(FLT_EPSILON), Color::Cyan());
            }
        }
    }

    RenderProxyList& pickRpl = g_editorState->GetPickCache().GetRenderProxyList();
    pickRpl.GetMeshes().Advance();

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

void EditorSubsystem::OnSceneAttached(const Handle<Scene>& scene)
{
}

void EditorSubsystem::OnSceneDetached(Scene* scene)
{
}

void EditorSubsystem::CreateHighlightNode()
{
    // m_highlightNode = Handle<Node>(MakeHandle<Node>("Editor_Highlight"));
    // m_highlightNode->SetFlags(m_highlightNode->GetNodeFlags() | NodeFlags::HideInSceneOutline);

    // const Handle<Entity> entity = m_scene->GetEntityManager()->AddEntity();

    // Handle<Mesh> mesh = MeshBuilder::Cube();
    // InitObject(mesh);

    // Handle<Material> material = g_MaterialCache->GetOrCreate(
    //     {
    //         .shaderDefinition = ShaderDefinition {
    //             NAME("Forward"),
    //             ShaderProperties(mesh->GetVertexAttributes())
    //         },
    //         .bucket = RenderBucket::Translucent,
    //         // .flags = MAF_NONE, // temp
    //         .stencilFunction = StencilFunction {
    //             .passOp        = SO_REPLACE,
    //             .failOp        = SO_KEEP,
    //             .depthFailOp  = SO_KEEP,
    //             .compareOp     = SCO_NOT_EQUAL,
    //             .mask           = 0xff,
    //             .value          = 0x1
    //         }
    //     },
    //     {
    //         { MATERIAL_KEY_ALBEDO, Vec4f(1.0f, 1.0f, 0.0f, 1.0f) },
    //         { MATERIAL_KEY_ROUGHNESS, 0.0f },
    //         { MATERIAL_KEY_METALNESS, 0.0f }
    //     }
    // );

    // InitObject(material);

    // m_scene->GetEntityManager()->AddComponent<MeshComponent>(
    //     entity,
    //     MeshComponent {
    //         mesh,
    //         material
    //     }
    // );

    // m_scene->GetEntityManager()->AddComponent<TransformComponent>(
    //     entity,
    //     TransformComponent { }
    // );

    // m_scene->GetEntityManager()->AddComponent<VisibilityStateComponent>(
    //     entity,
    //     VisibilityStateComponent {
    //         VisibilityStateFlags::ALWAYS_VISIBLE
    //     }
    // );

    // m_scene->GetEntityManager()->AddComponent<BoundingBoxComponent>(
    //     entity,
    //     BoundingBoxComponent {
    //         mesh->GetAABB()
    //     }
    // );

    // m_highlightNode->SetEntity(entity);
}

bool EditorSubsystem::StartSimulation()
{
    if (!m_currentProject.IsValid())
    {
        return false;
    }

    SetMeshEditModeEnabled(false);

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

    OpenProject(*loadResult);

    Assert(m_currentProject.IsValid() && m_currentProject->GetWorld() != nullptr);

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

    m_currentProject->GetGame()->StartSimulating();

    return true;
}

bool EditorSubsystem::StopSimulation()
{
    if (m_currentProject.IsValid())
    {
        Game* gameInstance = m_currentProject->GetGame();

        if (m_simulationView.IsValid())
        {
            gameInstance->GetWorld()->RemoveView(m_simulationView);
        }

        gameInstance->StopSimulating();
        gameInstance->Shutdown();

        m_simulationView.Reset();

        Assert(m_preSimulationProject.IsValid());
        OpenProject(m_preSimulationProject);

        m_preSimulationProject.Reset();

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

void EditorSubsystem::InitViewport()
{
    for (const Handle<EditorViewport>& vp : m_editorViewports)
    {
        vp->OnRemoved(this);
    }
    m_editorViewports.Clear();

    UISubsystem* uiSubsystem = GetWorld()->GetSubsystem<UISubsystem>();
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

            if (m_meshEditModeEnabled)
            {
                const Vec4f mouseWorld = activeViewport->GetCamera()->TransformScreenToWorld(event.relativePos);
                const Vec4f rayDirection = mouseWorld.Normalized();

                const Ray ray { activeViewport->GetCamera()->GetWorldTranslation(), rayDirection.GetXYZ() };

                MeshEditFaceSelection faceSelection;

                if (TryPickMeshEditFace(ray, faceSelection, /* ensureUniqueMesh */ true))
                {
                    if (Handle<Node> pickedNode = faceSelection.node.Lock(); pickedNode.IsValid())
                    {
                        SetFocusedNode(pickedNode, true);
                    }

                    SetSelectedMeshEditFace(faceSelection);
                }
                else
                {
                    SetSelectedMeshEditFace({});
                }

                return UIEventHandlerResult::STOP_BUBBLING;
            }

            // if (GetWorld()->GetGameState().IsEditMode())
            {
                if (IsHoveringGizmo())
                {
                    return UIEventHandlerResult::STOP_BUBBLING;
                }

                const Vec4f mouseWorld = activeViewport->GetCamera()->TransformScreenToWorld(event.relativePos);
                const Vec4f rayDirection = mouseWorld.Normalized();

                const Ray ray { activeViewport->GetCamera()->GetWorldTranslation(), rayDirection.GetXYZ() };

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
            }

            return UIEventHandlerResult::OK;
        }));

    backdropPanel->OnMouseLeave.RemoveAllFromSet(m_delegateHandlers);
    m_delegateHandlers.Add(backdropPanel->OnMouseLeave.Bind(
        backdropPanel.Get(),
        [this](const MouseEvent& event)
        {
            if (false) // if (!GetWorld()->GetGameState().IsEditMode())
            {
                return UIEventHandlerResult::OK;
            }

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
            if (false) // if (!GetWorld()->GetGameState().IsEditMode())
            {
                return UIEventHandlerResult::OK;
            }

            // prevent click being triggered on release once mouse has been dragged
            m_shouldCancelNextClick = true;

            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
            }

            if (IsMeshEditDragActive())
            {
                UpdateMeshEditDrag(activeViewport->GetCamera(), event);

                return UIEventHandlerResult::STOP_BUBBLING;
            }

            if (IsHoveringGizmo())
            {
                // If the mouse is currently over a manipulation widget, don't allow camera to handle the event
                Handle<EditorGizmoBase> gizmo = m_hoveredGizmo.Lock();
                Handle<Node> node = m_hoveredGizmoNode.Lock();

                if (!gizmo || !node)
                {
                    HYP_LOG(Editor, Warning, "Failed to lock hovered manipulation widget or node");

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

            if (m_meshEditModeEnabled && !event.mouseButtons[MouseButtonState::LEFT] && !IsMeshEditDragActive())
            {
                const Vec4f mouseWorld = activeViewport->GetCamera()->TransformScreenToWorld(event.relativePos);
                const Vec4f rayDirection = mouseWorld.Normalized();

                const Ray ray { activeViewport->GetCamera()->GetWorldTranslation(), rayDirection.GetXYZ() };

                UpdateHoveredMeshEditFace(ray);

                return UIEventHandlerResult::STOP_BUBBLING;
            }

            // Hover over a manipulation widget when mouse is not down
            if (!event.mouseButtons[MouseButtonState::LEFT]
                //&& GetWorld()->GetGameState().IsEditMode()
                && GetSelectedManipulationMode() != EditorManipulationMode::None)
            {
                // Ray test the widget

                const Vec4f mouseWorld = activeViewport->GetCamera()->TransformScreenToWorld(event.relativePos);
                const Vec4f rayDirection = mouseWorld.Normalized();

                const Ray ray { activeViewport->GetCamera()->GetWorldTranslation(), rayDirection.GetXYZ() };

                RayTestResults results;

                EditorGizmoBase* gizmo = GetSelectedGizmo();
                bool hitGizmo = false;

                if (gizmo && gizmo->GetNode()->TestRay(ray, results, RayTestFlags::TestBVH | RayTestFlags::EditorPick))
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
            if (false) // if (!GetWorld()->GetGameState().IsEditMode())
            {
                return UIEventHandlerResult::OK;
            }

            m_shouldCancelNextClick = false;

            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
            }

            if (m_meshEditModeEnabled && m_selectedMeshEditFace)
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
                    const Vec4f mouseWorld = activeViewport->GetCamera()->TransformScreenToWorld(event.relativePos);
                    const Vec4f rayDirection = mouseWorld.Normalized();

                    const Ray ray { activeViewport->GetCamera()->GetWorldTranslation(), rayDirection.GetXYZ() };

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
            if (false) // if (!GetWorld()->GetGameState().IsEditMode())
            {
                return UIEventHandlerResult::OK;
            }

            m_shouldCancelNextClick = false;

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
                    inputHandler->OnMouseUp(event);
                }
            }

            if (EditorGizmoBase* gizmo = GetSelectedGizmo(); gizmo && gizmo->IsDragging())
            {
                gizmo->OnDragEnd(activeViewport->GetCamera(), event);
            }

            if (IsMeshEditDragActive())
            {
                EndMeshEditDrag();
            }

            return UIEventHandlerResult::OK;
        }));

    backdropPanel->OnKeyDown.RemoveAllFromSet(m_delegateHandlers);
    m_delegateHandlers.Add(backdropPanel->OnKeyDown.Bind(
        backdropPanel.Get(),
        [this](const KeyboardEvent& event)
        {
            if (false) // if (!GetWorld()->GetGameState().IsEditMode())
            {
                return UIEventHandlerResult::OK;
            }

            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
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
            if (false) // if (!GetWorld()->GetGameState().IsEditMode())
            {
                return UIEventHandlerResult::OK;
            }

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
            if (false) // if (!GetWorld()->GetGameState().IsEditMode())
            {
                return UIEventHandlerResult::OK;
            }

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
            if (false) // if (!GetWorld()->GetGameState().IsEditMode())
            {
                return UIEventHandlerResult::OK;
            }

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

void EditorSubsystem::StartWatchingNode(const Handle<Node>& node)
{
    if (!node.IsValid())
    {
        return;
    }

    Assert(node->GetScene() != nullptr);

    UISubsystem* uiSubsystem = GetWorld()->GetSubsystem<UISubsystem>();
    AssertDebug(uiSubsystem != nullptr);

    node->OnChildAdded.RemoveAllFromSet(m_delegateHandlers);
    m_delegateHandlers.Add(node->OnChildAdded.Bind(node.Get(), [this](Node* node, bool isDirect)
                                                   {
                                                       Assert(node != nullptr);

                                                       if (node->GetNodeFlags() & NodeFlags::HideInSceneOutline)
                                                       {
                                                           return;
                                                       }
                                                   }));

    node->OnChildRemoved.RemoveAllFromSet(m_delegateHandlers);
    m_delegateHandlers.Add(node->OnChildRemoved.Bind(node.Get(), [this](Node* node, bool)
                                                     {
                                                         // If the node being removed is the focused node, clear the focused node
                                                         if (node == m_focusedNode.GetUnsafe())
                                                         {
                                                             SetFocusedNode(Handle<Node>::Null(), true);
                                                         }

                                                         // If the node being removed is in the selection, remove it
                                                         if (auto it = m_selectedNodes.FindAs(node->Id()); it != m_selectedNodes.End())
                                                         {
                                                             m_selectedNodes.Erase(it);

                                                             OnSelectionChanged();
                                                         }

                                                         if (!node)
                                                         {
                                                             return;
                                                         }
                                                     }));
}

void EditorSubsystem::StopWatchingNode(const Handle<Node>& node)
{
    if (!node.IsValid())
    {
        return;
    }

    UISubsystem* uiSubsystem = GetWorld()->GetSubsystem<UISubsystem>();
    AssertDebug(uiSubsystem != nullptr);

    Handle<UIListView> listView = DynamicCast<UIListView>(uiSubsystem->GetUIStage()->FindChildUIObject("Outline_ListView"_sh));
    AssertDebug(listView.IsValid());

    // Keep ref alive to node to prevent it from being destroyed while we're removing the watchers
    Handle<Node> nodeCopy = node;

    node->OnChildAdded.RemoveAllFromSet(m_delegateHandlers);
    node->OnChildRemoved.RemoveAllFromSet(m_delegateHandlers);

    m_editorDelegates->RemoveNodeWatcher(NAME("SceneView"), node.Get());
}

void EditorSubsystem::ClearWatchedNodes()
{
    UISubsystem* uiSubsystem = GetWorld()->GetSubsystem<UISubsystem>();
    AssertDebug(uiSubsystem != nullptr);

    Handle<UIListView> listView = DynamicCast<UIListView>(uiSubsystem->GetUIStage()->FindChildUIObject("Outline_ListView"_sh));
    AssertDebug(listView.IsValid());

    if (const Handle<UIDataSourceBase>& dataSource = listView->GetDataSource())
    {
        dataSource->Clear();
    }

    m_editorDelegates->RemoveNodeWatchers("SceneView"_sh);
}

void EditorSubsystem::UpdateWatchedNodes()
{
    ClearWatchedNodes();

    if (GetWorld()->GetGameState().IsSimulating())
    {
        for (const Handle<Scene>& scene : m_currentProject->GetWorld()->GetScenes())
        {
            if ((scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::DETACHED | SceneFlags::EDITOR | SceneFlags::UI)) == SceneFlags::FOREGROUND)
            {
                StartWatchingNode(scene->GetRoot());
            }
        }

        return;
    }

    // if (GetWorld()->GetGameState().IsEditMode())
    {
        if (ShowOnlyActiveScene)
        {
            if (Handle<Scene> activeScene = m_activeScene.Lock(); activeScene.IsValid())
            {
                StartWatchingNode(activeScene->GetRoot());
            }

            return;
        }

        if (m_currentProject.IsValid())
        {
            for (const Handle<Scene>& scene : m_currentProject->GetWorld()->GetScenes())
            {
                StartWatchingNode(scene->GetRoot());
            }

            return;
        }
    }
}

void EditorSubsystem::InitActiveSceneSelection()
{
    UISubsystem* uiSubsystem = GetWorld()->GetSubsystem<UISubsystem>();
    Assert(uiSubsystem != nullptr);

    Handle<UIObject> activeSceneSelection = uiSubsystem->GetUIStage()->FindChildUIObject("SetActiveScene_MenuItem"_sh);
    if (!activeSceneSelection.IsValid())
    {
        HYP_LOG(Editor, Warning, "Failed to find SetActiveScene_MenuItem element; creating a new one");
        return;
    }

    Handle<UIMenuItem> activeSceneMenuItem = DynamicCast<UIMenuItem>(activeSceneSelection);

    if (!activeSceneMenuItem.IsValid())
    {
        HYP_LOG(Editor, Error, "Failed to cast SetActiveScene_MenuItem to UIMenuItem");
        return;
    }

    // activeSceneMenuItem->RemoveAllChildUIObjects();

    if (Handle<Scene> activeScene = m_activeScene.Lock(); activeScene.IsValid())
    {
        activeSceneMenuItem->SetText(HYP_FORMAT("Active Scene: {}", activeScene->GetName()));
    }
    else
    {
        activeSceneMenuItem->SetText("Active Scene: None");
    }

    // OnActiveSceneChanged
    //     .Bind([this, activeSceneMenuItemWeak = activeSceneMenuItem.ToWeak()](const Handle<Scene>& scene)
    //         {
    //             Handle<UIMenuItem> activeSceneMenuItem = activeSceneMenuItemWeak.Lock();
    //             if (!activeSceneMenuItem)
    //             {
    //                 HYP_LOG(Editor, Error, "Failed to lock active scene menu item from weak reference in OnActiveSceneChanged");
    //                 return UIEventHandlerResult::ERR;
    //             }

    //             if (scene.IsValid())
    //             {
    //                 Handle<UIObject> selectedSubItem = activeSceneMenuItem->FindChildUIObject([&scene](UIObject* uiObject)
    //                     {
    //                         if (uiObject->IsA<UIMenuItem>())
    //                         {
    //                             const NodeTag& tag = uiObject->GetNodeTag("Scene");
    //                             HYP_LOG(Editor, Verbose, "Checking scene menu item with tag: {}", tag.ToString());

    //                             if (tag.IsValid())
    //                             {
    //                                 const UUID* uuid = tag.data.TryGet<UUID>();

    //                                 if (uuid && *uuid == scene->GetUUID())
    //                                 {
    //                                     return true;
    //                                 }
    //                             }
    //                         }

    //                         return false;
    //                     });

    //                 if (selectedSubItem.IsValid())
    //                 {
    //                     activeSceneMenuItem->SetSelectedSubItem(selectedSubItem.Cast<UIMenuItem>());
    //                     activeSceneMenuItem->SetText(HYP_FORMAT("Active Scene: {}", scene->GetName()));

    //                     return UIEventHandlerResult::OK;
    //                 }
    //                 else
    //                 {
    //                     HYP_LOG(Editor, Warning, "Failed to find active scene menu item for scene: {}", scene->GetName());
    //                 }
    //             }

    //             activeSceneMenuItem->SetSelectedSubItem(Handle<UIMenuItem>::Null());
    //             activeSceneMenuItem->SetText("Active Scene: None");

    //             return UIEventHandlerResult::OK;
    //         })
    //     .Detach();

    if (!m_currentProject.IsValid())
    {
        return;
    }

    // Build each scene menu item
    for (const Handle<Scene>& scene : m_currentProject->GetWorld()->GetScenes())
    {
        if (!scene.IsValid())
        {
            continue;
        }

        Handle<UIMenuItem> sceneMenuItem = activeSceneMenuItem->CreateUIObject<UIMenuItem>(scene->GetName(), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::FILL }, { 100, UIObjectSize::PIXEL }));
        Assert(sceneMenuItem != nullptr);

        sceneMenuItem->SetText(scene->GetName().LookupString());

        sceneMenuItem->OnClick
            .Bind(sceneMenuItem.Get(), [this, activeSceneMenuItemWeak = activeSceneMenuItem.ToWeak(), sceneMenuItemWeak = sceneMenuItem.ToWeak(), sceneWeak = scene.ToWeak()](const MouseEvent&)
                  {
                      Handle<Scene> scene = sceneWeak.Lock();
                      if (!scene.IsValid())
                      {
                          HYP_LOG(Editor, Error, "Failed to lock scene from weak reference in SetActiveScene");
                          return UIEventHandlerResult::ERR;
                      }

                      SetActiveScene(scene);

                      if (Handle<UIMenuItem> activeSceneMenuItem = activeSceneMenuItemWeak.Lock(); activeSceneMenuItem.IsValid())
                      {
                          if (Handle<UIMenuItem> sceneMenuItem = sceneMenuItemWeak.Lock(); sceneMenuItem.IsValid())
                          {
                              activeSceneMenuItem->SetSelectedSubItem(sceneMenuItem);
                              activeSceneMenuItem->SetText(HYP_FORMAT("Active Scene: {}", scene->GetName()));
                          }
                      }

                      return UIEventHandlerResult::OK;
                  })
            .Detach();

        activeSceneMenuItem->AddChildUIObject(std::move(sceneMenuItem));
    }
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

    if (IsOnThread(g_simThread))
    {
        command->Execute(this);
    }
    else
    {
        GetThreadById(g_simThread)->GetScheduler().Enqueue([this, weakThis = MakeWeakRef(this), command = command]()
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
    sun->SetDirection(Vec3f(-0.2f, 0.8f, 0.2f).Normalize());
    sun->SetColor(Color(Vec4f(1.0f, 0.9f, 0.8f, 1.0f)));
    sun->SetIntensity(50.0f);
    InitObject(sun);

    mainScene->GetRoot()->AddChild(sun);

    // Add primary camera
    Handle<Camera> camera = MakeHandle<Camera>();
    camera->SetDimensions(Vec2i(1920, 1080)); // @TODO Match window size
    camera->SetName(NAME("Camera"));
    camera->SetWorldTranslation(Vec3f(0.0f, 1.0f, -5.0f));
    camera->AddTag<EntityTag::PrimaryCamera>();

    Handle<FirstPersonCameraController> firstPersonController = MakeHandle<FirstPersonCameraController>();
    camera->AddCameraController(firstPersonController);

    InitObject(camera);

    mainScene->GetRoot()->AddChild(camera);

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
        OnProjectClosing(m_currentProject);

        m_currentProject->SetEditorSubsystem(WeakHandle<EditorSubsystem>::Null());
        m_currentProject->Close(/* shutdownWorld */ shutdownWorld);

        m_currentProject.Reset();
    }
}

void EditorSubsystem::OpenProject(const Handle<EditorProject>& project)
{
    AssertOnThread(g_simThread);

    if (project == m_currentProject)
    {
        return;
    }

    CloseProject(/* shutdownWorld*/ true);

    if (!project.IsValid())
    {
        return;
    }

    project->SetEditorSubsystem(MakeWeakRef(this));

    m_currentProject = project;

    Game* gameInstance = m_currentProject->GetGame();
    Assert(gameInstance != nullptr);

    gameInstance->Initialize();

    OnProjectOpened(m_currentProject);

    const bool isSimulationStateChange = m_preSimulationProject.IsValid();
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

        if (!m_meshEditModeEnabled)
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

    m_selectedNodes = Set<Handle<Node>>(nodes.Begin(), nodes.End());

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

    AssertDebug(viewport != nullptr);

    if (!viewport)
    {
        return;
    }

    auto impl = [this, weakThis = MakeWeakRef(this)](const Handle<EditorViewport>& viewport)
    {
        Handle<EditorSubsystem> strongThis = weakThis.Lock();
        if (!strongThis)
        {
            HYP_LOG(Editor, Error, "Failed to lock EditorSubsystem from weak reference in AddViewport");
            return;
        }

        InitObject(viewport);
        Handle<EditorViewport> viewportStrong = MakeStrongRef(viewport);

        viewport->OnAdded(strongThis);
        strongThis->m_editorViewports.PushBack(viewportStrong);

        // active VP is always the first one in the array
        // if size == 1 it's because we just added the first one
        if (strongThis->m_editorViewports.Size() == 1)
        {
            OnActiveViewportChanged(viewportStrong);
        }
    };

    if (IsOnThread(g_simThread))
    {
        impl(viewport);
    }
    /*else
    {
        GetThreadById(g_simThread)->GetScheduler().Enqueue([impl, viewport]()
            {
                impl(viewport);
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);
    }*/
}

void EditorSubsystem::RemoveViewport(EditorViewport* viewport)
{
    AssertOnThread(g_simThread);

    AssertDebug(viewport != nullptr);

    if (!viewport)
    {
        return;
    }

    auto impl = [this, weakThis = MakeWeakRef(this)](EditorViewport* viewport)
    {
        Handle<EditorSubsystem> strongThis = weakThis.Lock();
        if (!strongThis)
        {
            HYP_LOG(Editor, Error, "Failed to lock EditorSubsystem from weak reference in RemoveViewport");
            return;
        }

        auto it = strongThis->m_editorViewports.Find(viewport);
        if (it != strongThis->m_editorViewports.End())
        {
            Handle<EditorViewport> viewportStrong;

            const size_t idx = strongThis->m_editorViewports.IndexOf(it);

            if (idx == 0)
            {
                viewportStrong = MakeStrongRef(*it);
            }

            strongThis->m_editorViewports.Erase(it);

            if (viewportStrong)
            {
                OnActiveViewportChanged(viewportStrong);
            }

            viewport->OnRemoved(strongThis);
        }
    };

    if (IsOnThread(g_simThread))
    {
        impl(viewport);
    }
    /*else
    {
        GetThreadById(g_simThread)->GetScheduler().Enqueue([impl, viewportWeak = MakeWeakRef(viewport)]()
            {
                impl(viewportWeak.GetUnsafe());
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);
    }*/
}

#endif

#pragma endregion EditorSubsystem

} // namespace Hyperion
