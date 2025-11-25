/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <editor/EditorSubsystem.hpp>
#include <editor/EditorDelegates.hpp>
#include <editor/EditorCamera.hpp>
#include <editor/EditorTask.hpp>
#include <editor/EditorProject.hpp>
#include <editor/EditorActionStack.hpp>
#include <editor/EditorAction.hpp>
#include <editor/EditorState.hpp>

#include <editor/ui/debug/EditorDebugOverlay.hpp>

#include <dotnet/DotNetSystem.hpp>

#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/View.hpp>
#include <scene/Light.hpp>
#include <scene/EnvProbe.hpp>

#include <lightmapper/LightmapVolume.hpp>

#include <scene/EntityManager.hpp>
#include <scene/components/MeshComponent.hpp>
#include <scene/components/VisibilityStateComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/TransformComponent.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/AssetBatch.hpp>
#include <asset/TextureAsset.hpp>

#include <core/serialization/fbom/FBOMReader.hpp>
#include <core/serialization/fbom/FBOMWriter.hpp>

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

#include <system/AppContext.hpp>
#include <system/OpenFileDialog.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/io/ByteWriter.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <rendering/Mesh.hpp>
#include <engine/DebugDrawer.hpp>

#include <rendering/Texture.hpp>
#include <rendering/RenderCollection.hpp>

#include <rendering/util/SafeDeleter.hpp>

// temp
#include <scene/camera/streaming/CameraStreamingVolume.hpp>
#include <streaming/StreamingManager.hpp>

#include <ui/font/FontAtlas.hpp>

// temp
#include <lightmapper/LightmapperSubsystem.hpp>
#include <lightmapper/LightmapData.hpp>

#include <console/ui/ConsoleUI.hpp>

// for EnumToString
#include <core/reflection/Enum.hpp>

#include <core/math/MathUtil.hpp>

#include <dotnet/ManagedClass.hpp>

#include <scripting/ScriptingService.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <util/MeshBuilder.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

#include <HyperionEngine.hpp>

#include <EditorSubsystem.generated.inl>

namespace hyperion {

HYP_DEFINE_LOG_CHANNEL(Editor);

extern FilePath CoreApi_GetExecutablePath();

#pragma region RunningEditorTask

#pragma endregion RunningEditorTask

#pragma region GenerateLightmapsEditorTask

GenerateLightmapsEditorTask::GenerateLightmapsEditorTask(const Handle<LightmapVolume>& volume)
    : GenerateLightmapsEditorTask(Array<Handle<ObjectBase>> { { ObjCast<ObjectBase>(volume) } })
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
            && !source->IsA(EnvProbe::StaticClass()))
        {
            HYP_LOG(Editor, Error, "GenerateLightmapsEditorTask source is not a LightmapVolume or EnvProbe: \"{}\"", source->InstanceClass()->GetName());
            it = m_sources.Erase(it);

            continue;
        }

        ++it;
    }
}

void GenerateLightmapsEditorTask::Process()
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    if (m_sources.Empty())
    {
        HYP_LOG(Editor, Error, "No valid sources provided for GenerateLightmapsEditorTask");

        return;
    }

    HYP_LOG(Editor, Info, "Generating lightmaps");

    if (!m_world.IsValid() || !m_scene.IsValid())
    {
        HYP_LOG(Editor, Error, "World or scene not set for GenerateLightmapsEditorTask");

        return;
    }

    LightmapperSubsystem* lightmapperSubsystem = m_world->GetSubsystem<LightmapperSubsystem>();

    if (!lightmapperSubsystem)
    {
        lightmapperSubsystem = m_world->AddSubsystem<LightmapperSubsystem>();
    }

    for (const Handle<ObjectBase>& source : m_sources)
    {
        Task<void>* task = nullptr;

        if (source->IsA(LightmapVolume::StaticClass()))
        {
            task = lightmapperSubsystem->GenerateLightmaps(ObjCast<LightmapVolume>(source));
        }
        else if (source->IsA(EnvProbe::StaticClass()))
        {
            task = lightmapperSubsystem->GenerateLightmaps(ObjCast<EnvProbe>(source));
        }

        if (task != nullptr)
        {
            m_tasks.PushBack(task);
        }
    }
}

void GenerateLightmapsEditorTask::Cancel()
{
    if (m_tasks.Any())
    {
        for (Task<void>* task : m_tasks)
        {
            if (task != nullptr)
            {
                task->Cancel();
            }
        }
    }
}

bool GenerateLightmapsEditorTask::IsCompleted() const
{
    return m_tasks.Empty() || Every(m_tasks, &Task<void>::IsCompleted);
}

void GenerateLightmapsEditorTask::Tick(float delta)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    for (auto it = m_tasks.Begin(); it != m_tasks.End();)
    {
        Task<void>* task = *it;

        if (task == nullptr || task->IsCompleted())
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

#pragma region EditorManipulationWidgetBase

EditorManipulationWidgetBase::EditorManipulationWidgetBase()
    : m_isDragging(false)
{
}

void EditorManipulationWidgetBase::Init()
{
    // Keep the node around so we only have to load it once.
    if (m_node.IsValid() || IsA(NullEditorManipulationWidget::StaticClass()))
    {
        return;
    }

    m_node = Load_Internal();

    if (!m_node.IsValid())
    {
        HYP_LOG(Editor, Warning, "Failed to create manipulation widget node for \"{}\"!", InstanceClass()->GetName());

        // Create default node so we don't crash trying to use it
        m_node = CreateObject<Node>();
        m_node->SetName(NAME_FMT("{}_FallbackManipulationWidgetNode", InstanceClass()->GetName()));
    }

    m_node->UnlockTransform();

    m_node->SetNodeFlags(
        m_node->GetNodeFlags()
        | NodeFlags::HIDE_IN_SCENE_OUTLINE // don't display transform widget in the outline
        | NodeFlags::TRANSIENT             // should not ever be serialized to disk
    );
}

void EditorManipulationWidgetBase::Shutdown()
{
    if (m_node.IsValid())
    {
        // Remove from scene
        m_node->Remove();
    }

    m_focusedNode.Reset();
}

void EditorManipulationWidgetBase::UpdateWidget(const Handle<Node>& focusedNode)
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

    m_node->SetWorldTranslation(focusedNode->GetWorldAABB().GetCenter());
}

void EditorManipulationWidgetBase::OnDragStart(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node, const Vec3f& hitpoint)
{
    m_isDragging = true;
}

void EditorManipulationWidgetBase::OnDragEnd(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node)
{
    m_isDragging = false;
}

Handle<EditorProject> EditorManipulationWidgetBase::GetCurrentProject() const
{
    return m_currentProject.Lock();
}

#pragma endregion EditorManipulationWidgetBase

#pragma region TranslateEditorManipulationWidget

void TranslateEditorManipulationWidget::OnDragStart(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node, const Vec3f& hitpoint)
{
    EditorManipulationWidgetBase::OnDragStart(camera, mouseEvent, node, hitpoint);

    m_dragData.Unset();

    if (!node->IsA<Entity>())
    {
        return;
    }

    Entity* entity = static_cast<Entity*>(node.Get());

    MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>();

    if (!meshComponent || !meshComponent->material)
    {
        return;
    }

    const NodeTag& axisTag = node->GetTag("TransformWidgetAxis");

    if (!axisTag)
    {
        return;
    }

    int axis = axisTag.data.TryGet<int>(-1);

    if (axis == -1)
    {
        return;
    }

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

    dragData.axisDirection[axis] = 1.0f;

    if (axis == 1) // +Y, -Y
    {
        dragData.planeNormal = dragData.axisDirection.Cross(camera->GetSideVector()).Normalize();
    }
    else
    {
        dragData.planeNormal = dragData.axisDirection.Cross(camera->GetUpVector()).Normalize();
    }

    const Vec4f mouseWorld = camera->TransformScreenToWorld(mouseEvent.position);
    const Vec4f rayDirection = mouseWorld.Normalized();

    const Ray ray { camera->GetTranslation(), rayDirection.GetXYZ() };

    RayHit planeRayHit;

    if (Optional<RayHit> planeRayHitOpt = ray.TestPlane(dragData.planePoint, dragData.planeNormal))
    {
        planeRayHit = *planeRayHitOpt;
    }
    else
    {
        HYP_LOG(Editor, Debug, "Ray plane test returned no hit. plane point : {}, plane normal {}", dragData.planePoint, dragData.planeNormal);
        return;
    }

    m_dragData = dragData;

    HYP_LOG(Editor, Info, "Drag data: axis_direction = {}, plane_normal = {}, plane_point = {}, node_origin = {}",
        m_dragData->axisDirection,
        m_dragData->planeNormal,
        m_dragData->planePoint,
        m_dragData->nodeOrigin);
}

