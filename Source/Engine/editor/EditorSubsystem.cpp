/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

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

#include <editor/ui/debug/EditorDebugOverlay.hpp>

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

#include <Core/threading/TaskSystem.hpp>

#include <Core/io/ByteWriter.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Texture.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderProxyList.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/RenderInterface.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <ui/font/FontAtlas.hpp>

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
    HYP_SCOPE;
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
    HYP_SCOPE;
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

    m_node->SetNodeFlags(
        m_node->GetNodeFlags()
        | NodeFlags::HIDE_IN_SCENE_OUTLINE // don't display transform widget in the outline
        | NodeFlags::TRANSIENT             // should not ever be serialized to disk
    );
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

    const Ray ray { camera->GetTranslation(), rayDirection.GetXYZ() };

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

    // Commit editor transaction
    if (Handle<EditorProject> project = GetCurrentProject(); project.IsValid())
    {
        if (Handle<Node> focusedNode = m_focusedNode.Lock(); focusedNode.IsValid())
        {
            project->GetActionStack()->Push(MakeHandle<FunctionalEditorAction>(
                NAME("Translate"),
                [manipulationMode = GetManipulationMode(), focusedNode, node = m_node, finalPosition = focusedNode->GetWorldTranslation(), origin = m_dragData->nodeOrigin]() -> EditorActionFunctions
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

                            editorSubsystem->SetSelectedManipulationMode(manipulationMode);

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

                            editorSubsystem->SetSelectedManipulationMode(manipulationMode);

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

    meshComponent->material->SetParameter(
        MATERIAL_KEY_ALBEDO,
        Vec4f(1.0f, 1.0f, 0.0, 1.0));

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
        meshComponent->material->SetParameter(
            MATERIAL_KEY_ALBEDO,
            tag.data.TryGet<Vec4f>(Vec4f::Zero()));
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

    const Ray ray { camera->GetTranslation(), rayDirection.GetXYZ() };

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

        const Quaternion invNodeRotation = node->GetWorldRotation().Inverse();

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
                    materialAttributes = meshComponent->material->GetRenderAttributes();
                    materialParameters = meshComponent->material->GetParameters();
                }
                else
                {
                    materialParameters = Material::DefaultParameters();
                }

                materialAttributes.bucket = RB_DEBUG;

                meshComponent->material = MaterialCache::GetInstance()->CreateMaterial(materialAttributes, materialParameters);
                meshComponent->material->SetIsDynamic(true);

                childEntity->SetNeedsRenderProxyUpdate();
                childEntity->Node::AddTag(NodeTag(NAME("TransformWidgetElementColor"), Vec4f(meshComponent->material->GetParameter(MATERIAL_KEY_ALBEDO))));
            }

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
    auto result = AssetManager::GetInstance()->Load<Node>("Editor/Models/rotate_gizmo.obj");

    if (result.HasValue())
    {
        if (Handle<Node> node = result->Result(); node.IsValid())
        {
            node->SetName(NAME("RotateWidget"));

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
                        materialAttributes = meshComponent->material->GetRenderAttributes();
                        materialParameters = meshComponent->material->GetParameters();
                    }
                    else
                    {
                        materialParameters = Material::DefaultParameters();
                    }

                    materialAttributes.bucket = RB_DEBUG;

                    meshComponent->material = MaterialCache::GetInstance()->CreateMaterial(materialAttributes, materialParameters);
                    meshComponent->material->SetIsDynamic(true);

                    childEntity->SetNeedsRenderProxyUpdate();
                    childEntity->Node::AddTag(NodeTag(NAME("TransformWidgetElementColor"), Vec4f(meshComponent->material->GetParameter(MATERIAL_KEY_ALBEDO))));
                }
            }

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
            const Quaternion finalRotation = m_dragData->currentRotation;
            const Quaternion originRotation = m_dragData->startRotation;

            project->GetActionStack()->Push(MakeHandle<FunctionalEditorAction>(
                NAME("Rotate"),
                [manipulationMode = GetManipulationMode(), focusedNode, finalRotation, originRotation]() -> EditorActionFunctions
                {
                    return {
                        [&](EditorSubsystem* editorSubsystem, EditorProject*)
                        {
                            NodeUnlockTransformScope unlockTransformScope(*focusedNode);
                            focusedNode->SetWorldRotation(finalRotation);

                            editorSubsystem->SetSelectedManipulationMode(manipulationMode);
                            editorSubsystem->SetFocusedNode(focusedNode, true);
                        },
                        [&](EditorSubsystem* editorSubsystem, EditorProject*)
                        {
                            NodeUnlockTransformScope unlockTransformScope(*focusedNode);
                            focusedNode->SetWorldRotation(originRotation);

                            editorSubsystem->SetSelectedManipulationMode(manipulationMode);
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

    meshComponent->material->SetParameter(
        MATERIAL_KEY_ALBEDO,
        Vec4f(1.0f, 1.0f, 0.0f, 1.0f));

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
        meshComponent->material->SetParameter(
            MATERIAL_KEY_ALBEDO,
            tag.data.TryGet<Vec4f>(Vec4f::Zero()));
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
    const Vec4f rayDirection = (mouseWorld - Vec4f(camera->GetTranslation(), 1.0f)).Normalized();

    const Ray ray { camera->GetTranslation(), rayDirection.GetXYZ() };

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

    Quaternion deltaRotation = Quaternion::AxisAngles(m_dragData->axis, angle).Inverse();
    Quaternion newRotation = deltaRotation * m_dragData->startRotation;

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

#ifdef HYP_EDITOR

EditorSubsystem::EditorSubsystem()
    : m_selectedManipulationMode(EditorManipulationMode::NONE),
      m_editorCameraEnabled(false),
      m_shouldCancelNextClick(false)
{
    m_gizmos.Insert(MakeHandle<NullEditorGizmo>());
    m_gizmos.Insert(MakeHandle<TranslateEditorGizmo>());
    m_gizmos.Insert(MakeHandle<RotateEditorGizmo>());

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

                m_delegateHandlers.Add(
                    project->GetWorld()->OnSceneAdded.Bind([this, projectWeak = project.ToWeak()](World*, const Handle<Scene>& scene)
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

                m_delegateHandlers.Add(
                    project->GetWorld()->OnSceneRemoved.Bind([this, projectWeak = project.ToWeak()](World*, Scene* scene)
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

                m_delegateHandlers.Remove("OnPackageAdded"_sh);
                m_delegateHandlers.Remove("OnPackageRemoved"_sh);

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

                m_delegateHandlers.Remove("SetBuildBVHFlag"_sh);
                m_delegateHandlers.Remove("OnPackageAdded"_sh);
                m_delegateHandlers.Remove("OnPackageRemoved"_sh);

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
    HYP_SCOPE;

    if (!GetWorld()->GetSubsystem<UISubsystem>())
    {
        HYP_FAIL("EditorSubsystem requires UISubsystem to be initialized");
    }

    m_editorScene = MakeHandle<Scene>(NAME("EditorScene"), SceneFlags::FOREGROUND | SceneFlags::EDITOR);
    GetWorld()->AddScene(m_editorScene);

    LoadEditorUIDefinitions();

    // InitContentBrowser();
    InitViewport();
    // InitSceneOutline();
    // InitActiveSceneSelection();

    // InitDetailView();

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
    HYP_SCOPE;

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

    UpdateCamera(delta);

    UpdateDebugOverlays(delta);

    if (m_focusedNode.IsValid())
    {
        if (Handle<Node> focusedNode = m_focusedNode.Lock(); focusedNode.IsValid())
        {
            DebugDrawCommandList& dbg = DebugDrawer::GetInstance().CreateCommandList();

            dbg.box(focusedNode->GetWorldTranslation(), focusedNode->GetWorldBounds().GetExtent() * 0.5f + Vec3f(FLT_EPSILON), Color::Cyan());
        }
        //        g_engineDriver->GetDebugDrawer()->box(m_focusedNode->GetWorldTranslation(), m_focusedNode->GetWorldBounds().GetExtent(), Color(1.0f), RenderableAttributeSet(
        //            MeshAttributes {
        //                .vertexAttributes = StaticMeshVertexAttributes
        //            },
        //            MaterialAttributes {
        //                .bucket             = RB_TRANSLUCENT,
        //                .fillMode          = FM_FILL,
        //                .blendFunction     = BlendFunction::None(),
        //                .flags              = MAF_DEPTH_TEST,
        //                .stencilFunction   = StencilFunction {
        //                    .passOp        = SO_REPLACE,
        //                    .failOp        = SO_REPLACE,
        //                    .depthFailOp  = SO_REPLACE,
        //                    .compareOp     = SCO_NEVER,
        //                    .mask           = 0xFF,
        //                    .value          = 0x1
        //                }
        //            }
        //        ));
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

    /// \todo : Prioritize based on distance from camera
    for (Mesh* mesh : pickRpl.GetMeshes())
    {
        g_editorState->GetPickCache().PutEntry(mesh);
    }
}

void EditorSubsystem::OnSceneAttached(const Handle<Scene>& scene)
{
    HYP_SCOPE;
}

void EditorSubsystem::OnSceneDetached(Scene* scene)
{
    HYP_SCOPE;
}

void EditorSubsystem::LoadEditorUIDefinitions()
{
    UISubsystem* uiSubsystem = GetWorld()->GetSubsystem<UISubsystem>();
    Assert(uiSubsystem != nullptr);

    TResult<Handle<FontAtlas>> fontAtlasResult = CreateFontAtlas();

    if (fontAtlasResult.HasError())
    {
        HYP_FAIL("Failed to create font atlas for editor UI: {}", fontAtlasResult.GetError().GetMessage());
    }

    uiSubsystem->GetUIStage()->SetDefaultFontAtlas(*fontAtlasResult);
}

void EditorSubsystem::CreateHighlightNode()
{
    // m_highlightNode = Handle<Node>(MakeHandle<Node>("Editor_Highlight"));
    // m_highlightNode->SetFlags(m_highlightNode->GetNodeFlags() | NodeFlags::HIDE_IN_SCENE_OUTLINE);

    // const Handle<Entity> entity = m_scene->GetEntityManager()->AddEntity();

    // Handle<Mesh> mesh = MeshBuilder::Cube();
    // InitObject(mesh);

    // Handle<Material> material = g_materialCache->GetOrCreate(
    //     {
    //         .shaderDefinition = ShaderDefinition {
    //             NAME("Forward"),
    //             ShaderProperties(mesh->GetVertexAttributes())
    //         },
    //         .bucket = RB_TRANSLUCENT,
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

                const Ray ray { activeViewport->GetCamera()->GetTranslation(), rayDirection.GetXYZ() };

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

                const Ray ray { activeViewport->GetCamera()->GetTranslation(), rayDirection.GetXYZ() };

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

                    const Ray ray { activeViewport->GetCamera()->GetTranslation(), rayDirection.GetXYZ() };

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

    InitDebugOverlays();
    InitGizmoSelection();
}

void EditorSubsystem::InitSceneOutline()
{
    UISubsystem* uiSubsystem = GetWorld()->GetSubsystem<UISubsystem>();
    AssertDebug(uiSubsystem != nullptr);

    Handle<UIListView> listView = ObjCast<UIListView>(uiSubsystem->GetUIStage()->FindChildUIObject(NAME("Outline_ListView")));
    AssertDebug(listView.IsValid());

    listView->SetInnerSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    listView->SetDataSource(MakeHandle<UIDataSource>(TypeWrapper<WeakHandle<Node>> {}));

    if (ShowOnlyActiveScene)
    {
        OnActiveSceneChanged
            .Bind([this](const Handle<Scene>& activeScene)
                {
                    UpdateWatchedNodes();
                })
            .Detach();
    }

    m_delegateHandlers.Remove(&listView->OnSelectedItemChange);
    m_delegateHandlers.Add(listView->OnSelectedItemChange.Bind([this, listViewWeak = listView.ToWeak()](UIListViewItem* listViewItem)
        {
            Handle<UIListView> listView = listViewWeak.Lock();

            if (!listView)
            {
                return UIEventHandlerResult::ERR;
            }

            if (!listViewItem)
            {
                SetFocusedNode(Handle<Node>::Null(), false);

                return UIEventHandlerResult::OK;
            }

            const UUID dataSourceElementUuid = listViewItem->GetDataSourceElementUUID();

            if (dataSourceElementUuid == UUID::Invalid())
            {
                return UIEventHandlerResult::ERR;
            }

            if (!listView->GetDataSource())
            {
                return UIEventHandlerResult::ERR;
            }

            const UIDataSourceElement* dataSourceElementValue = listView->GetDataSource()->Get(dataSourceElementUuid);

            if (!dataSourceElementValue)
            {
                return UIEventHandlerResult::ERR;
            }

            const WeakHandle<Node>& nodeWeak = dataSourceElementValue->GetValue().Get<WeakHandle<Node>>();
            Handle<Node> node = nodeWeak.Lock();

            SetFocusedNode(node, false);

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
    //            if (node->GetNodeFlags() & NodeFlags::HIDE_IN_SCENE_OUTLINE)
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

            if (node->GetNodeFlags() & NodeFlags::HIDE_IN_SCENE_OUTLINE)
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
    HYP_SCOPE;

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

void EditorSubsystem::InitDetailView()
{
    UISubsystem* uiSubsystem = GetWorld()->GetSubsystem<UISubsystem>();
    AssertDebug(uiSubsystem != nullptr);

    Handle<UIListView> detailsListView = ObjCast<UIListView>(uiSubsystem->GetUIStage()->FindChildUIObject(NAME("Detail_View_ListView")));
    AssertDebug(detailsListView.IsValid());

    Handle<UIListView> outlineListView = ObjCast<UIListView>(uiSubsystem->GetUIStage()->FindChildUIObject(NAME("Outline_ListView")));
    AssertDebug(outlineListView.IsValid());

    detailsListView->SetInnerSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

    m_editorDelegates->RemoveNodeWatchers("DetailView"_sh);

    m_delegateHandlers.Remove(&OnFocusedNodeChanged);
    m_delegateHandlers.Add(OnFocusedNodeChanged.Bind([this, cls = Node::StaticClass(), detailsListViewWeak = detailsListView.ToWeak(), outlineListViewWeak = outlineListView.ToWeak()](const Handle<Node>& node, const Handle<Node>& previousNode, bool shouldSelectInOutline)
        {
            m_editorDelegates->RemoveNodeWatchers("DetailView"_sh);

            Handle<UIListView> detailsListView = detailsListViewWeak.Lock();

            if (!detailsListView)
            {
                return;
            }

            if (!node.IsValid())
            {
                detailsListView->SetDataSource(nullptr);

                return;
            }

            detailsListView->SetDataSource(MakeHandle<UIDataSource>(TypeWrapper<EditorNodePropertyRef> {}));

            UIDataSourceBase* dataSource = detailsListView->GetDataSource();

            Array<Pair<Property*, int>> propertiesWithSortOrder;
            Array<Property*> propertiesWithoutSortOrder;

            for (auto it = cls->GetMembers(MemberType::Property).Begin(); it != cls->GetMembers(MemberType::Property).End(); ++it)
            {
                if (Property* property = static_cast<Property*>(&*it))
                {
                    if (!property->GetAttribute(Attributes::g_attrEditor))
                    {
                        continue;
                    }

                    if (!property->CanGet())
                    {
                        continue;
                    }

                    if (const ClassAttributeValue& attr = property->GetAttribute(Attributes::g_attrEditOrder); attr.IsValid())
                    {
                        propertiesWithSortOrder.EmplaceBack(property, attr.GetInt());

                        continue;
                    }

                    propertiesWithoutSortOrder.PushBack(property);
                }
                else
                {
                    HYP_UNREACHABLE();
                }
            }

            // sort properties with sort order
            std::sort(
                propertiesWithSortOrder.Begin(),
                propertiesWithSortOrder.End(),
                [](const Pair<Property*, int>& a, const Pair<Property*, int>& b)
                {
                    return a.second < b.second;
                });

            Array<Property*> allProperties;
            allProperties.Reserve(propertiesWithSortOrder.Size() + propertiesWithoutSortOrder.Size());

            for (const Pair<Property*, int>& pair : propertiesWithSortOrder)
            {
                allProperties.PushBack(pair.first);
            }

            for (Property* property : propertiesWithoutSortOrder)
            {
                allProperties.PushBack(property);
            }

            for (Property* property : allProperties)
            {
                EditorNodePropertyRef nodePropertyRef;
                nodePropertyRef.node = node.ToWeak();
                nodePropertyRef.property = property;

                if (const ClassAttributeValue& attr = property->GetAttribute(Attributes::g_attrLabel))
                {
                    nodePropertyRef.title = attr.GetString();
                }
                else
                {
                    nodePropertyRef.title = *property->GetName();
                }

                if (const ClassAttributeValue& attr = property->GetAttribute(Attributes::g_attrDescription))
                {
                    nodePropertyRef.description = attr.GetString();
                }

                dataSource->Push(UUID(), BoxedValue(std::move(nodePropertyRef)));
            }

            m_editorDelegates->AddNodeWatcher(
                NAME("DetailView"),
                node,
                {},
                Proc<void(Node*, const Property*)> {
                    [this, cls = Node::StaticClass(), detailsListViewWeak](Node* node, const Property* property)
                    {
                        // Update name in list view

                        Handle<UIListView> detailsListView = detailsListViewWeak.Lock();

                        if (!detailsListView)
                        {
                            HYP_LOG(Editor, Error, "Failed to get strong reference to list view!");

                            return;
                        }

                        if (UIDataSourceBase* dataSource = detailsListView->GetDataSource())
                        {
                            UIDataSourceElement* dataSourceElement = dataSource->FindWithPredicate([node, property](const UIDataSourceElement* item)
                                {
                                    return item->GetValue().Get<EditorNodePropertyRef>().property == property;
                                });

                            if (!dataSourceElement)
                            {
                                return;
                            }

                            dataSource->ForceUpdate(dataSourceElement->GetUUID());
                        }
                    } });
        }));
}

void EditorSubsystem::InitDebugOverlays()
{
    HYP_SCOPE;

    UISubsystem* uiSubsystem = GetWorld()->GetSubsystem<UISubsystem>();
    Assert(uiSubsystem != nullptr);

    static constexpr UIObjectAlignment Aligments[4] = {
        UIObjectAlignment::TOP_LEFT,
        UIObjectAlignment::BOTTOM_LEFT,
        UIObjectAlignment::TOP_RIGHT,
        UIObjectAlignment::BOTTOM_RIGHT
    };

    for (int i = 0; i < 4; i++)
    {
        Handle<UIObject>& debugOverlayContainer = m_debugOverlayContainers[i];

        debugOverlayContainer = uiSubsystem->GetUIStage()->CreateUIObject<UIListView>(NAME_FMT("DebugOverlay_{}", i), Vec2i::Zero(), UIObjectSize({ 0, UIObjectSize::AUTO }, { 0, UIObjectSize::AUTO }));
        debugOverlayContainer->SetDepth(100);
        debugOverlayContainer->SetBackgroundColor(Color(0.0f, 0.0f, 0.0f, 0.0f));
        debugOverlayContainer->SetParentAlignment(Aligments[i]);
        debugOverlayContainer->SetOriginAlignment(Aligments[i]);
        debugOverlayContainer->SetAcceptsFocus(false); // so we don't steal focus from the viewport

        debugOverlayContainer->OnClick.RemoveAllDetached();
        debugOverlayContainer->OnKeyDown.RemoveAllDetached();
    }

    for (const Handle<EditorDebugOverlayBase>& debugOverlay : m_debugOverlays)
    {
        int placement = debugOverlay->GetPlacement();

        if (placement < 0 || placement >= int(m_debugOverlayContainers.Size()))
        {
            // Invalid placement, skip this overlay
            HYP_LOG(Editor, Warning, "Invalid debug overlay placement: {}", placement);

            placement = 0; // Default to the first container
        }

        debugOverlay->Initialize(m_debugOverlayContainers[placement]);

        const Handle<UIObject>& uiObject = debugOverlay->GetUIObject();
        AssertDebug(uiObject != nullptr);

        if (uiObject != nullptr)
        {
            m_debugOverlayContainers[placement]->AddChildUIObject(uiObject);
        }
    }

    for (const Handle<UIObject>& debugOverlayContainer : m_debugOverlayContainers)
    {
        uiSubsystem->GetUIStage()->AddChildUIObject(debugOverlayContainer);
    }
}

void EditorSubsystem::InitGizmoSelection()
{
    HYP_SCOPE;

    UISubsystem* uiSubsystem = GetWorld()->GetSubsystem<UISubsystem>();
    Assert(uiSubsystem != nullptr);

    Handle<UIObject> gizmoSelection = uiSubsystem->GetUIStage()->FindChildUIObject(NAME("GizmoSelection"));
    if (gizmoSelection != nullptr)
    {
        gizmoSelection->RemoveFromParent();
    }

    gizmoSelection = uiSubsystem->GetUIStage()->CreateUIObject<UIMenuBar>(NAME("GizmoSelection"), Vec2i { 0, 0 }, UIObjectSize({ 80, UIObjectSize::PIXEL }, { 12, UIObjectSize::PIXEL }));
    Assert(gizmoSelection != nullptr);

    gizmoSelection->SetDepth(150);
    gizmoSelection->SetTextSize(8.0f);
    gizmoSelection->SetBackgroundColor(Color(0.0f, 0.0f, 0.0f, 0.5f));
    gizmoSelection->SetTextColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
    gizmoSelection->SetBorderFlags(UIObjectBorderFlags::ALL);
    gizmoSelection->SetBorderRadius(5.0f);
    gizmoSelection->SetPosition(Vec2i { 5, 5 });

    Array<Pair<int, Handle<UIObject>>> gizmoMenuItems;
    gizmoMenuItems.Reserve(GetGizmos().Size());

    // add each manipulation widget to the selection menu
    for (const Handle<EditorGizmoBase>& gizmo : GetGizmos())
    {
        if (gizmo->GetManipulationMode() == EditorManipulationMode::NONE)
        {
            continue;
        }

        Handle<UIObject> gizmoMenuItem = gizmoSelection->CreateUIObject<UIMenuItem>(gizmo->InstanceClass()->GetName(), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::FILL }, { 100, UIObjectSize::PIXEL }));
        Assert(gizmoMenuItem != nullptr);

        gizmoMenuItem->SetText(gizmo->GetMenuText());

        auto it = std::lower_bound(gizmoMenuItems.Begin(), gizmoMenuItems.End(), gizmo->GetPriority(), [](const Pair<int, Handle<UIObject>>& a, int b)
            {
                return a.first < b;
            });

        gizmoMenuItems.Insert(it, Pair<int, Handle<UIObject>> { gizmo->GetPriority(), std::move(gizmoMenuItem) });
    }

    for (Pair<int, Handle<UIObject>>& gizmoMenuItem : gizmoMenuItems)
    {
        gizmoSelection->AddChildUIObject(std::move(gizmoMenuItem.second));
    }

    uiSubsystem->GetUIStage()->AddChildUIObject(gizmoSelection);
}

void EditorSubsystem::InitActiveSceneSelection()
{
    HYP_SCOPE;

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

void EditorSubsystem::SetSelectedPackage(const Handle<AssetPackage>& package)
{
    HYP_SCOPE;

    if (m_selectedPackage == package)
    {
        return;
    }

    m_selectedPackage = package;

    OnSelectedPackageChanged(package);
}

TResult<Handle<FontAtlas>> EditorSubsystem::CreateFontAtlas()
{
    HYP_SCOPE;

    auto fontFaceAsset = AssetManager::GetInstance()->Load<RC<FontFace>>("Fonts/Roboto/Roboto-Regular.ttf");

    if (fontFaceAsset.HasError())
    {
        return HYP_MAKE_ERROR(Error, "Failed to load font face! Error: {}", 0, fontFaceAsset.GetError().GetMessage());
    }

    Handle<AssetPackage> package = g_assetManager->GetAssetRegistry()->GetPackageFromPath("Engine/Fonts/Roboto", /* createIfNotExist */ true);
    Assert(package.IsValid());

    Handle<FontAtlas> atlas = MakeHandle<FontAtlas>(std::move(fontFaceAsset->Result()));

    if (Result renderAtlasResult = atlas->RenderAtlasTextures(1.0f, 2.0f, 0.1f); renderAtlasResult.HasError())
    {
        return renderAtlasResult.GetError();
    }

    // register all textures within the atlas as assets:
    for (const auto& it : atlas->GetAtlasTextures().atlases)
    {
        const Handle<Texture>& texture = it.second;
        Assert(texture != nullptr);

        HYP_LOG(Font, Verbose, "Adding texture {} to package", texture->GetName());

        Result result = package->AddAssetObject(texture, /* replaceOnConflict */ true);
        
        if (result.HasError())
        {
            HYP_LOG(Editor, Error, "Failed to add texture asset to package: {}", result.GetError().GetMessage());
        }
    }

    // need to move in return since return type is wrapped result
    return std::move(atlas);
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

bool EditorSubsystem::ExecuteCommandByName(Name name)
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

    return ExecuteCommand(command);
}

void EditorSubsystem::NewProject()
{
    Handle<EditorProject> project = MakeHandle<EditorProject>();
    InitObject(project);

    Handle<Scene> defaultScene = MakeHandle<Scene>();
    defaultScene->SetName(NAME("DefaultScene1"));
    defaultScene->SetSceneFlags(SceneFlags::DEFAULT);
    project->AddScene(defaultScene);

    Handle<DirectionalLight> sun = MakeHandle<DirectionalLight>();
    sun->SetName(NAME("SunLight"));
    sun->SetDirection(Vec3f(-0.2f, 0.8f, 0.2f).Normalize());
    sun->SetColor(Color(Vec4f(1.0f, 0.9f, 0.8f, 1.0f)));
    sun->SetIntensity(15.0f);
    InitObject(sun);

    defaultScene->GetRoot()->AddChild(sun);

    OpenProject(project);
}

void EditorSubsystem::CloseProject()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (m_currentProject)
    {
        OnProjectClosing(m_currentProject);

        m_currentProject->SetEditorSubsystem(WeakHandle<EditorSubsystem>::Null());
        m_currentProject->Close();
    }
}

void EditorSubsystem::OpenProject(const Handle<EditorProject>& project)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (project == m_currentProject)
    {
        return;
    }

    CloseProject();

    if (project)
    {
        if (project.IsValid())
        {
            project->SetEditorSubsystem(MakeWeakRef(this));
        }

        InitObject(project);

        m_currentProject = project;
        
        // temp
        project->GetWorld()->AddSystemT<DynamicSkySystem>();

        // if (Result saveResult = project->Save(); saveResult.HasError())
        // {
        //     HYP_LOG(Editor, Error, "Failed to save newly created project: {}", saveResult.GetError().GetMessage());
        // }

        OnProjectOpened(m_currentProject);

        g_editorState->SetCurrentProject(m_currentProject);
    }
}

void EditorSubsystem::ShowImportContentDialog()
{
    HYP_SCOPE;

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

                // Debug
                if (MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>())
                {
                    if (Material* material = meshComponent->material)
                    {
                        material->GetRenderAttributes().stencilReference = 0b1000;
                        material->GetRenderAttributes().flags |= MAF_STENCIL_TEST;
                        material->SetNeedsRenderProxyUpdate();
                    }
                }

                entity->SetNeedsRenderProxyUpdate();
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

        if (GetSelectedManipulationMode() == EditorManipulationMode::NONE)
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
            
            // Debug
            if (MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>())
            {
                if (Material* material = meshComponent->material)
                {
                    material->GetRenderAttributes().stencilReference = 0;
                    material->GetRenderAttributes().flags &= ~MAF_STENCIL_TEST;

                    material->SetNeedsRenderProxyUpdate();
                }
            }

            entity->SetNeedsRenderProxyUpdate();
        }
    }

    OnFocusedNodeChanged(focusedNode, previousFocusedNode, shouldSelectInOutline);
}

void EditorSubsystem::AddDebugOverlay(const Handle<EditorDebugOverlayBase>& debugOverlay)
{
    HYP_SCOPE;

    AssertDebug(debugOverlay != nullptr);

    if (!debugOverlay)
    {
        return;
    }

    AssertOnThread(g_simThread);

    UISubsystem* uiSubsystem = GetWorld()->GetSubsystem<UISubsystem>();
    Assert(uiSubsystem != nullptr);

    auto it = m_debugOverlays.Find(debugOverlay);

    if (it != m_debugOverlays.End())
    {
        return;
    }

    m_debugOverlays.PushBack(debugOverlay);

    int placement = debugOverlay->GetPlacement();

    if (placement < 0 || placement >= int(m_debugOverlayContainers.Size()))
    {
        // Invalid placement, skip this overlay
        HYP_LOG(Editor, Warning, "Invalid debug overlay placement: {}", placement);

        placement = 0; // Default to the first container
    }

    if (!m_debugOverlayContainers[placement])
    {
        return; // not initialized yet; it'll be added later
    }

    debugOverlay->Initialize(uiSubsystem->GetUIStage());

    if (const Handle<UIObject>& object = debugOverlay->GetUIObject())
    {
        Handle<UIListViewItem> listViewItem = uiSubsystem->GetUIStage()->CreateUIObject<UIListViewItem>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
        listViewItem->SetBackgroundColor(Color(0.0f, 0.0f, 0.0f, 0.0f));
        listViewItem->AddChildUIObject(object);

        m_debugOverlayContainers[placement]->AddChildUIObject(listViewItem);
    }
}

bool EditorSubsystem::RemoveDebugOverlay(EditorDebugOverlayBase* debugOverlay)
{
    HYP_SCOPE;

    if (!debugOverlay)
    {
        return false;
    }

    AssertOnThread(g_simThread);

    auto it = m_debugOverlays.FindAs(debugOverlay);

    if (it == m_debugOverlays.End())
    {
        return false;
    }

    if (const Handle<UIObject>& object = (*it)->GetUIObject())
    {
        object->RemoveFromParent();
    }

    m_debugOverlays.Erase(it);

    return true;
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
    HYP_SCOPE;

    EditorViewport* activeViewport = GetActiveViewport();
    if (activeViewport == nullptr)
    {
        return Vec3f::Zero();
    }

    const Vec3f cameraPosition = activeViewport->GetCamera()->GetTranslation();
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

void EditorSubsystem::UpdateCamera(float delta)
{
    HYP_SCOPE;
}

void EditorSubsystem::UpdateDebugOverlays(float delta)
{
    HYP_SCOPE;

    for (const Handle<EditorDebugOverlayBase>& debugOverlay : m_debugOverlays)
    {
        if (!debugOverlay->IsEnabled())
        {
            continue;
        }

        debugOverlay->Update(delta);
    }
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
    HYP_SCOPE;

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
    HYP_SCOPE;

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

    const SizeType idx = m_editorViewports.IndexOf(it);
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
    HYP_SCOPE;
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
    HYP_SCOPE;
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

            const SizeType idx = strongThis->m_editorViewports.IndexOf(it);

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
