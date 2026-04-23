/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <EditorPch.hpp>

#include <editor/EditorSubsystem.hpp>
#include <editor/EditorDelegates.hpp>
#include <editor/EditorCamera.hpp>
#include <editor/EditorTask.hpp>
#include <editor/EditorProject.hpp>
#include <editor/EditorActionStack.hpp>
#include <editor/EditorAction.hpp>
#include <editor/EditorState.hpp>
#include <editor/EditorViewport.hpp>
#include <editor/EditorCommand.hpp>

#include <dotnet/DotNETHost.hpp>

#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/View.hpp>
#include <scene/Light.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/FogVolume.hpp>
#include <scene/EntityManager.hpp>

#include <scene/components/MeshComponent.hpp>
#include <scene/components/VisibilityStateComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/TransformComponent.hpp>

#include <scene/sky/DynamicSkySystem.hpp>

#include <scene/LightmapVolume.hpp>
#include <scene/Volume.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/AssetBatch.hpp>

#include <ui/UISubsystem.hpp>
#include <ui/UIObject.hpp>
#include <ui/UIStage.hpp>
#include <ui/UIImage.hpp>
#include <ui/UIEvent.hpp>
#include <ui/UIListView.hpp>
#include <ui/UIWindow.hpp>
#include <ui/UIGrid.hpp>
#include <ui/UIText.hpp>
#include <ui/UIButton.hpp>
#include <ui/UIMenuBar.hpp>
#include <ui/UIDataSource.hpp>
#include <ui/UITextbox.hpp>

#include <input/InputManager.hpp>
#include <input/Event.hpp>

#include <system/AppContext.hpp>
#include <system/OpenFileDialog.hpp>
#include <system/MessageBox.hpp>

#include <Core/threading/TaskSystem.hpp>

#include <Core/io/ByteWriter.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/MaterialDefinition.hpp>
#include <rendering/MaterialInstance.hpp>

#include <engine/EngineGlobals.hpp>
#include <rendering/Texture.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderProxyList.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/RenderInterface.hpp>

#include <rendering/util/DeletionQueue.hpp>

// temp
#include <baking/BakerSubsystem.hpp>
#include <baking/BakeData.hpp>

// for EnumToString
#include <Core/reflection/Enum.hpp>
#include <Core/reflection/ClassRegistry.hpp>

#include <Core/math/MathUtil.hpp>

#include <scripting/ScriptingService.hpp>

#include <util/MeshBuilder.hpp>

#include <engine/Game.hpp>

#include <engine/EngineDriver.hpp>
#include <rendering/DebugDrawer.hpp>

#include <HyperionEngine.hpp>

#include <EditorSubsystem.generated.inl>

namespace Hyperion {

HYP_DEFINE_LOG_CHANNEL(Editor);

namespace CoreApi {
extern FilePath GetExecutablePath();
} // namespace CoreApi

static ShaderPropertyId s_propUniformScaling = InternShaderProperty(ShaderProperty(NAME("UNIFORM_SCALING")));

#pragma region GenerateLightmapsEditorTask

GenerateLightmapsEditorTask::GenerateLightmapsEditorTask(const Handle<LightmapVolume>& volume)
    : GenerateLightmapsEditorTask(Array<Handle<ObjectBase>> { { ObjCast<ObjectBase>(volume) } })
{
}

GenerateLightmapsEditorTask::GenerateLightmapsEditorTask(const Handle<ReflectionProbe>& probe)
    : GenerateLightmapsEditorTask(Array<Handle<ObjectBase>> { { ObjCast<ObjectBase>(probe) } })
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
            && !source->IsA(ReflectionProbe::StaticClass())
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