void TranslateEditorManipulationWidget::OnDragEnd(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node)
{
    EditorManipulationWidgetBase::OnDragEnd(camera, mouseEvent, node);

    // Commit editor transaction
    if (Handle<EditorProject> project = GetCurrentProject())
    {
        if (Handle<Node> focusedNode = m_focusedNode.Lock())
        {
            project->GetActionStack()->Push(CreateObject<FunctionalEditorAction>(
                NAME("Translate"),
                [manipulationMode = GetManipulationMode(), focusedNode, node = m_node, finalPosition = focusedNode->GetWorldTranslation(), origin = m_dragData->nodeOrigin]() -> EditorActionFunctions
                {
                    return {
                        [&](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                        {
                            NodeUnlockTransformScope unlockTransformScope(*focusedNode);
                            focusedNode->SetWorldTranslation(finalPosition);

                            if (Node* parent = node->FindParentWithName("TranslateWidget"))
                            {
                                parent->SetWorldTranslation(finalPosition);
                            }

                            editorSubsystem->GetManipulationWidgetHolder().SetSelectedManipulationMode(manipulationMode);

                            editorSubsystem->SetFocusedNode(focusedNode, true);
                        },
                        [&](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                        {
                            NodeUnlockTransformScope unlockTransformScope(*focusedNode);
                            focusedNode->SetWorldTranslation(origin);

                            if (Node* parent = node->FindParentWithName("TranslateWidget"))
                            {
                                parent->SetWorldTranslation(origin);
                            }

                            editorSubsystem->GetManipulationWidgetHolder().SetSelectedManipulationMode(manipulationMode);

                            editorSubsystem->SetFocusedNode(focusedNode, true);
                        }
                    };
                }));
        }
    }

    m_dragData.Unset();
}

bool TranslateEditorManipulationWidget::OnMouseHover(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node)
{
    if (!node->IsA<Entity>())
    {
        return false;
    }

    Entity* entity = static_cast<Entity*>(node.Get());

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

bool TranslateEditorManipulationWidget::OnMouseLeave(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node)
{
    if (!node->IsA<Entity>())
    {
        return false;
    }

    Entity* entity = static_cast<Entity*>(node.Get());

    MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>();

    if (!meshComponent || !meshComponent->material)
    {
        return false;
    }

    if (const NodeTag& tag = node->GetTag("TransformWidgetElementColor"))
    {
        meshComponent->material->SetParameter(
            MATERIAL_KEY_ALBEDO,
            tag.data.TryGet<Vec4f>(Vec4f::Zero()));
    }

    return true;
}

bool TranslateEditorManipulationWidget::OnMouseMove(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node)
{
    if (!mouseEvent.mouseButtons[MouseButtonState::LEFT])
    {
        return false;
    }

    if (!node->IsA<Entity>())
    {
        return false;
    }

    if (!m_dragData)
    {
        return false;
    }

    Entity* entity = static_cast<Entity*>(node.Get());

    MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>();

    if (!meshComponent || !meshComponent->material)
    {
        return false;
    }

    const NodeTag& axisTag = node->GetTag("TransformWidgetAxis");

    if (!axisTag)
    {
        return false;
    }
    const Vec4f mouseWorld = camera->TransformScreenToWorld(mouseEvent.position);
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

    const float t = (planeRayHit.hitpoint - m_dragData->hitpointOrigin).Dot(m_dragData->axisDirection);
    const Vec3f translation = m_dragData->nodeOrigin + (m_dragData->axisDirection * t);

    Handle<Node> focusedNode = m_focusedNode.Lock();

    if (!focusedNode.IsValid())
    {
        return false;
    }

    NodeUnlockTransformScope unlockTransformScope(*focusedNode);
    focusedNode->SetWorldTranslation(translation);

    if (Node* parent = node->FindParentWithName("TranslateWidget"))
    {
        parent->SetWorldTranslation(translation);
    }

    return true;
}

bool TranslateEditorManipulationWidget::OnKeyPress(const Handle<Camera>& camera, const KeyboardEvent& keyboardEvent, const Handle<Node>& node)
{
    if (!node)
    {
        return false;
    }

    switch (keyboardEvent.keyCode)
    {
    case KeyCode::ARROW_LEFT:
    case KeyCode::ARROW_RIGHT:
    case KeyCode::ARROW_UP:
    case KeyCode::ARROW_DOWN: // fallthrough
    {
        const Bitset& keyStates = camera->GetCameraController()->GetInputHandler()->GetKeyStates();

        const bool snapMovement = keyStates.Test(uint32(KeyCode::LEFT_ALT)) | keyStates.Test(uint32(KeyCode::RIGHT_ALT));

        float step = 1.0f;

        if (keyStates.Test(uint32(KeyCode::LEFT_SHIFT)) | keyStates.Test(uint32(KeyCode::RIGHT_SHIFT)))
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
        case KeyCode::ARROW_LEFT:
            moveVec = nodeSideVector;

            break;
        case KeyCode::ARROW_RIGHT:
            moveVec = -nodeSideVector;

            break;
        case KeyCode::ARROW_UP:
            moveVec = nodeForwardVector;

            break;
        case KeyCode::ARROW_DOWN:
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
            // @TODO: Configurable snap value
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

Handle<Node> TranslateEditorManipulationWidget::Load_Internal() const
{
    auto result = AssetManager::GetInstance()->Load<Node>("models/editor/axis_arrows.obj");

    if (result.HasValue())
    {
        if (Handle<Node> node = result->Result())
        {
            node->SetName(NAME("TranslateWidget"));

            node->SetWorldScale(2.5f);

            node->GetChild(1)->SetName(NAME("AxisX"));
            node->GetChild(1)->AddTag(NodeTag(NAME("TransformWidgetAxis"), 0));

            node->GetChild(0)->SetName(NAME("AxisY"));
            node->GetChild(0)->AddTag(NodeTag(NAME("TransformWidgetAxis"), 1));

            node->GetChild(2)->SetName(NAME("AxisZ"));
            node->GetChild(2)->AddTag(NodeTag(NAME("TransformWidgetAxis"), 2));

            for (Node* child : node->GetDescendants())
            {
                if (!child->IsA<Entity>())
                {
                    continue;
                }

                Entity* childEntity = static_cast<Entity*>(child);

                childEntity->RemoveTag<EntityTag::STATIC>();
                childEntity->AddTag<EntityTag::DYNAMIC>();

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

                // disable depth write and depth test
                materialAttributes.flags &= ~(MAF_DEPTH_TEST);
                materialAttributes.bucket = RB_DEBUG;

                meshComponent->material = MaterialCache::GetInstance()->CreateMaterial(materialAttributes, materialParameters);
                meshComponent->material->SetIsDynamic(true);

                childEntity->AddTag<EntityTag::UPDATE_RENDER_PROXY>();
                childEntity->Node::AddTag(NodeTag(NAME("TransformWidgetElementColor"), Vec4f(meshComponent->material->GetParameter(MATERIAL_KEY_ALBEDO))));
            }

            // FileByteWriter byteWriter(GetResourceDirectory() / "models/editor/axis_arrows.hypmodel");
            // FBOMWriter writer { FBOMWriterConfig {} };
            // writer.Append(*node);

            // FBOMResult writeErr = writer.Emit(&byteWriter);

            // byteWriter.Close();
            //
            //            if (writeErr)
            //            {
            //                HYP_LOG(Editor, Error, "Failed to write axis arrows to disk: {}", writeErr.message);
            //            }

            return node;
        }
    }

    HYP_LOG(Editor, Error, "Failed to load axis arrows: {}", result.GetError().GetMessage());

    return Handle<Node>::empty;
}

#pragma endregion TranslateEditorManipulationWidget

#pragma region EditorManipulationWidgetHolder

EditorManipulationWidgetHolder::EditorManipulationWidgetHolder(EditorSubsystem* editorSubsystem)
    : m_editorSubsystem(editorSubsystem),
      m_selectedManipulationMode(EditorManipulationMode::NONE)
{
    m_manipulationWidgets.Insert(CreateObject<NullEditorManipulationWidget>());
    m_manipulationWidgets.Insert(CreateObject<TranslateEditorManipulationWidget>());
}

EditorManipulationMode EditorManipulationWidgetHolder::GetSelectedManipulationMode() const
{
    AssertOnThread(g_gameThread);

    return m_selectedManipulationMode;
}

void EditorManipulationWidgetHolder::SetSelectedManipulationMode(EditorManipulationMode mode)
{
    AssertOnThread(g_gameThread);

    if (mode == m_selectedManipulationMode)
    {
        return;
    }

    EditorManipulationWidgetBase& newWidget = *m_manipulationWidgets.At(mode);
    EditorManipulationWidgetBase& prevWidget = *m_manipulationWidgets.At(m_selectedManipulationMode);

    OnSelectedManipulationWidgetChange(newWidget, prevWidget);

    m_selectedManipulationMode = mode;
}

EditorManipulationWidgetBase& EditorManipulationWidgetHolder::GetSelectedManipulationWidget() const
{
    AssertOnThread(g_gameThread);

    return *m_manipulationWidgets.At(m_selectedManipulationMode);
}

void EditorManipulationWidgetHolder::SetCurrentProject(const WeakHandle<EditorProject>& project)
{
    AssertOnThread(g_gameThread);

    m_currentProject = project;

    for (auto& it : m_manipulationWidgets)
    {
        it->SetCurrentProject(project);
    }
}

void EditorManipulationWidgetHolder::Initialize()
{
    AssertOnThread(g_gameThread);

    for (const Handle<EditorManipulationWidgetBase>& widget : m_manipulationWidgets)
    {
        widget->SetEditorSubsystem(m_editorSubsystem);
        widget->SetCurrentProject(m_currentProject);

        InitObject(widget);
    }
}

void EditorManipulationWidgetHolder::Shutdown()
{
    AssertOnThread(g_gameThread);

    if (m_selectedManipulationMode != EditorManipulationMode::NONE)
    {
        OnSelectedManipulationWidgetChange(
            *m_manipulationWidgets.At(EditorManipulationMode::NONE),
            *m_manipulationWidgets.At(m_selectedManipulationMode));

        m_selectedManipulationMode = EditorManipulationMode::NONE;
    }

    for (auto& it : m_manipulationWidgets)
    {
        it->Shutdown();
    }
}

#pragma endregion EditorManipulationWidgetHolder

#pragma region EditorSubsystem

static constexpr bool g_showOnlyActiveScene = true; // @TODO: Make this configurable

#ifdef HYP_EDITOR

EditorSubsystem::EditorSubsystem()
    : m_editorCameraEnabled(false),
      m_shouldCancelNextClick(false),
      m_manipulationWidgetHolder(this)
{
    m_editorDelegates = new EditorDelegates();

    m_taskManager.OnTaskAdded
        .Bind([this](RunningEditorTask& runningTask)
            {
                UISubsystem* uiSubsystem = GetWorld()->GetSubsystem<UISubsystem>();
                Assert(uiSubsystem != nullptr);

                Handle<UIMenuItem> tasksMenuItem = ObjCast<UIMenuItem>(uiSubsystem->GetUIStage()->FindChildUIObject(NAME("Tasks_MenuItem")));

                if (tasksMenuItem != nullptr)
                {
                    const uint32 numRunningTasks = m_taskManager.NumRunningTasks();

                    if (numRunningTasks == 0)
                    {
                        tasksMenuItem->SetText("No running tasks");
                    }
                    else if (numRunningTasks == 1)
                    {
                        tasksMenuItem->SetText("1 running task");
                    }
                    else
                    {
                        tasksMenuItem->SetText(HYP_FORMAT("{} running task(s)", numRunningTasks));
                    }

                    if (UIObjectSpawnContext context { tasksMenuItem })
                    {
                        Handle<UIGrid> taskGrid = context.CreateUIObject<UIGrid>(NAME("Task_Grid"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));
                        taskGrid->SetNumColumns(12);

                        Handle<UIGridRow> taskGridRow = taskGrid->AddRow();
                        taskGridRow->SetSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));

                        Handle<UIGridColumn> taskGridColumnLeft = taskGridRow->AddColumn();
                        taskGridColumnLeft->SetColumnSize(8);
                        taskGridColumnLeft->AddChildUIObject(runningTask.CreateUIObject(uiSubsystem->GetUIStage()));

                        Handle<UIGridColumn> taskGridColumnRight = taskGridRow->AddColumn();
                        taskGridColumnRight->SetColumnSize(4);

                        Handle<UIButton> cancelButton = context.CreateUIObject<UIButton>(NAME("Task_Cancel"), Vec2i::Zero(), UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
                        cancelButton->SetText("Cancel");
                        cancelButton->OnClick
                            .Bind(
                                [taskWeak = MakeWeakRef(runningTask.GetTask())](...)
                                {
                                    if (Handle<EditorTaskBase> task = taskWeak.Lock())
                                    {
                                        task->Cancel();
                                    }

                                    return UIEventHandlerResult::OK;
                                })
                            .Detach();

                        taskGridColumnRight->AddChildUIObject(cancelButton);

                        runningTask.SetUIObject(taskGrid);

                        tasksMenuItem->AddChildUIObject(taskGrid);

                        // testing
                        Handle<Texture> dummyIconTexture;

                        if (auto dummyIconTextureAsset = AssetManager::GetInstance()->Load<Texture>("textures/editor/icons/loading.png"))
                        {
                            dummyIconTexture = dummyIconTextureAsset->Result();
                        }

                        tasksMenuItem->SetIconTexture(dummyIconTexture);
                    }
                }
            })
        .Detach();

    m_taskManager.OnTaskRemoved
        .Bind([this](RunningEditorTask& runningTask)
            {
                UISubsystem* uiSubsystem = GetWorld()->GetSubsystem<UISubsystem>();
                Assert(uiSubsystem != nullptr);

                Handle<UIMenuItem> tasksMenuItem = ObjCast<UIMenuItem>(uiSubsystem->GetUIStage()->FindChildUIObject(NAME("Tasks_MenuItem")));

                if (tasksMenuItem != nullptr)
                {
                    const uint32 numRunningTasks = m_taskManager.NumRunningTasks();

                    if (numRunningTasks == 0)
                    {
                        tasksMenuItem->SetText("No running tasks");
                    }
                    else if (numRunningTasks == 1)
                    {
                        tasksMenuItem->SetText("1 running task");
                    }
                    else
                    {
                        tasksMenuItem->SetText(HYP_FORMAT("{} running task(s)", numRunningTasks));
                    }
                }

                if (const Handle<UIObject>& uiObject = runningTask.GetUIObject())
                {
                    uiObject->RemoveFromParent();
                }
            })
        .Detach();

    OnProjectOpened
        .Bind([this](const Handle<EditorProject>& project)
            {
                HYP_LOG(Editor, Info, "Opening project: {}", *project->GetName());

                InitObject(project);

                m_manipulationWidgetHolder.Initialize();
                m_manipulationWidgetHolder.SetCurrentProject(project);

                g_engineDriver->AddWorld(project->GetWorld());

                WeakHandle<Scene> activeScene;

                for (const Handle<Scene>& scene : project->GetWorld()->GetScenes())
                {
                    Assert(scene != nullptr);

                    if ((scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) != SceneFlags::FOREGROUND)
                    {
                        continue;
                    }

                    // add to all editor views
                    for (const Handle<View>& view : m_editorViews)
                    {
                        view->AddScene(scene);
                    }

                    if (!activeScene.IsValid())
                    {
                        activeScene = scene;
                    }

                    m_delegateHandlers.Add(
                        scene->OnRootNodeChanged
                            .Bind([this](const Handle<Node>& newRoot, const Handle<Node>& prevRoot)
                                {
                                    UpdateWatchedNodes();
                                }));
                }

                for (const Handle<View>& view : m_editorViews)
                {
                    project->GetWorld()->AddView(view);
                }

                UpdateWatchedNodes();

                m_delegateHandlers.Add(
                    project->GetWorld()->OnSceneAdded.Bind([this, projectWeak = project.ToWeak()](World*, const Handle<Scene>& scene)
                        {
                            Assert(scene != nullptr);

                            if ((scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) != SceneFlags::FOREGROUND)
                            {
                                return;
                            }

                            Handle<EditorProject> project = projectWeak.Lock();
                            Assert(project != nullptr);

                            // Add scene to all editor views
                            for (const Handle<View>& view : m_editorViews)
                            {
                                view->AddScene(scene);
                            }

                            m_delegateHandlers.Add(
                                scene->OnRootNodeChanged
                                    .Bind([this](const Handle<Node>& newRoot, const Handle<Node>& prevRoot)
                                        {
                                            UpdateWatchedNodes();
                                        }));

                            if (!m_activeScene)
                            {
                                SetActiveScene(scene);
                            }

                            // reinitialize scene selector on scene add
                            InitActiveSceneSelection();
                        }));

                m_delegateHandlers.Add(
                    project->GetWorld()->OnSceneRemoved.Bind([this, projectWeak = project.ToWeak()](World*, Scene* scene)
                        {
                            Assert(scene != nullptr);

                            Handle<EditorProject> project = projectWeak.Lock();
                            Assert(project != nullptr);

                            HYP_LOG(Editor, Info, "Project {} removed scene: {}", *project->GetName(), *scene->GetName());

                            m_delegateHandlers.Remove(&scene->OnRootNodeChanged);

                            // remove from all editor views
                            for (const Handle<View>& view : m_editorViews)
                            {
                                view->RemoveScene(scene);
                            }

                            StopWatchingNode(scene->GetRoot());

                            // GetWorld()->RemoveScene(scene);

                            // reinitialize scene selector on scene remove
                            InitActiveSceneSelection();
                        }));

                m_delegateHandlers.Remove("OnPackageAdded");
                m_delegateHandlers.Remove("OnPackageRemoved");

                if (m_contentBrowserDirectoryList && m_contentBrowserDirectoryList->GetDataSource())
                {
                    m_contentBrowserDirectoryList->GetDataSource()->Clear();

                    for (const Handle<AssetPackage>& package : g_assetManager->GetAssetRegistry()->GetPackages())
                    {
                        Assert(package.IsValid());

                        if (!package->IsReady())
                        {
                            HYP_LOG(Editor, Debug, "Package {} with UUID {} is not ready; skipping adding to content browser", package->GetName(), package->GetUUID());

                            continue;
                        }

                        AddPackageToContentBrowser(package, true);
                    }

                    m_delegateHandlers.Add(
                        NAME("OnPackageAdded"),
                        g_assetManager->GetAssetRegistry()->OnPackageAdded.BindThreaded([this](const Handle<AssetPackage>& package)
                            {
                                AddPackageToContentBrowser(package, false);
                            },
                            g_gameThread));

                    m_delegateHandlers.Add(
                        NAME("OnPackageRemoved"),
                        g_assetManager->GetAssetRegistry()->OnPackageRemoved.BindThreaded([this](const Handle<AssetPackage>& package)
                            {
                                RemovePackageFromContentBrowser(package);
                            },
                            g_gameThread));
                }

                m_delegateHandlers.Add(
                    NAME("OnGameStateChange"),
                    GetWorld()->OnGameStateChange.Bind([this](World* world, GameStateMode previousGameStateMode, GameStateMode currentGameStateMode)
                        {
                            UpdateWatchedNodes();

                            switch (currentGameStateMode)
                            {
                            case GameStateMode::EDITOR:
                            {
                                m_delegateHandlers.Remove("World_SceneAddedDuringSimulation");
                                m_delegateHandlers.Remove("World_SceneRemovedDuringSimulation");

                                break;
                            }
                            case GameStateMode::SIMULATING: // fallthrough
                            case GameStateMode::PAUSED:
                            {
                                // unset manipulation widgets
                                m_manipulationWidgetHolder.SetSelectedManipulationMode(EditorManipulationMode::NONE);

                                m_delegateHandlers.Add(
                                    NAME("World_SceneAddedDuringSimulation"),
                                    world->OnSceneAdded.Bind([this](World*, const Handle<Scene>& scene)
                                        {
                                            if (!scene.IsValid())
                                            {
                                                return;
                                            }

                                            StartWatchingNode(scene->GetRoot());
                                        }));

                                m_delegateHandlers.Add(
                                    NAME("World_SceneRemovedDuringSimulation"),
                                    world->OnSceneRemoved.Bind([this](World*, Scene* scene)
                                        {
                                            if (!scene)
                                            {
                                                return;
                                            }

                                            StopWatchingNode(scene->GetRoot());
                                        }));

                                break;
                            }
                            default:
                                HYP_UNREACHABLE();
                                break;
                            }
                        }));

                SetActiveScene(activeScene);
            })
        .Detach();

    OnProjectClosing
        .Bind([this](const Handle<EditorProject>& project)
            {
                g_engineDriver->RemoveWorld(project->GetWorld());

                // Shutdown to reinitialize widget holder after project is opened
                m_manipulationWidgetHolder.Shutdown();

                m_focusedNode.Reset();

                if (m_highlightNode.IsValid())
                {
                    m_highlightNode->Remove();
                }

                SetActiveScene(WeakHandle<Scene>::empty);

                for (const Handle<Scene>& scene : project->GetWorld()->GetScenes())
                {
                    if (!scene.IsValid())
                    {
                        continue;
                    }

                    HYP_LOG(Editor, Info, "Closing project {} scene: {}", *project->GetName(), *scene->GetName());

                    // remove from all editor views
                    for (const Handle<View>& view : m_editorViews)
                    {
                        view->RemoveScene(scene);
                    }

                    m_delegateHandlers.Remove(&scene->OnRootNodeChanged);

                    StopWatchingNode(scene->GetRoot());

                    // GetWorld()->RemoveScene(scene);
                }

                for (const Handle<View>& view : m_editorViews)
                {
                    project->GetWorld()->RemoveView(view);
                }

                m_delegateHandlers.Remove(&project->GetWorld()->OnSceneAdded);
                m_delegateHandlers.Remove(&project->GetWorld()->OnSceneRemoved);

                m_delegateHandlers.Remove("SetBuildBVHFlag");
                m_delegateHandlers.Remove("OnPackageAdded");
                m_delegateHandlers.Remove("OnPackageRemoved");
                m_delegateHandlers.Remove("OnGameStateChange");

                if (m_contentBrowserDirectoryList && m_contentBrowserDirectoryList->GetDataSource())
                {
                    m_contentBrowserDirectoryList->GetDataSource()->Clear();
                }

                // reinitialize scene selector
                InitActiveSceneSelection();
            })
        .Detach();

    m_manipulationWidgetHolder.OnSelectedManipulationWidgetChange
        .Bind([this](EditorManipulationWidgetBase& newWidget, EditorManipulationWidgetBase& prevWidget)
            {
                SetHoveredManipulationWidget(MouseEvent {}, nullptr, Handle<Node>::empty);

                if (prevWidget.GetManipulationMode() != EditorManipulationMode::NONE)
                {
                    if (prevWidget.GetNode().IsValid())
                    {
                        prevWidget.GetNode()->Remove();
                    }

                    prevWidget.UpdateWidget(Handle<Node>::empty);
                }

                if (newWidget.GetManipulationMode() != EditorManipulationMode::NONE)
                {
                    newWidget.UpdateWidget(m_focusedNode.Lock());

                    if (!newWidget.GetNode().IsValid())
                    {
                        HYP_LOG(Editor, Warning, "Manipulation widget has no valid node; cannot attach to scene");

                        return;
                    }

                    m_editorScene->GetRoot()->AddChild(newWidget.GetNode());
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

    m_editorScene = CreateObject<Scene>(NAME("EditorScene"), SceneFlags::FOREGROUND | SceneFlags::EDITOR);
    GetWorld()->AddScene(m_editorScene);

    Handle<Node> cameraNode = m_editorScene->GetRoot()->AddChild();

    m_camera = CreateObject<Camera>();
    m_camera->AddCameraController(CreateObject<EditorCameraController>());
    m_camera->SetName(NAME("EditorCamera"));
    m_camera->SetFOV(60.0f);
    m_camera->SetNear(0.1f);
    m_camera->SetFar(3000.0f);
    InitObject(m_camera);

    m_editorScene->GetEntityManager()->AddExistingEntity(m_camera);
    m_editorScene->GetEntityManager()->AddTag<EntityTag::CAMERA_PRIMARY>(m_camera);

    cameraNode->AddChild(m_camera);
    cameraNode->SetName(m_camera->GetName());

    LoadEditorUIDefinitions();

    InitContentBrowser();
    InitViewport();
    InitSceneOutline();
    InitActiveSceneSelection();

    InitDetailView();

    CreateHighlightNode();

    g_engineDriver->GetScriptingService()
        ->OnScriptStateChanged
        .Bind([](const ScriptData& script)
            {
                HYP_LOG(Script, Debug, "Script state changed: now is {}", EnumToString(ScriptCompileStatus(script.compileStatus)));
            })
        .Detach();

    if (Handle<AssetCollector> baseAssetCollector = g_assetManager->GetBaseAssetCollector())
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

    for (const Handle<View>& view : m_editorViews)
    {
        view->RemoveScene(m_editorScene);
    }

    GetWorld()->RemoveScene(m_editorScene);

    if (m_currentProject)
    {
        g_editorState->SetCurrentProject(nullptr);

        OnProjectClosing(m_currentProject);

        m_currentProject->Close();
        m_currentProject.Reset();
    }
}

void EditorSubsystem::Update(float delta)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    m_editorDelegates->Update();

    UpdateCamera(delta);
    UpdateTasks(delta);
    UpdateDebugOverlays(delta);

    if (m_focusedNode.IsValid())
    {
        if (Handle<Node> focusedNode = m_focusedNode.Lock())
        {
            DebugDrawCommandList& dbg = g_engineDriver->GetDebugDrawer()->CreateCommandList();

            dbg.box(focusedNode->GetWorldTranslation(), focusedNode->GetWorldAABB().GetExtent() * 0.5f + Vec3f(FLT_EPSILON), Color::Cyan());
        }
        //        g_engineDriver->GetDebugDrawer()->box(m_focusedNode->GetWorldTranslation(), m_focusedNode->GetWorldAABB().GetExtent(), Color(1.0f), RenderableAttributeSet(
        //            MeshAttributes {
        //                .vertexAttributes = staticMeshVertexAttributes
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

    for (const Handle<View>& view : m_editorViews)
    {
        if (!(view->GetViewDesc().flags & ViewFlags::GBUFFER))
        {
            continue; // skip non-primary views
        }

        for (Mesh* mesh : view->GetRenderProxyList(RenderApi::GetRingIndex())->GetMeshes())
        {
            pickRpl.GetMeshes().Track(mesh->Id(), mesh);
        }
    }

    // @TODO: Prioritize based on distance from camera
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
        HYP_FAIL("Failed to create font atlas for editor UI: {}", *fontAtlasResult.GetError().GetMessage());
    }

    uiSubsystem->GetUIStage()->SetDefaultFontAtlas(*fontAtlasResult);

    auto loadedUiAsset = AssetManager::GetInstance()->Load<UIObject>("ui/Editor.Main.ui.xml");
    Assert(loadedUiAsset.HasValue(), "Failed to load editor UI definitions!");

    Handle<UIStage> loadedUi = ObjCast<UIStage>(loadedUiAsset->Result());
    Assert(loadedUi.IsValid(), "Failed to load editor UI");

    loadedUi->SetDefaultFontAtlas(*fontAtlasResult);

    uiSubsystem->GetUIStage()->AddChildUIObject(loadedUi);

    loadedUi->Focus();
}

void EditorSubsystem::CreateHighlightNode()
{
    // m_highlightNode = Handle<Node>(CreateObject<Node>("Editor_Highlight"));
    // m_highlightNode->SetFlags(m_highlightNode->GetNodeFlags() | NodeFlags::HIDE_IN_SCENE_OUTLINE);

    // const Handle<Entity> entity = m_scene->GetEntityManager()->AddEntity();

    // Handle<Mesh> mesh = MeshBuilder::Cube();
    // InitObject(mesh);

    // Handle<Material> material = g_materialSystem->GetOrCreate(
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
    for (const Handle<View>& view : m_editorViews)
    {
        if (m_currentProject != nullptr)
        {
            m_currentProject->GetWorld()->RemoveView(view);
        }
    }

    SafeDelete(std::move(m_editorViews));

    UISubsystem* uiSubsystem = GetWorld()->GetSubsystem<UISubsystem>();
    Assert(uiSubsystem != nullptr);

    Handle<UIObject> sceneImageObject = uiSubsystem->GetUIStage()->FindChildUIObject(NAME("Scene_Image"));
    if (!sceneImageObject)
    {
        HYP_LOG(Editor, Error, "Scene image object not found in UI stage!");
        return;
    }

    uiSubsystem->GetUIStage()->UpdateSize(true);

    // bind console key
    m_delegateHandlers.Add(uiSubsystem->GetUIStage()->OnKeyDown.Bind([this](const KeyboardEvent& event)
        {
            // Check we aren't entering text in non-console text field
            UISubsystem* uiSubsystem = GetWorld()->GetSubsystem<UISubsystem>();
            Assert(uiSubsystem != nullptr && uiSubsystem->GetUIStage() != nullptr);

            // if (Handle<UIObject> focusedObject = uiSubsystem->GetUIStage()->GetFocusedObject().Lock())
            // {
            //     if (focusedObject->IsA<UITextbox>() && (m_consoleUi && !m_consoleUi->HasFocus()))
            //     {
            //         HYP_BREAKPOINT;
            //         return UIEventHandlerResult::OK;
            //     }
            // }

            if (event.keyCode == KeyCode::TILDE)
            {
                const bool isConsoleOpen = m_consoleUi && m_consoleUi->IsVisible();

                if (isConsoleOpen)
                {
                    m_consoleUi->SetIsVisible(false);

                    for (const Handle<UIObject>& debugOverlayContainer : m_debugOverlayContainers)
                    {
                        if (!debugOverlayContainer)
                        {
                            continue;
                        }

                        debugOverlayContainer->SetIsVisible(true);
                    }
                }
                else
                {
                    m_consoleUi->SetIsVisible(true);

                    for (const Handle<UIObject>& debugOverlayContainer : m_debugOverlayContainers)
                    {
                        if (!debugOverlayContainer)
                        {
                            continue;
                        }

                        debugOverlayContainer->SetIsVisible(false);
                    }
                }

                return UIEventHandlerResult::STOP_BUBBLING;
            }

            return UIEventHandlerResult::OK;
        }));

    Vec2u viewportSize = MathUtil::Max(Vec2u(sceneImageObject->GetActualSize()), Vec2u::One());
    m_camera->SetDimensions(Vec2i(viewportSize));

    ViewDesc viewDesc {
        .flags = ViewFlags::DEFAULT
            | ViewFlags::GBUFFER
            | ViewFlags::ENABLE_READBACK
            | ViewFlags::MATCH_CAMERA_DIMENSIONS,
        .viewport = Viewport { .extent = viewportSize, .position = Vec2i::Zero() },
        .outputTargetDesc = { .extent = viewportSize },
        .camera = m_camera,
        .readbackTextureFormat = TF_R10G10B10A2
    };

    Handle<View> view = CreateObject<View>(viewDesc);
    InitObject(view);

    // view needs editor scene so we can see things like manipulation widgets
    view->AddScene(m_editorScene);

    HYP_LOG(Editor, Info, "Creating editor viewport with size: {}", viewportSize);

    // add all current project scenes to the view
    if (m_currentProject != nullptr)
    {
        for (const Handle<Scene>& scene : m_currentProject->GetWorld()->GetScenes())
        {
            Assert(scene != nullptr);

            if ((scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) != SceneFlags::FOREGROUND)
            {
                continue;
            }

            view->AddScene(scene);
        }

        m_currentProject->GetWorld()->AddView(view);
    }

    m_editorViews.PushBack(view);

    m_delegateHandlers.Remove(&sceneImageObject->OnSizeChange);
    m_delegateHandlers.Add(sceneImageObject->OnSizeChange.Bind([this, sceneImageObjectWeak = sceneImageObject.ToWeak(), cameraWeak = m_camera.ToWeak()]()
        {
            Handle<UIObject> sceneImageObject = sceneImageObjectWeak.Lock();
            if (!sceneImageObject)
            {
                HYP_LOG(Editor, Warning, "Scene image object is no longer valid!");
                return UIEventHandlerResult::ERR;
            }

            Handle<Camera> camera = cameraWeak.Lock();
            if (!camera)
            {
                HYP_LOG(Editor, Warning, "Camera is no longer valid!");
                return UIEventHandlerResult::ERR;
            }

            Vec2i viewportSize = MathUtil::Max(sceneImageObject->GetActualSize(), Vec2i::One());
            camera->SetDimensions(viewportSize);

            HYP_LOG(Editor, Info, "Main editor view viewport size changed to {}", viewportSize);

            return UIEventHandlerResult::OK;
        }));

    m_camera->SetDimensions(Vec2i(viewportSize));

    Handle<UIImage> uiImage = ObjCast<UIImage>(sceneImageObject);
    Assert(uiImage.IsValid());

    Handle<Texture> readbackTexture = view->GetReadbackTexture();
    Assert(readbackTexture != nullptr);

    m_delegateHandlers.Remove("OnReadbackTextureChanged");
    m_delegateHandlers.Add(NAME("OnReadbackTextureChanged"), view->OnReadbackTextureChanged.Bind([uiImageWeak = uiImage.ToWeak()](const Handle<Texture>& readbackTexture)
                                                                 {
                                                                     Handle<UIImage> uiImage = uiImageWeak.Lock();
                                                                     AssertDebug(uiImage != nullptr);

                                                                     uiImage->SetTexture(readbackTexture);
                                                                 }));

    m_delegateHandlers.Remove(&uiImage->OnClick);
    m_delegateHandlers.Add(uiImage->OnClick.Bind([this](const MouseEvent& event)
        {
            if (m_shouldCancelNextClick)
            {
                return UIEventHandlerResult::STOP_BUBBLING;
            }

            // if (m_camera->GetCameraController()->GetInputHandler()->OnClick(event))
            // {
            //     return UIEventHandlerResult::STOP_BUBBLING;
            // }

            if (GetWorld()->GetGameState().IsEditor())
            {
                if (IsHoveringManipulationWidget())
                {
                    return UIEventHandlerResult::STOP_BUBBLING;
                }

                const Vec4f mouseWorld = m_camera->TransformScreenToWorld(event.position);
                const Vec4f rayDirection = mouseWorld.Normalized();

                const Ray ray { m_camera->GetTranslation(), rayDirection.GetXYZ() };

                RayTestResults results;

                bool hasHits = false;
                for (const Handle<View>& view : m_editorViews)
                {
                    if (view->TestRay(ray, results, RTF_USE_BVH | RTF_EDITOR_PICK))
                    {
                        hasHits = true;
                    }
                }

                if (hasHits)
                {
                    for (const RayHit& hit : results)
                    {
                        /// \FIXME: Can't do TypeId::ForType<Entity>, there may be derived types of Entity
                        if (ObjId<Entity> entityId = ObjId<Entity>(ObjIdBase { TypeId::ForType<Entity>(), hit.id }))
                        {
                            Handle<Entity> entity { entityId };
                            HYP_LOG(Editor, Debug, "Clicked on entity: {} ({})", *entity->GetName(), entityId);

                            EntityManager* entityManager = entity->GetEntityManager();

                            if (!entityManager)
                            {
                                continue;
                            }

                            SetFocusedNode(entity, true);

                            break;
                        }
                    }

                    return UIEventHandlerResult::STOP_BUBBLING;
                }
            }

            return UIEventHandlerResult::OK;
        }));

    m_delegateHandlers.Remove(&uiImage->OnMouseLeave);
    m_delegateHandlers.Add(uiImage->OnMouseLeave.Bind([this](const MouseEvent& event)
        {
            if (IsHoveringManipulationWidget())
            {
                SetHoveredManipulationWidget(event, nullptr, Handle<Node>::empty);
            }

            m_camera->GetCameraController()->GetInputHandler()->OnMouseLeave(event);

            return UIEventHandlerResult::OK;
        }));

    m_delegateHandlers.Remove(&uiImage->OnMouseDrag);
    m_delegateHandlers.Add(uiImage->OnMouseDrag.Bind([this, uiImage = uiImage.Get()](const MouseEvent& event)
        {
            // prevent click being triggered on release once mouse has been dragged
            m_shouldCancelNextClick = true;

            if (!event.inputManager->IsMouseLocked() && IsHoveringManipulationWidget())
            {
                // If the mouse is currently over a manipulation widget, don't allow camera to handle the event
                Handle<EditorManipulationWidgetBase> manipulationWidget = m_hoveredManipulationWidget.Lock();
                Handle<Node> node = m_hoveredManipulationWidgetNode.Lock();

                if (!manipulationWidget || !node)
                {
                    HYP_LOG(Editor, Warning, "Failed to lock hovered manipulation widget or node");

                    return UIEventHandlerResult::ERR;
                }

                if (manipulationWidget->OnMouseMove(m_camera, event, Handle<Node>(node)))
                {
                    return UIEventHandlerResult::STOP_BUBBLING;
                }
            }

            m_camera->GetCameraController()->GetInputHandler()->OnMouseDrag(event);

            // handle move before we reset mouse pos
            if (event.inputManager->IsMouseLocked())
            {
                const Vec2f position = uiImage->GetAbsolutePosition();
                const Vec2i size = uiImage->GetActualSize();

                // @TODO : refactor, we're calling SetMousePosition() on game thread which is causing warnings on macOS
                // since this is a UI event we should call this on the UI thread (main)
                event.inputManager->SetMousePosition(Vec2i(position + Vec2f(size) * 0.5f));

                return UIEventHandlerResult::OK;
            }

            return UIEventHandlerResult::STOP_BUBBLING;
        }));

    m_delegateHandlers.Remove(&uiImage->OnMouseMove);
    m_delegateHandlers.Add(uiImage->OnMouseMove.Bind([this, uiImage = uiImage.Get()](const MouseEvent& event)
        {
            m_camera->GetCameraController()->GetInputHandler()->OnMouseMove(event);

            // Hover over a manipulation widget when mouse is not down
            if (!event.mouseButtons[MouseButtonState::LEFT]
                && GetWorld()->GetGameState().IsEditor()
                && m_manipulationWidgetHolder.GetSelectedManipulationMode() != EditorManipulationMode::NONE)
            {
                // Ray test the widget

                const Vec4f mouseWorld = m_camera->TransformScreenToWorld(event.position);
                const Vec4f rayDirection = mouseWorld.Normalized();

                const Ray ray { m_camera->GetTranslation(), rayDirection.GetXYZ() };

                RayTestResults results;

                EditorManipulationWidgetBase& manipulationWidget = m_manipulationWidgetHolder.GetSelectedManipulationWidget();
                bool hitManipulationWidget = false;

                if (manipulationWidget.GetNode()->TestRay(ray, results, RTF_USE_BVH | RTF_EDITOR_PICK))
                {
                    for (const RayHit& rayHit : results)
                    {
                        ObjId<Entity> entityId = ObjId<Entity>(ObjIdBase { TypeId::ForType<Entity>(), rayHit.id });

                        if (!entityId.IsValid())
                        {
                            continue;
                        }

                        Handle<Entity> entity { entityId };
                        Assert(entity.IsValid());

                        if (entity.Get() == m_hoveredManipulationWidgetNode.GetUnsafe())
                        {
                            return UIEventHandlerResult::STOP_BUBBLING;
                        }

                        if (manipulationWidget.OnMouseHover(m_camera, event, Handle<Node>(entity)))
                        {
                            SetHoveredManipulationWidget(event, &manipulationWidget, Handle<Node>(entity));

                            return UIEventHandlerResult::STOP_BUBBLING;
                        }
                    }
                }

                SetHoveredManipulationWidget(event, nullptr, Handle<Node>::empty);
            }

            return UIEventHandlerResult::OK;
        }));

    m_delegateHandlers.Remove(&uiImage->OnMouseDown);
    m_delegateHandlers.Add(uiImage->OnMouseDown.Bind([this, uiImageWeak = uiImage.ToWeak()](const MouseEvent& event)
        {
            m_shouldCancelNextClick = false;

            if (IsHoveringManipulationWidget())
            {
                Handle<EditorManipulationWidgetBase> manipulationWidget = m_hoveredManipulationWidget.Lock();
                Handle<Node> node = m_hoveredManipulationWidgetNode.Lock();

                if (!manipulationWidget || !node)
                {
                    HYP_LOG(Editor, Warning, "Failed to lock hovered manipulation widget or node");

                    return UIEventHandlerResult::ERR;
                }

                if (!manipulationWidget->IsDragging())
                {
                    const Vec4f mouseWorld = m_camera->TransformScreenToWorld(event.position);
                    const Vec4f rayDirection = mouseWorld.Normalized();

                    const Ray ray { m_camera->GetTranslation(), rayDirection.GetXYZ() };

                    RayTestResults results;

                    if (node->TestRay(ray, results, RTF_USE_BVH | RTF_EDITOR_PICK))
                    {
                        for (const RayHit& rayHit : results)
                        {
                            manipulationWidget->OnDragStart(m_camera, event, node, rayHit.hitpoint);
                        }
                    }
                }

                return UIEventHandlerResult::STOP_BUBBLING;
            }

            m_camera->GetCameraController()->GetInputHandler()->OnMouseDown(event);

            return UIEventHandlerResult::STOP_BUBBLING;
        }));

    m_delegateHandlers.Remove(&uiImage->OnMouseUp);
    m_delegateHandlers.Add(uiImage->OnMouseUp.Bind([this](const MouseEvent& event)
        {
            m_shouldCancelNextClick = false;

            if (IsHoveringManipulationWidget())
            {
                Handle<EditorManipulationWidgetBase> manipulationWidget = m_hoveredManipulationWidget.Lock();
                Handle<Node> node = m_hoveredManipulationWidgetNode.Lock();

                if (!manipulationWidget || !node)
                {
                    HYP_LOG(Editor, Warning, "Failed to lock hovered manipulation widget or node");

                    return UIEventHandlerResult::ERR;
                }

                if (manipulationWidget->IsDragging())
                {
                    manipulationWidget->OnDragEnd(m_camera, event, Handle<Node>(node));
                }

                return UIEventHandlerResult::STOP_BUBBLING;
            }

            m_camera->GetCameraController()->GetInputHandler()->OnMouseUp(event);

            return UIEventHandlerResult::STOP_BUBBLING;
        }));

    m_delegateHandlers.Remove(&uiImage->OnKeyDown);
    m_delegateHandlers.Add(uiImage->OnKeyDown.Bind([this](const KeyboardEvent& event)
        {
            // On escape press, stop simulating if we're currently simulating
            if (event.keyCode == KeyCode::ESC && GetWorld()->GetGameState().IsSimulating())
            {
                GetWorld()->StopSimulating();

                return UIEventHandlerResult::STOP_BUBBLING;
            }

            if (m_focusedNode.IsValid())
            {
                if (m_manipulationWidgetHolder.GetManipulationWidget(EditorManipulationMode::TRANSLATE).OnKeyPress(m_camera, event, m_focusedNode.Lock()))
                {
                    return UIEventHandlerResult::STOP_BUBBLING;
                }
            }

            if (m_camera->GetCameraController()->GetInputHandler()->OnKeyDown(event))
            {
                return UIEventHandlerResult::STOP_BUBBLING;
            }

            return UIEventHandlerResult::OK;
        }));

    m_delegateHandlers.Remove(&uiImage->OnKeyUp);
    m_delegateHandlers.Add(uiImage->OnKeyUp.Bind([this](const KeyboardEvent& event)
        {
            if (m_camera->GetCameraController()->GetInputHandler()->OnKeyUp(event))
            {
                return UIEventHandlerResult::STOP_BUBBLING;
            }

            return UIEventHandlerResult::OK;
        }));

    m_delegateHandlers.Remove(&uiImage->OnGainFocus);
    m_delegateHandlers.Add(uiImage->OnGainFocus.Bind([this](const MouseEvent& event)
        {
            m_editorCameraEnabled = true;

            return UIEventHandlerResult::OK;
        }));

    m_delegateHandlers.Remove(&uiImage->OnLoseFocus);
    m_delegateHandlers.Add(uiImage->OnLoseFocus.Bind([this](const MouseEvent& event)
        {
            m_editorCameraEnabled = false;

            return UIEventHandlerResult::OK;
        }));

    uiImage->SetTexture(readbackTexture);

    InitConsoleUI();
    InitDebugOverlays();
    InitManipulationWidgetSelection();
}

void EditorSubsystem::InitSceneOutline()
{
    UISubsystem* uiSubsystem = GetWorld()->GetSubsystem<UISubsystem>();
    AssertDebug(uiSubsystem != nullptr);

    Handle<UIListView> listView = ObjCast<UIListView>(uiSubsystem->GetUIStage()->FindChildUIObject(NAME("Outline_ListView")));
    AssertDebug(listView.IsValid());

    listView->SetInnerSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    listView->SetDataSource(CreateObject<UIDataSource>(TypeWrapper<WeakHandle<Node>> {}));

    if (g_showOnlyActiveScene)
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

            HYP_LOG(Editor, Debug, "Selected item changed: {}", listViewItem != nullptr ? listViewItem->GetName() : Name());

            if (!listViewItem)
            {
                SetFocusedNode(Handle<Node>::empty, false);

                return UIEventHandlerResult::OK;
            }

            const Uuid dataSourceElementUuid = listViewItem->GetDataSourceElementUUID();

            if (dataSourceElementUuid == Uuid::Invalid())
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

static void AddNodeToSceneOutline(const Handle<UIListView>& listView, Node* node)
{
    Assert(node != nullptr);

    if (node->GetNodeFlags() & NodeFlags::HIDE_IN_SCENE_OUTLINE)
    {
        return;
    }

    if (!listView)
    {
        return;
    }

    if (UIDataSourceBase* dataSource = listView->GetDataSource())
    {
        WeakHandle<Node> editorNodeWeak = MakeWeakRef(node);

        Uuid parentNodeUuid = Uuid::Invalid();

        if (Node* parentNode = node->GetParent())
        {
            parentNodeUuid = parentNode->GetUUID();
        }

        dataSource->Push(node->GetUUID(), HypData(std::move(editorNodeWeak)), parentNodeUuid);
    }

    for (Node* child : node->GetChildren())
    {
        if (child->GetNodeFlags() & NodeFlags::HIDE_IN_SCENE_OUTLINE)
        {
            continue;
        }

        AddNodeToSceneOutline(listView, child);
    }
};

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

    HYP_LOG(Editor, Debug, "Start watching node: {}", *node->GetName());

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
    //            HYP_LOG(Editor, Debug, "Node {} property changed : {}", *node->GetName(), *property->GetName());
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

    AddNodeToSceneOutline(listView, node.Get());

    m_delegateHandlers.Remove(&node->GetDelegates()->OnChildAdded);
    m_delegateHandlers.Add(node->GetDelegates()->OnChildAdded.Bind([this, listViewWeak = listView.ToWeak()](Node* node, bool isDirect)
        {
            Assert(node != nullptr);

            if (node->GetNodeFlags() & NodeFlags::HIDE_IN_SCENE_OUTLINE)
            {
                return;
            }

            Handle<UIListView> listView = listViewWeak.Lock();

            AddNodeToSceneOutline(listView, node);
            if (isDirect)
                HYP_LOG(Editor, Debug, "Added to scene outline: {}\tparent: {}", node->GetName(), (node->GetParent() ? node->GetParent()->GetUUID() : Uuid::Invalid()));
        }));

    m_delegateHandlers.Remove(&node->GetDelegates()->OnChildRemoved);
    m_delegateHandlers.Add(node->GetDelegates()->OnChildRemoved.Bind([this, listViewWeak = listView.ToWeak()](Node* node, bool)
        {
            // If the node being removed is the focused node, clear the focused node
            if (node == m_focusedNode.GetUnsafe())
            {
                SetFocusedNode(Handle<Node>::empty, true);
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

            if (UIDataSourceBase* dataSource = listView->GetDataSource())
            {
                dataSource->Remove(node->GetUUID());
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

    if (const Handle<UIDataSourceBase>& dataSource = listView->GetDataSource())
    {
        dataSource->Remove(node->GetUUID());
    }

    // Keep ref alive to node to prevent it from being destroyed while we're removing the watchers
    Handle<Node> nodeCopy = node;

    m_delegateHandlers.Remove(&node->GetDelegates()->OnChildAdded);
    m_delegateHandlers.Remove(&node->GetDelegates()->OnChildRemoved);

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

    m_editorDelegates->RemoveNodeWatchers("SceneView");
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

    if (GetWorld()->GetGameState().IsEditor())
    {
        if (g_showOnlyActiveScene)
        {
            if (Handle<Scene> activeScene = m_activeScene.Lock())
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

    m_editorDelegates->RemoveNodeWatchers("DetailView");

    m_delegateHandlers.Remove(&OnFocusedNodeChanged);
    m_delegateHandlers.Add(OnFocusedNodeChanged.Bind([this, cls = Node::StaticClass(), detailsListViewWeak = detailsListView.ToWeak(), outlineListViewWeak = outlineListView.ToWeak()](const Handle<Node>& node, const Handle<Node>& previousNode, bool shouldSelectInOutline)
        {
            m_editorDelegates->RemoveNodeWatchers("DetailView");

            if (shouldSelectInOutline)
            {
                if (Handle<UIListView> outlineListView = outlineListViewWeak.Lock())
                {
                    if (node.IsValid())
                    {
                        if (UIListViewItem* outlineListViewItem = outlineListView->FindListViewItem(node->GetUUID()))
                        {
                            outlineListView->SetSelectedItem(outlineListViewItem);
                        }
                    }
                    else
                    {
                        outlineListView->SetSelectedItem(nullptr);
                    }
                }
            }

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

            detailsListView->SetDataSource(CreateObject<UIDataSource>(TypeWrapper<EditorNodePropertyRef> {}));

            UIDataSourceBase* dataSource = detailsListView->GetDataSource();

            HashMap<String, Property*> propertiesByName;

            for (auto it = cls->GetMembers(HypMemberType::TYPE_PROPERTY).Begin(); it != cls->GetMembers(HypMemberType::TYPE_PROPERTY).End(); ++it)
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

                    propertiesByName[property->GetName().LookupString()] = property;
                }
                else
                {
                    HYP_UNREACHABLE();
                }
            }

            for (auto& it : propertiesByName)
            {
                EditorNodePropertyRef nodePropertyRef;
                nodePropertyRef.node = node.ToWeak();
                nodePropertyRef.property = it.second;

                if (const ClassAttributeValue& attr = it.second->GetAttribute("label"))
                {
                    nodePropertyRef.title = attr.GetString();
                }
                else
                {
                    nodePropertyRef.title = it.first;
                }

                if (const ClassAttributeValue& attr = it.second->GetAttribute("description"))
                {
                    nodePropertyRef.description = attr.GetString();
                }

                dataSource->Push(Uuid(), HypData(std::move(nodePropertyRef)));
            }

            m_editorDelegates->AddNodeWatcher(
                NAME("DetailView"),
                node,
                {},
                Proc<void(Node*, const Property*)> {
                    [this, cls = Node::StaticClass(), detailsListViewWeak](Node* node, const Property* property)
                    {
                        HYP_LOG(Editor, Debug, "(detail) Node property changed: {}", property->GetName());

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

void EditorSubsystem::InitConsoleUI()
{
    HYP_SCOPE;

    UISubsystem* uiSubsystem = GetWorld()->GetSubsystem<UISubsystem>();
    AssertDebug(uiSubsystem != nullptr);

    if (m_consoleUi != nullptr)
    {
        m_consoleUi->RemoveFromParent();
    }

    m_consoleUi = uiSubsystem->GetUIStage()->CreateUIObject<ConsoleUI>(NAME("Console"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::FILL }, { 100, UIObjectSize::PIXEL }));
    AssertDebug(m_consoleUi.IsValid());

    m_consoleUi->SetDepth(150);
    m_consoleUi->SetIsVisible(false);

    if (Handle<UIObject> sceneImageObject = uiSubsystem->GetUIStage()->FindChildUIObject(NAME("Scene_Image")))
    {
        Handle<UIImage> sceneImage = ObjCast<UIImage>(sceneImageObject);
        AssertDebug(sceneImage.IsValid());

        sceneImage->AddChildUIObject(m_consoleUi);
    }
    else
    {
        HYP_FAIL("Failed to find Scene_Image element; cannot add console UI");
    }
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

    if (Handle<UIImage> sceneImage = ObjCast<UIImage>(uiSubsystem->GetUIStage()->FindChildUIObject(NAME("Scene_Image"))))
    {
        for (const Handle<UIObject>& debugOverlayContainer : m_debugOverlayContainers)
        {
            sceneImage->AddChildUIObject(debugOverlayContainer);
        }
    }
}

void EditorSubsystem::InitManipulationWidgetSelection()
{
    HYP_SCOPE;

    UISubsystem* uiSubsystem = GetWorld()->GetSubsystem<UISubsystem>();
    Assert(uiSubsystem != nullptr);

    Handle<UIObject> manipulationWidgetSelection = uiSubsystem->GetUIStage()->FindChildUIObject(NAME("ManipulationWidgetSelection"));
    if (manipulationWidgetSelection != nullptr)
    {
        manipulationWidgetSelection->RemoveFromParent();
    }

    manipulationWidgetSelection = uiSubsystem->GetUIStage()->CreateUIObject<UIMenuBar>(NAME("ManipulationWidgetSelection"), Vec2i { 0, 0 }, UIObjectSize({ 80, UIObjectSize::PIXEL }, { 12, UIObjectSize::PIXEL }));
    Assert(manipulationWidgetSelection != nullptr);

    manipulationWidgetSelection->SetDepth(150);
    manipulationWidgetSelection->SetTextSize(8.0f);
    manipulationWidgetSelection->SetBackgroundColor(Color(0.0f, 0.0f, 0.0f, 0.5f));
    manipulationWidgetSelection->SetTextColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
    manipulationWidgetSelection->SetBorderFlags(UIObjectBorderFlags::ALL);
    manipulationWidgetSelection->SetBorderRadius(5.0f);
    manipulationWidgetSelection->SetPosition(Vec2i { 5, 5 });

    Array<Pair<int, Handle<UIObject>>> manipulationWidgetMenuItems;
    manipulationWidgetMenuItems.Reserve(m_manipulationWidgetHolder.GetManipulationWidgets().Size());

    // add each manipulation widget to the selection menu
    for (const Handle<EditorManipulationWidgetBase>& manipulationWidget : m_manipulationWidgetHolder.GetManipulationWidgets())
    {
        if (manipulationWidget->GetManipulationMode() == EditorManipulationMode::NONE)
        {
            continue;
        }

        Handle<UIObject> manipulationWidgetMenuItem = manipulationWidgetSelection->CreateUIObject<UIMenuItem>(manipulationWidget->InstanceClass()->GetName(), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::FILL }, { 100, UIObjectSize::PIXEL }));
        Assert(manipulationWidgetMenuItem != nullptr);

        manipulationWidgetMenuItem->SetText(manipulationWidget->GetMenuText());

        auto it = std::lower_bound(manipulationWidgetMenuItems.Begin(), manipulationWidgetMenuItems.End(), manipulationWidget->GetPriority(), [](const Pair<int, Handle<UIObject>>& a, int b)
            {
                return a.first < b;
            });

        manipulationWidgetMenuItems.Insert(it, Pair<int, Handle<UIObject>> { manipulationWidget->GetPriority(), std::move(manipulationWidgetMenuItem) });
    }

    for (Pair<int, Handle<UIObject>>& manipulationWidgetMenuItem : manipulationWidgetMenuItems)
    {
        manipulationWidgetSelection->AddChildUIObject(std::move(manipulationWidgetMenuItem.second));
    }

    if (Handle<UIObject> sceneImageObject = uiSubsystem->GetUIStage()->FindChildUIObject(NAME("Scene_Image")))
    {
        Handle<UIImage> sceneImage = ObjCast<UIImage>(sceneImageObject);
        Assert(sceneImage != nullptr);

        sceneImage->AddChildUIObject(manipulationWidgetSelection);
    }
    else
    {
        HYP_FAIL("Failed to find Scene_Image element; cannot add manipulation widget selection UI");
    }
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

    if (Handle<Scene> activeScene = m_activeScene.Lock())
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
    //                             HYP_LOG(Editor, Debug, "Checking scene menu item with tag: {}", tag.ToString());

    //                             if (tag.IsValid())
    //                             {
    //                                 const Uuid* uuid = tag.data.TryGet<Uuid>();

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

    //             activeSceneMenuItem->SetSelectedSubItem(Handle<UIMenuItem>::empty);
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

        sceneMenuItem->SetNodeTag(NodeTag(NAME("Scene"), scene->GetUUID()));

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

                    if (Handle<UIMenuItem> activeSceneMenuItem = activeSceneMenuItemWeak.Lock())
                    {
                        if (Handle<UIMenuItem> sceneMenuItem = sceneMenuItemWeak.Lock())
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

void EditorSubsystem::InitContentBrowser()
{
    HYP_SCOPE;

    UISubsystem* uiSubsystem = GetWorld()->GetSubsystem<UISubsystem>();
    Assert(uiSubsystem != nullptr);

    m_contentBrowserDirectoryList = ObjCast<UIListView>(uiSubsystem->GetUIStage()->FindChildUIObject("ContentBrowser_Directory_List"));
    Assert(m_contentBrowserDirectoryList != nullptr);

    m_contentBrowserDirectoryList->SetDataSource(CreateObject<UIDataSource>(TypeWrapper<AssetPackage> {}));

    m_delegateHandlers.Remove(NAME("SelectContentDirectory"));
    m_delegateHandlers.Add(
        NAME("SelectContentDirectory"),
        m_contentBrowserDirectoryList->OnSelectedItemChange
            .Bind([this](UIListViewItem* listViewItem)
                {
                    if (listViewItem != nullptr)
                    {
                        if (const NodeTag& assetPackageTag = listViewItem->GetNodeTag("AssetPackage"); assetPackageTag.IsValid())
                        {
                            if (Handle<AssetPackage> assetPackage = g_assetManager->GetAssetRegistry()->GetPackageFromPath(assetPackageTag.ToString(), /* createIfNotExist */ false))
                            {
                                SetSelectedPackage(assetPackage);

                                return;
                            }
                            else
                            {
                                HYP_LOG(Editor, Error, "Failed to get asset package from path: {}", assetPackageTag.ToString());
                            }
                        }
                    }

                    SetSelectedPackage(Handle<AssetPackage>::empty);
                }));

    m_contentBrowserContents = ObjCast<UIGrid>(uiSubsystem->GetUIStage()->FindChildUIObject("ContentBrowser_Contents"));
    Assert(m_contentBrowserContents != nullptr);

    // create data source that handles AssetObject and AssetPackage (so we display as subfolders)
    m_contentBrowserContents->SetDataSource(CreateObject<UIDataSource>(TypeWrapper<AssetObject> {}, TypeWrapper<AssetPackage> {}));
    m_contentBrowserContents->SetIsVisible(false);

    m_contentBrowserContentsEmpty = uiSubsystem->GetUIStage()->FindChildUIObject("ContentBrowser_Contents_Empty");
    Assert(m_contentBrowserContentsEmpty != nullptr);
    m_contentBrowserContentsEmpty->SetIsVisible(true);

    Handle<UIObject> importButton = uiSubsystem->GetUIStage()->FindChildUIObject("ContentBrowser_Import_Button");
    Assert(importButton != nullptr);

    importButton->OnClick.RemoveAllDetached();

    m_delegateHandlers.Remove(NAME("ImportClicked"));
    m_delegateHandlers.Add(
        NAME("ImportClicked"),
        importButton->OnClick
            .Bind([this](...)
                {
                    ShowImportContentDialog();

                    return UIEventHandlerResult::STOP_BUBBLING;
                }));
}

void EditorSubsystem::AddPackageToContentBrowser(const Handle<AssetPackage>& package, bool nested)
{
    HYP_SCOPE;

    Assert(package.IsValid());

    HYP_LOG(Editor, Debug, "Adding package to content browser: {}", package->GetName());

    if (package->IsHidden())
    {
        return;
    }

    if (UIDataSourceBase* dataSource = m_contentBrowserDirectoryList->GetDataSource())
    {
        Handle<AssetPackage> parentPackage = package->GetParentPackage().Lock();

        Uuid parentPackageUuid = parentPackage.IsValid() ? parentPackage->GetUUID() : Uuid::Invalid();

        dataSource->Push(package->GetUUID(), HypData(package), parentPackageUuid);
    }

    if (nested)
    {
        package->ForEachSubpackage([this](const Handle<AssetPackage>& subpackage)
            {
                if (subpackage->IsHidden())
                {
                    return IterationResult::CONTINUE;
                }

                AddPackageToContentBrowser(subpackage, true);

                return IterationResult::CONTINUE;
            });
    }
}

void EditorSubsystem::RemovePackageFromContentBrowser(AssetPackage* package)
{
    HYP_SCOPE;

    if (!package)
    {
        return;
    }

    if (m_selectedPackage == package)
    {
        SetSelectedPackage(nullptr);
    }

    if (UIDataSourceBase* dataSource = m_contentBrowserDirectoryList->GetDataSource())
    {
        if (!dataSource->Remove(package->GetUUID()))
        {
            return;
        }
    }

    // Remove all subpackages
    package->ForEachSubpackage([this](const Handle<AssetPackage>& subpackage)
        {
            RemovePackageFromContentBrowser(subpackage.Get());

            return IterationResult::CONTINUE;
        });
}

void EditorSubsystem::SetSelectedPackage(const Handle<AssetPackage>& package)
{
    HYP_SCOPE;

    if (m_selectedPackage == package)
    {
        return;
    }

    m_delegateHandlers.Remove(NAME("OnAssetObjectAdded"));
    m_delegateHandlers.Remove(NAME("OnAssetObjectRemoved"));

    m_selectedPackage = package;

    m_contentBrowserContents->GetDataSource()->Clear();

    if (package.IsValid())
    {
        m_delegateHandlers.Add(
            NAME("OnAssetObjectAdded"),
            package->OnAssetObjectAdded.BindThreaded([this](Handle<AssetObject> assetObject, bool isDirect)
                {
                    if (!isDirect)
                    {
                        return;
                    }

                    m_contentBrowserContents->GetDataSource()->Push(assetObject->GetUUID(), HypData(assetObject));
                },
                g_gameThread));

        m_delegateHandlers.Add(
            NAME("OnAssetObjectRemoved"),
            package->OnAssetObjectRemoved.BindThreaded([this](Handle<AssetObject> assetObject, bool isDirect)
                {
                    if (!isDirect)
                    {
                        return;
                    }

                    m_contentBrowserContents->GetDataSource()->Remove(assetObject->GetUUID());
                },
                g_gameThread));

        package->ForEachAssetObject([&](const Handle<AssetObject>& assetObject)
            {
                m_contentBrowserContents->GetDataSource()->Push(assetObject->GetUUID(), HypData(assetObject));

                return IterationResult::CONTINUE;
            });

        m_delegateHandlers.Add(
            NAME("OnSubpackageAdded"),
            package->OnSubpackageAdded.BindThreaded([this](const Handle<AssetPackage>& subpackage)
                {
                    m_contentBrowserContents->GetDataSource()->Push(subpackage->GetUUID(), HypData(subpackage));
                },
                g_gameThread));

        m_delegateHandlers.Add(
            NAME("OnSubpackageRemoved"),
            package->OnSubpackageRemoved.BindThreaded([this](const Handle<AssetPackage>& subpackage)
                {
                    m_contentBrowserContents->GetDataSource()->Remove(subpackage->GetUUID());
                },
                g_gameThread));

        g_assetManager->GetAssetRegistry()->LoadSubpackages(package, /* recursive */ false);

        package->ForEachSubpackage([this](const Handle<AssetPackage>& subpackage)
            {
                if (subpackage->IsHidden())
                {
                    return IterationResult::CONTINUE;
                }

                m_contentBrowserContents->GetDataSource()->Push(subpackage->GetUUID(), HypData(subpackage));

                return IterationResult::CONTINUE;
            });
    }

    HYP_LOG(Editor, Debug, "Num assets in package: {}", m_contentBrowserContents->GetDataSource()->Size());

    if (m_contentBrowserContents->GetDataSource()->Size() == 0)
    {
        m_contentBrowserContents->SetIsVisible(false);
        m_contentBrowserContentsEmpty->SetIsVisible(true);
    }
    else
    {
        m_contentBrowserContents->SetIsVisible(true);
        m_contentBrowserContentsEmpty->SetIsVisible(false);
    }
}

TResult<Handle<FontAtlas>> EditorSubsystem::CreateFontAtlas()
{
    HYP_SCOPE;

    const FilePath outputDirectory = GetResourceDirectory() / "data" / "fonts";
    const FilePath outputFilePath = outputDirectory / "Roboto.hyp";

    if (!outputDirectory.Exists())
    {
        outputDirectory.MkDir();
    }

    // if (serializedFilePath.Exists())
    // {
    //     HypData loadedFontAtlasData;

    //     FBOMReader reader({});

    //     if (FBOMResult err = reader.LoadFromFile(serializedFilePath, loadedFontAtlasData))
    //     {
    //         return HYP_MAKE_ERROR(Error, "Failed to load font atlas from file: {}", 0, err.message);
    //     }

    //     return loadedFontAtlasData.Get<Handle<FontAtlas>>();
    // }

    auto fontFaceAsset = AssetManager::GetInstance()->Load<RC<FontFace>>("fonts/Roboto/Roboto-Regular.ttf");

    if (fontFaceAsset.HasError())
    {
        return HYP_MAKE_ERROR(Error, "Failed to load font face! Error: {}", 0, fontFaceAsset.GetError().GetMessage());
    }

    Handle<AssetPackage> package = g_assetManager->GetAssetRegistry()->GetPackageFromPath("Engine/Media/Fonts/Roboto", /* createIfNotExist */ true);
    Assert(package.IsValid());

    Handle<FontAtlas> atlas = CreateObject<FontAtlas>(std::move(fontFaceAsset->Result()));

    if (Result renderAtlasResult = atlas->RenderAtlasTextures(); renderAtlasResult.HasError())
    {
        return renderAtlasResult.GetError();
    }

    // register all textures within the atlas as assets:
    Array<Task<Result>> futureResults;
    for (const auto& it : atlas->GetAtlasTextures().atlases)
    {
        const Handle<Texture>& texture = it.second;
        Assert(texture != nullptr);

        const Handle<TextureAsset>& textureAsset = texture->GetAsset();
        Assert(textureAsset != nullptr);

        HYP_LOG(Font, Debug, "Adding texture {} to package", texture->GetName());

        futureResults.PushBack(package->AddAssetObject(textureAsset));
    }

    AwaitAll(futureResults.ToSpan());

    for (const Task<Result>& futureResult : futureResults)
    {
        if (const Result& result = futureResult.Await(); result.HasError())
        {
            HYP_LOG(Editor, Error, "Failed to add texture asset to package: {}", result.GetError().GetMessage());
        }
    }

    /*if (Result savePackageResult = package->Save(outputDirectory); savePackageResult.HasError())
    {
        return savePackageResult.GetError();
    }*/

    /*FileByteWriter byteWriter { outputFilePath };
    FBOMWriter writer { FBOMWriterConfig {} };
    writer.Append(*atlas);
    auto writeErr = writer.Emit(&byteWriter);
    byteWriter.Close();

    if (writeErr != FBOMResult::FBOM_OK)
    {
        return HYP_MAKE_ERROR(Error, "Failed to save font atlas! {}", 0, writeErr.message);
    }*/

    // @TODO: Add FontAtlas asset to package.

    // need to move in return since return type is wrapped result
    return std::move(atlas);
}

void EditorSubsystem::NewProject()
{
    OpenProject(CreateObject<EditorProject>());
}

void EditorSubsystem::OpenProject(const Handle<EditorProject>& project)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    if (project == m_currentProject)
    {
        return;
    }

    if (m_currentProject)
    {
        OnProjectClosing(m_currentProject);

        m_currentProject->SetEditorSubsystem(WeakHandle<EditorSubsystem>::Null());
        m_currentProject->Close();
    }

    if (project)
    {
        if (project.IsValid())
        {
            project->SetEditorSubsystem(MakeWeakRef(this));
        }

        InitObject(project);

        m_currentProject = project;

        // if (Result saveResult = project->Save(); saveResult.HasError())
        // {
        //     HYP_LOG(Editor, Error, "Failed to save newly created project: {}", saveResult.GetError().GetMessage());
        // }

        OnProjectOpened(m_currentProject);

        g_editorState->SetCurrentProject(m_currentProject);
    }
}

void EditorSubsystem::ShowOpenProjectDialog()
{
    HYP_SCOPE;

    ShowOpenFileDialog(
        "Select the project to open",
        GetResourceDirectory(),
        { "hypproj" },
        /* allowMultiple */ false, /* allowDirectories */ true,
        [weakThis = MakeWeakRef(this)](TResult<Array<FilePath>>&& result) mutable
        {
            if (result.HasError())
            {
                HYP_LOG(Editor, Error, "Failed to select project file: {}", result.GetError().GetMessage());
                return;
            }

            if (result.GetValue().Empty())
            {
                HYP_LOG(Editor, Warning, "No project file selected.");
                return;
            }

            GetThreadById(g_gameThread)->GetScheduler().Enqueue([weakThis = std::move(weakThis), projectFilepath = std::move(result.GetValue()[0])]() mutable
                {
                    Handle<EditorSubsystem> strongThis = weakThis.Lock();
                    if (!strongThis)
                    {
                        HYP_LOG(Editor, Error, "Failed to lock EditorSubsystem from weak reference in ShowOpenProjectDialog");
                        return;
                    }

                    TResult<Handle<EditorProject>> loadProjectResult = EditorProject::Load(projectFilepath);

                    if (loadProjectResult.HasError())
                    {
                        HYP_LOG(Editor, Error, "Failed to load project: {}", loadProjectResult.GetError().GetMessage());
                        return;
                    }

                    Handle<EditorProject> project = loadProjectResult.GetValue();

                    if (!project.IsValid())
                    {
                        HYP_LOG(Editor, Error, "Loaded project is invalid.");
                        return;
                    }

                    strongThis->OpenProject(project);
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);
        });
}

void EditorSubsystem::ShowImportContentDialog()
{
    HYP_SCOPE;

    ShowOpenFileDialog(
        "Select the file(s) to import into the project",
        GetResourceDirectory(),
        { "obj", "jpg", "jpeg", "png", "tga", "bmp", "ogre.xml" },
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
            RC<AssetBatch> batch = AssetManager::GetInstance()->CreateBatch(identifier);

            for (const FilePath& file : result.GetValue())
            {
                batch->Add(file.Basename(), FilePath::Relative(file, CoreApi_GetExecutablePath()));
            }

            batch->OnComplete
                .Bind([](AssetMap& results)
                    {
                        HYP_LOG(Editor, Info, "{} assets loaded.", results.Size());

                        // @TODO Open folder the assets ended up in
                    })
                .Detach();

            batch->LoadAsync();
        });
}

void EditorSubsystem::AddTask(const Handle<EditorTaskBase>& task)
{
    HYP_SCOPE;

    if (!task)
    {
        return;
    }

    AssertOnThread(g_gameThread);

    m_taskManager.AddTask(task);
}

void EditorSubsystem::SetFocusedNode(const Handle<Node>& focusedNode, bool shouldSelectInOutline)
{
    if (focusedNode == m_focusedNode)
    {
        return;
    }

    const Handle<Node> previousFocusedNode = m_focusedNode.Lock();

    m_focusedNode = focusedNode;

    if (Handle<Node> focusedNode = m_focusedNode.Lock())
    {
        if (focusedNode->GetScene() != nullptr)
        {
            if (const Handle<Entity>& entity = ObjCast<Entity>(focusedNode))
            {
                entity->AddTag<EntityTag::EDITOR_FOCUSED>();
            }
        }

        // @TODO watch for transform changes and update the highlight node

        // m_scene->GetRoot()->AddChild(m_highlightNode);
        // m_highlightNode->SetWorldScale(m_focusedNode->GetWorldAABB().GetExtent() * 0.5f);
        // m_highlightNode->SetWorldTranslation(m_focusedNode->GetWorldTranslation());

        // HYP_LOG(Editor, Debug, "Set focused node: {}\t{}", m_focusedNode->GetName(), m_focusedNode->GetWorldTranslation());
        // HYP_LOG(Editor, Debug, "Set highlight node translation: {}", m_highlightNode->GetWorldTranslation());

        if (m_manipulationWidgetHolder.GetSelectedManipulationMode() == EditorManipulationMode::NONE)
        {
            m_manipulationWidgetHolder.SetSelectedManipulationMode(EditorManipulationMode::TRANSLATE);
        }

        EditorManipulationWidgetBase& manipulationWidget = m_manipulationWidgetHolder.GetSelectedManipulationWidget();

        manipulationWidget.UpdateWidget(focusedNode);
    }

    if (previousFocusedNode != nullptr)
    {
        if (const Handle<Entity>& entity = ObjCast<Entity>(previousFocusedNode))
        {
            entity->RemoveTag<EntityTag::EDITOR_FOCUSED>();
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

    AssertOnThread(g_gameThread);

    UISubsystem* uiSubsystem = GetWorld()->GetSubsystem<UISubsystem>();
    Assert(uiSubsystem != nullptr);

    auto it = m_debugOverlays.FindIf([name = debugOverlay->GetName()](const auto& item)
        {
            return item->GetName() == name;
        });

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

        placement = 0; // Default to the first corner
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

    HYP_LOG(Editor, Debug, "Added debug overlay: {}", debugOverlay->GetName());
}

bool EditorSubsystem::RemoveDebugOverlay(StringHash name)
{
    HYP_SCOPE;

    AssertOnThread(g_gameThread);

    auto it = m_debugOverlays.FindIf([name](const auto& item)
        {
            return item->GetName() == name;
        });

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

Handle<Node> EditorSubsystem::GetFocusedNode() const
{
    return m_focusedNode.Lock();
}

Vec3f EditorSubsystem::CalculateSceneInsertionPoint(float desiredDistance, float offsetFromSurface) const
{
    HYP_SCOPE;

    const Vec3f cameraPosition = m_camera->GetTranslation();
    const Vec3f cameraDirection = m_camera->GetDirection();

    Vec3f insertionPoint = cameraPosition + cameraDirection * desiredDistance;

    const Ray ray { cameraPosition, cameraDirection };

    RayTestResults results;

    Handle<Scene> activeScene = m_activeScene.Lock();
    if (!activeScene)
    {
        return insertionPoint;
    }

    // raytest using scene's octree
    if ((activeScene->GetSceneFlags() & SceneFlags::HAS_OCTREE) && activeScene->GetOctree().TestRay(ray, results, RTF_USE_BVH))
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

void EditorSubsystem::UpdateTasks(float delta)
{
    HYP_SCOPE;

    m_taskManager.Tick(delta);
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

void EditorSubsystem::SetHoveredManipulationWidget(
    const MouseEvent& event,
    EditorManipulationWidgetBase* manipulationWidget,
    const Handle<Node>& manipulationWidgetNode)
{
    if (m_hoveredManipulationWidget.IsValid() && m_hoveredManipulationWidgetNode.IsValid())
    {
        Handle<Node> hoveredManipulationWidgetNode = m_hoveredManipulationWidgetNode.Lock();
        Handle<EditorManipulationWidgetBase> hoveredManipulationWidget = m_hoveredManipulationWidget.Lock();

        if (hoveredManipulationWidgetNode && hoveredManipulationWidget)
        {
            hoveredManipulationWidget->OnMouseLeave(m_camera, event, Handle<Node>(hoveredManipulationWidgetNode));
        }
    }

    if (manipulationWidget != nullptr)
    {
        m_hoveredManipulationWidget = MakeWeakRef(manipulationWidget);
    }
    else
    {
        m_hoveredManipulationWidget.Reset();
    }

    m_hoveredManipulationWidgetNode = manipulationWidgetNode;
}

void EditorSubsystem::SetActiveScene(const WeakHandle<Scene>& scene)
{
    HYP_SCOPE;

    if (scene == m_activeScene)
    {
        return;
    }

    m_activeScene = scene;
    HYP_LOG(Editor, Debug, "Set active scene: {}", m_activeScene.IsValid() ? m_activeScene.Lock()->GetName() : Name::Invalid());

    OnActiveSceneChanged(m_activeScene.Lock());
}

#endif

#pragma endregion EditorSubsystem

} // namespace hyperion