        if (source->IsA(LightmapVolume::StaticClass()))
        {
            task = lightmapperSubsystem->EnqueueBake(ObjCast<LightmapVolume>(source));
        }
        else if (source->IsA(ReflectionProbe::StaticClass()))
        {
            task = lightmapperSubsystem->EnqueueBake(ObjCast<ReflectionProbe>(source));
        }
        else if (source->IsA(FogVolume::StaticClass()))
        {
            task = lightmapperSubsystem->EnqueueBake(ObjCast<FogVolume>(source));
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
    m_node->SetIsTransient(true);
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

    Entity* entity = ObjCast<Entity>(node);
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

    int axis = axisTag.data.TryGet<int>(-1);

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
}

void TranslateEditorGizmo::OnDragEnd(const Handle<Camera>& camera, const MouseEvent& mouseEvent)
{
    EditorGizmoBase::OnDragEnd(camera, mouseEvent);

    if (Handle<EditorProject> project = GetCurrentProject(); project.IsValid())
    {
        if (Handle<Node> focusedNode = m_focusedNode.Lock(); focusedNode.IsValid())
        {
            project->GetActionStack()->PushAction(MakeHandle<FunctionalEditorAction>(
                HYP_FORMAT("Translate {}", focusedNode->GetName()),
                [focusedNode, node = m_node, finalPosition = focusedNode->GetWorldTranslation(), origin = m_dragData->nodeOrigin]() -> EditorActionFunctions
                {
                    return {
                        [&](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                        {
                            NodeUnlockTransformScope unlockTransformScope(*focusedNode);
                            focusedNode->SetWorldTranslation(finalPosition);

                            if (Node* parent = node->FindParentWithName("TranslateGizmo"))
                            {
                                parent->SetWorldTranslation(finalPosition);
                            }

                            editorSubsystem->SetFocusedNode(focusedNode, true);
                        },
                        [&](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                        {
                            NodeUnlockTransformScope unlockTransformScope(*focusedNode);
                            focusedNode->SetWorldTranslation(origin);

                            if (Node* parent = node->FindParentWithName("TranslateGizmo"))
                            {
                                parent->SetWorldTranslation(origin);
                            }

                            editorSubsystem->SetFocusedNode(focusedNode, true);
                        }
                    };
                }));
        }
    }

    m_dragData.Unset();
}

bool TranslateEditorGizmo::OnMouseHover(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node)
{
    Entity* entity = ObjCast<Entity>(node);
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
    Entity* entity = ObjCast<Entity>(node);
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

    Entity* entity = ObjCast<Entity>(node);
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

    return true;
}

bool TranslateEditorGizmo::OnKeyPress(const Handle<Camera>& camera, const KeyboardEvent& keyboardEvent, const Handle<Node>& node)
{
    if (!node)
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
        const Bitset& keyStates = camera->GetCameraController()->GetInputHandler()->GetKeyStates();

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

        const Vec3f nodeForwardVector = (invNodeRotation * cameraForwardVector);
        const Vec3f nodeSideVector = (invNodeRotation * cameraSideVector);

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

        moveVec = node->GetWorldRotation() * moveVec;
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

    // Try to load from registry first:
    if (Handle<Node> node = GetCurrentAssetRegistry()->GetAsset<Node>(AssetBuckets::Nodes, "TranslateGizmo"_sh); node.IsValid())
    {
        return node;
    }

    auto result = AssetManager::GetInstance()->Load<Node>("Editor/Models/translate_gizmo.obj");

    if (result.HasValue())
    {
        if (Handle<Node> node = result->Result(); node.IsValid())
        {
            node->SetName(NAME("TranslateGizmo"));
            node->SetWorldScale(2.5f);

            Handle<Node> axisX = node->FindChildByName("Mat_Translate_X"_sh);
            AssertDebug(axisX != nullptr);
            axisX->AddTag(NodeTag(NAME("TransformWidgetAxis"), 0));

            Handle<Node> axisY = node->FindChildByName("Mat_Translate_Y"_sh);
            AssertDebug(axisY != nullptr);
            axisY->AddTag(NodeTag(NAME("TransformWidgetAxis"), 1));

            Handle<Node> axisZ = node->FindChildByName("Mat_Translate_Z"_sh);
            AssertDebug(axisZ != nullptr);
            axisZ->AddTag(NodeTag(NAME("TransformWidgetAxis"), 2));

            Handle<Node> centroid = node->FindChildByName("Mat_Translate_Centroid"_sh);
            AssertDebug(centroid != nullptr);
            centroid->AddTag(NodeTag(NAME("TransformWidgetAxis"), -1));

            for (Node* child : node->GetDescendants())
            {
                if (!child->IsA<Entity>())
                {
                    continue;
                }

                Entity* childEntity = static_cast<Entity*>(child);
                childEntity->SetIsDynamic(true);

                VisibilityStateComponent* visibilityState = childEntity->TryGetComponent<VisibilityStateComponent>();

                if (visibilityState)
                {
                    visibilityState->flags |= VisibilityStateFlags::ALWAYS_VISIBLE;
                }
                else
                {
                    childEntity->AddComponent<VisibilityStateComponent>(VisibilityStateComponent { VisibilityStateFlags::ALWAYS_VISIBLE });
                }

                MeshComponent* meshComponent = childEntity->TryGetComponent<MeshComponent>();

                if (!meshComponent)
                {
                    continue;
                }

                MaterialAttributes materialAttributes;
                MaterialParameters materialParameters;

                if (meshComponent->material.IsValid())
                {
                    materialAttributes = meshComponent->material->GetAttributes();
                    materialParameters = meshComponent->material->GetParameters();
                }

                materialAttributes.bucket = RenderBucket::Debug;

                {
                    Handle<MaterialDefinition> materialDefinition = MakeHandle<MaterialDefinition>(NAME_FMT("{}_Material", child->GetName()), materialAttributes, materialParameters, MaterialTextures {});
                    InitObject(materialDefinition);

                    GetCurrentAssetRegistry()->PutAssetsDeep(materialDefinition);

                    Handle<MaterialInstance> materialInstance = materialDefinition->CreateInstance();
                    materialInstance->SetIsDynamic(true);
                    InitObject(materialInstance);

                    GetCurrentAssetRegistry()->PutAssetsDeep(materialInstance);

                    meshComponent->material = std::move(materialInstance);
                }

                childEntity->SetNeedsRenderProxyUpdate();
                childEntity->Node::AddTag(NodeTag(NAME("TransformWidgetElementColor"), Vec4f(materialParameters.albedo)));
            }

            GetCurrentAssetRegistry()->PutAssetsDeep(node);
            GetCurrentAssetRegistry()->SaveDirtyAssets();

            return node;
        }
    }

    HYP_LOG(Editor, Error, "Failed to load axis arrows: {}", result.GetError().GetMessage());

    return Handle<Node>::Null();
}

#pragma endregion TranslateEditorGizmo

#pragma region RotateEditorGizmo

Handle<Node> RotateEditorGizmo::Load_Internal() const
{
    GlobalContextScope assetRegistryScope { AssetRegistryContext { GetEditorAssetRegistry() } };

    // Try to load from registry first:
    if (Handle<Node> node = GetCurrentAssetRegistry()->GetAsset<Node>(AssetBuckets::Nodes, "RotateGizmo"_sh); node.IsValid())
    {
        return node;
    }

    auto result = AssetManager::GetInstance()->Load<Node>("Editor/Models/rotate_gizmo.obj");

    if (result.HasValue())
    {
        if (Handle<Node> node = result->Result(); node.IsValid())
        {
            node->SetName(NAME("RotateGizmo"));
            node->SetWorldScale(2.5f);

            Handle<Node> axisX = node->FindChildByName("Rotate_X"_sh);
            AssertDebug(axisX != nullptr);
            axisX->AddTag(NodeTag(NAME("TransformWidgetAxis"), 0));

            Handle<Node> axisY = node->FindChildByName("Rotate_Y"_sh);
            AssertDebug(axisY != nullptr);
            axisY->AddTag(NodeTag(NAME("TransformWidgetAxis"), 1));

            Handle<Node> axisZ = node->FindChildByName("Rotate_Z"_sh);
            AssertDebug(axisZ != nullptr);
            axisZ->AddTag(NodeTag(NAME("TransformWidgetAxis"), 2));

            for (Node* child : node->GetDescendants())
            {
                if (!child->IsA<Entity>())
                {
                    continue;
                }

                Entity* childEntity = static_cast<Entity*>(child);
                childEntity->SetIsDynamic(true);

                VisibilityStateComponent* visibilityState = childEntity->TryGetComponent<VisibilityStateComponent>();

                if (visibilityState)
                {
                    visibilityState->flags |= VisibilityStateFlags::ALWAYS_VISIBLE;
                }
                else
                {
                    childEntity->AddComponent<VisibilityStateComponent>(VisibilityStateComponent { VisibilityStateFlags::ALWAYS_VISIBLE });
                }

                MeshComponent* meshComponent = childEntity->TryGetComponent<MeshComponent>();

                if (meshComponent)
                {
                    MaterialAttributes materialAttributes;
                    MaterialParameters materialParameters;

                    if (meshComponent->material.IsValid())
                    {
                        materialAttributes = meshComponent->material->GetAttributes();
                        materialParameters = meshComponent->material->GetParameters();
                    }

                    materialAttributes.bucket = RenderBucket::Debug;

                    {
                        Handle<MaterialDefinition> materialDefinition = MakeHandle<MaterialDefinition>(NAME_FMT("{}_Material", child->GetName()), materialAttributes, materialParameters, MaterialTextures {});
                        InitObject(materialDefinition);
                        
                        GetCurrentAssetRegistry()->PutAssetsDeep(materialDefinition);
                        
                        Handle<MaterialInstance> materialInstance = materialDefinition->CreateInstance();
                        materialInstance->SetIsDynamic(true);
                        
                        GetCurrentAssetRegistry()->PutAssetsDeep(materialInstance);

                        meshComponent->material = std::move(materialInstance);
                    }

                    childEntity->SetNeedsRenderProxyUpdate();
                    childEntity->Node::AddTag(NodeTag(NAME("TransformWidgetElementColor"), Vec4f(materialParameters.albedo)));
                }
            }
            
            GetCurrentAssetRegistry()->PutAssetsDeep(node);
            GetCurrentAssetRegistry()->SaveDirtyAssets();

            return node;
        }
    }

    HYP_LOG(Editor, Error, "Failed to load rotate gizmo: {}", result.GetError().GetMessage());

    return Handle<Node>::Null();
}

void RotateEditorGizmo::OnDragStart(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node, const Vec3f& hitpoint)
{
    EditorGizmoBase::OnDragStart(camera, mouseEvent, node, hitpoint);

    m_dragData.Unset();

    Entity* entity = ObjCast<Entity>(node);
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

    const int axisIndex = axisTag.data.TryGet<int>(-1);

    if (axisIndex < 0)
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
    dragData.axis[axisIndex] = 1.0f;
    dragData.axis = (focusedNode->GetWorldRotation() * dragData.axis).Normalize();

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

            project->GetActionStack()->PushAction(MakeHandle<FunctionalEditorAction>(
                HYP_FORMAT("Rotate {}", focusedNode->GetName()),
                [focusedNode, finalRotation, originRotation]() -> EditorActionFunctions
                {
                    return {
                        [&](EditorSubsystem* editorSubsystem, EditorProject*)
                        {
                            NodeUnlockTransformScope unlockTransformScope(*focusedNode);
                            focusedNode->SetWorldRotation(finalRotation);

                            editorSubsystem->SetFocusedNode(focusedNode, true);
                        },
                        [&](EditorSubsystem* editorSubsystem, EditorProject*)
                        {
                            NodeUnlockTransformScope unlockTransformScope(*focusedNode);
                            focusedNode->SetWorldRotation(originRotation);

                            editorSubsystem->SetFocusedNode(focusedNode, true);
                        }
                    };
                }));
        }
    }

    m_dragData.Unset();
}

bool RotateEditorGizmo::OnMouseHover(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node)
{
    Entity* entity = ObjCast<Entity>(node);
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
    Entity* entity = ObjCast<Entity>(node);
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

    Entity* entity = ObjCast<Entity>(node);
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

    return true;
}

bool RotateEditorGizmo::OnKeyPress(const Handle<Camera>& camera, const KeyboardEvent& keyboardEvent, const Handle<Node>& node)
{
    return false;
}

#pragma endregion RotateEditorGizmo

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

    // Try to load from registry first:
    if (Handle<Node> node = GetCurrentAssetRegistry()->GetAsset<Node>(AssetBuckets::Nodes, "VolumeEditGizmo"_sh); node.IsValid())
    {
        return node;
    }

    const Vec4f volumeColor = Vec4f(0.3f, 0.0f, 0.28f, 0.25f);

    Handle<Node> rootNode = MakeHandle<Node>();
    rootNode->SetName(NAME("VolumeEditGizmo"));

    rootNode->UnlockTransform();
    rootNode->SetNodeFlags(rootNode->GetNodeFlags() | NodeFlags::HideInSceneOutline);
    rootNode->SetIsTransient(true);

    // quad face rotations
    static const Quat4f s_faceRotations[VEF_Max] = {
        Quat4f::AxisAngles(Vec3f::UnitY(), -MathUtil::pi<float> * 0.5f),
        Quat4f::AxisAngles(Vec3f::UnitY(), MathUtil::pi<float> * 0.5f),
        Quat4f::AxisAngles(Vec3f::UnitX(), MathUtil::pi<float> * 0.5f),
        Quat4f::AxisAngles(Vec3f::UnitX(), -MathUtil::pi<float> * 0.5f),
        Quat4f::AxisAngles(Vec3f::UnitY(), MathUtil::pi<float>),
        Quat4f::Identity()
    };

    Handle<Mesh> quadMesh = MeshBuilder::Quad();
    InitObject(quadMesh);

    MaterialAttributes materialAttributes;
    materialAttributes.bucket = RenderBucket::Debug;
    materialAttributes.blendFunction = BlendFunction::Additive();
    materialAttributes.cullFaces = FCM_NONE;
    materialAttributes.flags = MAF_NONE;

    MaterialParameters materialParameters;
    materialParameters.albedo = volumeColor;

    Handle<MaterialDefinition> materialDefinition = MakeHandle<MaterialDefinition>(NAME("VolumeEditMaterial"), materialAttributes, materialParameters, MaterialTextures {});
    InitObject(materialDefinition);
    GetCurrentAssetRegistry()->PutAssetsDeep(materialDefinition);

    Handle<MaterialInstance> materialInstance = materialDefinition->CreateInstance();
    materialInstance->SetIsDynamic(true);
    InitObject(materialInstance);
    GetCurrentAssetRegistry()->PutAssetsDeep(materialInstance);

    for (int i = 0; i < VEF_Max; i++)
    {
        Handle<Entity> faceEntity = MakeHandle<Entity>(NAME_FMT("VolumeFace_{}", i));
        faceEntity->SetIsDynamic(true);
        faceEntity->UnlockTransform();
        faceEntity->SetLocalRotation(s_faceRotations[i]);

        faceEntity->Node::AddTag(NodeTag(NAME("VolumeFaceIndex"), i));

        rootNode->AddChild(faceEntity);

        faceEntity->AddComponent<MeshComponent>(MeshComponent { quadMesh, materialInstance });
        faceEntity->SetLocalBounds(quadMesh->GetAABB());

        VisibilityStateComponent* visibilityState = faceEntity->TryGetComponent<VisibilityStateComponent>();

        if (visibilityState)
        {
            visibilityState->flags |= VisibilityStateFlags::ALWAYS_VISIBLE;
        }
        else
        {
            faceEntity->AddComponent<VisibilityStateComponent>(VisibilityStateComponent { VisibilityStateFlags::ALWAYS_VISIBLE });
        }

        faceEntity->Node::AddTag(NodeTag(NAME("TransformWidgetElementColor"), volumeColor));
    }

    rootNode->SetLocalBounds(BoundingBox(Vec3f(-1.0), Vec3f(1.0f)));
    
    GetCurrentAssetRegistry()->PutAssetsDeep(rootNode);
    GetCurrentAssetRegistry()->SaveDirtyAssets();

    return rootNode;
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

    Entity* entity = ObjCast<Entity>(node);
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
                            [&](EditorSubsystem* editorSubsystem, EditorProject*)
                            {
                                BoundingBox finalBoundsLocal = finalBounds;
                                finalBoundsLocal = focusedNode->GetWorldMatrix().Inverse() * finalBoundsLocal;

                                focusedNode->SetLocalBounds(finalBoundsLocal);
                                
                                editorSubsystem->SetSelectedManipulationMode(manipulationMode);
                                editorSubsystem->SetFocusedNode(focusedNode, true);
                            },
                            [&](EditorSubsystem* editorSubsystem, EditorProject*)
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
    Entity* entity = ObjCast<Entity>(node);
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
    Entity* entity = ObjCast<Entity>(node);
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
    //const BoundingBox newBoundsLocal = focusedNode->GetWorldMatrix().Inverse() * newBounds;
    //focusedNode->SetLocalBounds(newBoundsLocal);

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

    if (mode == m_selectedManipulationMode)
    {
        return;
    }

    if (!m_gizmos.Contains(mode))
    {
        SetSelectedManipulationMode(EditorManipulationMode::NONE);
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

    if (m_selectedManipulationMode != EditorManipulationMode::NONE)
    {
        m_selectedManipulationMode = EditorManipulationMode::NONE;

        OnSelectedGizmoChanged(
            m_gizmos.At(EditorManipulationMode::NONE),
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
    : m_selectedManipulationMode(EditorManipulationMode::NONE),
      m_editorCameraEnabled(false),
      m_shouldCancelNextClick(false)
{
    m_gizmos.Insert(MakeHandle<NullEditorGizmo>());
    m_gizmos.Insert(MakeHandle<TranslateEditorGizmo>());
    m_gizmos.Insert(MakeHandle<RotateEditorGizmo>());
    m_gizmos.Insert(MakeHandle<VolumeEditorGizmo>());

    m_editorDelegates = new EditorDelegates();

    OnProjectOpened
        .Bind([this](const Handle<EditorProject>& project)
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

                m_delegateHandlers.Add(project->GetWorld()->OnSceneAdded.Bind([this, projectWeak = project.ToWeak()](World*, const Handle<Scene>& scene)
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

                m_delegateHandlers.Add(project->GetWorld()->OnSceneRemoved.Bind([this, projectWeak = project.ToWeak()](World*, Scene* scene)
                        {
                            Assert(scene != nullptr);
                            Assert(scene != m_editorScene);

                            Handle<EditorProject> project = projectWeak.Lock();
                            Assert(project != nullptr);

                            m_delegateHandlers.Remove(&scene->OnRootNodeChanged);

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

                //m_delegateHandlers.Add(project->GetGame()->OnGameStateChange.Bind([this](Game*, GameStateMode previousMode, GameStateMode currentMode)
                //    {
                //        const bool wasSimulating = previousMode == GameStateMode::SIMULATING
                //            || previousMode == GameStateMode::PAUSED;
                //        const bool isSimulating  = currentMode  == GameStateMode::SIMULATING
                //            || currentMode == GameStateMode::PAUSED;

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
        .Bind([this](const Handle<EditorProject>& project)
            {
                g_editorState->GetPickCache().Clear();

                g_engineDriver->RemoveWorld(project->GetWorld());

                // Shutdown to reinitialize gizmos after project is opened
                ShutdownGizmos();

                m_focusedNode.Reset();

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

                    m_delegateHandlers.Remove(&scene->OnRootNodeChanged);

                    // StopWatchingNode(scene->GetRoot());
                }

                for (const Handle<EditorViewport>& vp : m_editorViewports)
                {
                    vp->OnRemoved(this);
                }

                m_delegateHandlers.Remove(&project->GetWorld()->OnSceneAdded);
                m_delegateHandlers.Remove(&project->GetWorld()->OnSceneRemoved);
                m_delegateHandlers.Remove(&project->GetGame()->OnGameStateChange);

                // if (m_contentBrowserDirectoryList && m_contentBrowserDirectoryList->GetDataSource())
                // {
                //     m_contentBrowserDirectoryList->GetDataSource()->Clear();
                // }

                // // reinitialize scene selector
                // InitActiveSceneSelection();
            })
        .Detach();

    OnSelectedGizmoChanged
        .Bind([this](EditorGizmoBase* newGizmo, EditorGizmoBase* prevGizmo)
            {
                SetHoveredGizmo(MouseEvent {}, nullptr, Handle<Node>::Null());

                if (prevGizmo && prevGizmo->GetManipulationMode() != EditorManipulationMode::NONE)
                {
                    if (prevGizmo->GetNode().IsValid())
                    {
                        prevGizmo->GetNode()->Remove();
                    }

                    prevGizmo->SetFocusedNode(Handle<Node>::Null());
                }

                if (newGizmo && newGizmo->GetManipulationMode() != EditorManipulationMode::NONE)
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
        g_editorState->SetCurrentProject(nullptr);

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

    GetWorld()->RemoveScene(m_editorScene);

    if (m_currentProject)
    {
        g_editorState->SetCurrentProject(nullptr);

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

    if (m_focusedNode.IsValid())
    {
        if (Handle<Node> focusedNode = m_focusedNode.Lock(); focusedNode.IsValid())
        {
            DebugDrawCommandList& dbg = DebugDrawer::GetInstance().CreateCommandList();

            dbg.box(focusedNode->GetWorldBounds().GetCenter(), focusedNode->GetWorldBounds().GetExtent() * 0.5f + Vec3f(FLT_EPSILON), Color::Cyan());
        }
    }

#if 0
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

    /// \todo : Prioritize based on distance from camera
    for (Mesh* mesh : pickRpl.GetMeshes())
    {
        g_editorState->GetPickCache().PutEntry(mesh);
    }
#endif
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

    // Handle<MaterialInstance> material = g_materialInstanceCache->GetOrCreate(
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

void EditorSubsystem::StartSimulation()
{
    if (m_editorViewports.Empty() || !m_currentProject.IsValid())
    {
        return;
    }

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

        return;
    }

    // Save the current project state as a snapshot to restore from when simulation ends.
    if (Result saveResult = m_currentProject->Save(); saveResult.HasError())
    {
        HYP_LOG(Editor, Error, "Failed to save project snapshot before simulation: {}", saveResult.GetError().GetMessage());

        return;
    }
    
    m_preSimulationProject = m_currentProject;
    m_simulationSnapshotPath = m_preSimulationProject->GetFilePath();

    CloseProject();

    FilePath snapshotPath = std::move(m_simulationSnapshotPath);

    TResult<Handle<EditorProject>> loadResult = EditorProject::Load(snapshotPath);

    if (loadResult.HasError())
    {
        HYP_LOG(Editor, Error, "Failed to load project when starting simulation!! Error was: {}", loadResult.GetError().GetMessage());

        return;
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
}

void EditorSubsystem::StopSimulation()
{
    if (m_currentProject)
    {
        Game* gameInstance = m_currentProject->GetGame();

        if (m_simulationView.IsValid())
        {
            gameInstance->GetWorld()->RemoveView(m_simulationView);
        }
        
        gameInstance->StopSimulating();
        gameInstance->Shutdown();
    }

    m_simulationView.Reset();

    // should be set in StartSimulation, but just in case.
    AssertDebug(m_preSimulationProject.IsValid());

    OpenProject(m_preSimulationProject);

    m_preSimulationProject.Reset();
}

void EditorSubsystem::PauseSimulation()
{
    if (m_currentProject.IsValid())
    {
        m_currentProject->GetGame()->PauseSimulation();
    }
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

    Vec2u viewportSize = MathUtil::Max(Vec2u(uiSubsystem->GetUIStage()->GetActualSize()), Vec2u::One());

    m_delegateHandlers.Remove(&backdropPanel->OnClick);
    m_delegateHandlers.Add(backdropPanel->OnClick.Bind([this](const MouseEvent& event)
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

            if (GetWorld()->GetGameState().IsEditMode())
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
                            SetFocusedNode(MakeStrongRef(hit.node), true);

                            break;
                        }
                    }

                    return UIEventHandlerResult::STOP_BUBBLING;
                }
            }

            return UIEventHandlerResult::OK;
        }));

    m_delegateHandlers.Remove(&backdropPanel->OnMouseLeave);
    m_delegateHandlers.Add(backdropPanel->OnMouseLeave.Bind([this](const MouseEvent& event)
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

            activeViewport->GetCamera()->GetCameraController()->GetInputHandler()->OnMouseLeave(event);

            return UIEventHandlerResult::OK;
        }));

    m_delegateHandlers.Remove(&backdropPanel->OnMouseDrag);
    m_delegateHandlers.Add(backdropPanel->OnMouseDrag.Bind([this, uiStage = uiSubsystem->GetUIStage().Get()](const MouseEvent& event)
        {
            // prevent click being triggered on release once mouse has been dragged
            m_shouldCancelNextClick = true;

            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
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

            if (activeViewport->GetCamera()->GetCameraController()->GetInputHandler()->OnMouseDrag(event))
            {
                return UIEventHandlerResult::STOP_BUBBLING;
            }

            return UIEventHandlerResult::OK;
        }));

    m_delegateHandlers.Remove(&backdropPanel->OnMouseMove);
    m_delegateHandlers.Add(backdropPanel->OnMouseMove.Bind([this, uiStage = uiSubsystem->GetUIStage().Get()](const MouseEvent& event)
        {
            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
            }

            // Hover over a manipulation widget when mouse is not down
            if (!event.mouseButtons[MouseButtonState::LEFT]
                && GetWorld()->GetGameState().IsEditMode()
                && GetSelectedManipulationMode() != EditorManipulationMode::NONE)
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

            if (activeViewport->GetCamera()->GetCameraController()->GetInputHandler()->OnMouseMove(event))
            {
                return UIEventHandlerResult::STOP_BUBBLING;
            }

            return UIEventHandlerResult::OK;
        }));

    m_delegateHandlers.Remove(&backdropPanel->OnMouseDown);
    m_delegateHandlers.Add(backdropPanel->OnMouseDown.Bind([this, uiStageWeak = uiSubsystem->GetUIStage().ToWeak()](const MouseEvent& event)
        {
            m_shouldCancelNextClick = false;

            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
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

            if (activeViewport->GetCamera()->GetCameraController()->GetInputHandler()->OnMouseDown(event))
            {
                return UIEventHandlerResult::STOP_BUBBLING;
            }

            return UIEventHandlerResult::OK;
        }));

    m_delegateHandlers.Remove(&backdropPanel->OnMouseUp);
    m_delegateHandlers.Add(backdropPanel->OnMouseUp.Bind([this](const MouseEvent& event)
        {
            m_shouldCancelNextClick = false;

            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
            }

            activeViewport->GetCamera()->GetCameraController()->GetInputHandler()->OnMouseUp(event);

            if (EditorGizmoBase* gizmo = GetSelectedGizmo(); gizmo && gizmo->IsDragging())
            {
                gizmo->OnDragEnd(activeViewport->GetCamera(), event);
            }

            return UIEventHandlerResult::OK;
        }));

    m_delegateHandlers.Remove(&backdropPanel->OnKeyDown);
    m_delegateHandlers.Add(backdropPanel->OnKeyDown.Bind([this](const KeyboardEvent& event)
        {
            if (!GetWorld()->GetGameState().IsEditMode())
            {
                return UIEventHandlerResult::OK;
            }

            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
            }

            if (activeViewport->GetCamera()->GetCameraController()->GetInputHandler()->OnKeyDown(event))
            {
                return UIEventHandlerResult::STOP_BUBBLING;
            }

            return UIEventHandlerResult::OK;
        }));

    m_delegateHandlers.Remove(&backdropPanel->OnKeyUp);
    m_delegateHandlers.Add(backdropPanel->OnKeyUp.Bind([this](const KeyboardEvent& event)
        {
            if (!GetWorld()->GetGameState().IsEditMode())
            {
                return UIEventHandlerResult::OK;
            }

            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
            }

            if (activeViewport->GetCamera()->GetCameraController()->GetInputHandler()->OnKeyUp(event))
            {
                return UIEventHandlerResult::STOP_BUBBLING;
            }

            return UIEventHandlerResult::OK;
        }));

    m_delegateHandlers.Remove(&backdropPanel->OnGainFocus);
    m_delegateHandlers.Add(backdropPanel->OnGainFocus.Bind([this](const MouseEvent& event)
        {
            m_editorCameraEnabled = true;

            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
            }

            activeViewport->GetCamera()->GetCameraController()->GetInputHandler()->OnGainFocus(event);

            return UIEventHandlerResult::OK;
        }));

    m_delegateHandlers.Remove(&backdropPanel->OnLoseFocus);
    m_delegateHandlers.Add(backdropPanel->OnLoseFocus.Bind([this](const MouseEvent& event)
        {
            m_editorCameraEnabled = false;

            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
            }

            activeViewport->GetCamera()->GetCameraController()->GetInputHandler()->OnLoseFocus(event);

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

    Handle<UIListView> listView = ObjCast<UIListView>(uiSubsystem->GetUIStage()->FindChildUIObject(NAME("Outline_ListView")));
    AssertDebug(listView.IsValid());

    //    m_editorDelegates->AddNodeWatcher(
    //        NAME("SceneView"),
    //        node.Get(),
    //        { Node::StaticClass()->GetProperty(NAME("Name")), 1 },
    //        [this, listViewWeak = listView.ToWeak()](Node* node, const Property* property)
    //        {
    //            // Update name in list view
    //            if (node->GetNodeFlags() & NodeFlags::HideInSceneOutline)
    //            {
    //                return;
    //            }
    //
    //            HYP_LOG(Editor, Verbose, "Node {} property changed : {}", *node->GetName(), *property->GetName());
    //
    //            Handle<UIListView> listView = listViewWeak.Lock();
    //
    //            if (!listView)
    //            {
    //                return;
    //            }
    //
    //            if (UIDataSourceBase* dataSource = listView->GetDataSource())
    //            {
    //                const UIDataSourceElement* dataSourceElement = dataSource->Get(node->GetUUID());
    //                Assert(dataSourceElement != nullptr);
    //
    //                dataSource->ForceUpdate(node->GetUUID());
    //            }
    //        });

    m_delegateHandlers.Remove(&node->OnChildAdded);
    m_delegateHandlers.Add(node->OnChildAdded.Bind([this, listViewWeak = listView.ToWeak()](Node* node, bool isDirect)
        {
            Assert(node != nullptr);

            if (node->GetNodeFlags() & NodeFlags::HideInSceneOutline)
            {
                return;
            }

            Handle<UIListView> listView = listViewWeak.Lock();
        }));

    m_delegateHandlers.Remove(&node->OnChildRemoved);
    m_delegateHandlers.Add(node->OnChildRemoved.Bind([this, listViewWeak = listView.ToWeak()](Node* node, bool)
        {
            // If the node being removed is the focused node, clear the focused node
            if (node == m_focusedNode.GetUnsafe())
            {
                SetFocusedNode(Handle<Node>::Null(), true);
            }

            if (!node)
            {
                return;
            }

            Handle<UIListView> listView = listViewWeak.Lock();

            if (!listView)
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

    Handle<UIListView> listView = ObjCast<UIListView>(uiSubsystem->GetUIStage()->FindChildUIObject(NAME("Outline_ListView")));
    AssertDebug(listView.IsValid());


    // Keep ref alive to node to prevent it from being destroyed while we're removing the watchers
    Handle<Node> nodeCopy = node;

    m_delegateHandlers.Remove(&node->OnChildAdded);
    m_delegateHandlers.Remove(&node->OnChildRemoved);

    m_editorDelegates->RemoveNodeWatcher(NAME("SceneView"), node.Get());
}

void EditorSubsystem::ClearWatchedNodes()
{
    UISubsystem* uiSubsystem = GetWorld()->GetSubsystem<UISubsystem>();
    AssertDebug(uiSubsystem != nullptr);

    Handle<UIListView> listView = ObjCast<UIListView>(uiSubsystem->GetUIStage()->FindChildUIObject(NAME("Outline_ListView")));
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

    if (GetWorld()->GetGameState().IsEditMode())
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

    Handle<UIObject> activeSceneSelection = uiSubsystem->GetUIStage()->FindChildUIObject(NAME("SetActiveScene_MenuItem"));
    if (!activeSceneSelection.IsValid())
    {
        HYP_LOG(Editor, Warning, "Failed to find SetActiveScene_MenuItem element; creating a new one");
        return;
    }

    Handle<UIMenuItem> activeSceneMenuItem = ObjCast<UIMenuItem>(activeSceneSelection);

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
            .Bind([this, activeSceneMenuItemWeak = activeSceneMenuItem.ToWeak(), sceneMenuItemWeak = sceneMenuItem.ToWeak(), sceneWeak = scene.ToWeak()](const MouseEvent&)
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
        HYP_LOG(Editor, Error, "Invalid command class: {}", name);
        return false;
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
    sun->SetIntensity(40.0f);
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

void EditorSubsystem::CloseProject()
{
    AssertOnThread(g_simThread);

    if (m_currentProject)
    {
        OnProjectClosing(m_currentProject);

        m_currentProject->SetEditorSubsystem(WeakHandle<EditorSubsystem>::Null());
        m_currentProject->Close();

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

    CloseProject();

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

    g_editorState->SetCurrentProject(m_currentProject);
}

void EditorSubsystem::ShowImportContentDialog()
{
    ShowOpenFileDialog(
        "Select the file(s) to import into the project",
        GetDataDirectory(),
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
            if (Entity* entity = ObjCast<Entity>(focusedNode))
            {
                entity->AddTag<EntityTag::FocusedInEditor>();
            }
        }

        HYP_LOG(Editor, Verbose, "Set focused node: {}\t{}\t is static ? {}", focusedNode->GetName(), focusedNode->GetWorldTranslation(),
            focusedNode->IsStatic());

        /// \todo watch for transform changes and update the highlight node

        // m_scene->GetRoot()->AddChild(m_highlightNode);
        // m_highlightNode->SetWorldScale(m_focusedNode->GetWorldBounds().GetExtent() * 0.5f);
        // m_highlightNode->SetWorldTranslation(m_focusedNode->GetWorldTranslation());

        // HYP_LOG(Editor, Verbose, "Set focused node: {}\t{}", m_focusedNode->GetName(), m_focusedNode->GetWorldTranslation());
        // HYP_LOG(Editor, Verbose, "Set highlight node translation: {}", m_highlightNode->GetWorldTranslation());

        if (focusedNode->IsA(VolumeBase::StaticClass()))
            //|| (focusedNode->IsA(Light::StaticClass()) && !focusedNode->IsA(DirectionalLight::StaticClass())))
        {
            SetSelectedManipulationMode(EditorManipulationMode::VOLUME_EDIT);
        }
        else if (GetSelectedManipulationMode() == EditorManipulationMode::NONE
            || GetSelectedManipulationMode() == EditorManipulationMode::VOLUME_EDIT)
        {
            SetSelectedManipulationMode(EditorManipulationMode::TRANSLATE);
        }

        EditorGizmoBase* gizmo = GetSelectedGizmo();

        if (gizmo)
        {
            gizmo->SetFocusedNode(focusedNode);
        }
    }

    if (previousFocusedNode != nullptr)
    {
        if (Entity* entity = ObjCast<Entity>(previousFocusedNode))
        {
            entity->RemoveTag<EntityTag::FocusedInEditor>();
        }
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
