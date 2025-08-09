/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

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
#include <rendering/Texture.hpp>

#include <scene/EntityManager.hpp>
#include <scene/components/NodeLinkComponent.hpp>
#include <scene/components/MeshComponent.hpp>
#include <scene/components/VisibilityStateComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/TransformComponent.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/AssetBatch.hpp>

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

#include <input/InputManager.hpp>

#include <system/AppContext.hpp>
#include <system/OpenFileDialog.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/io/ByteWriter.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/debug/DebugDrawer.hpp>

#include <rendering/font/FontAtlas.hpp>

// temp
#include <rendering/lightmapper/LightmapperSubsystem.hpp>
#include <rendering/lightmapper/LightmapUVBuilder.hpp>

#include <console/ui/ConsoleUI.hpp>

#include <core/math/MathUtil.hpp>

#include <dotnet/Class.hpp>

#include <scripting/ScriptingService.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <util/MeshBuilder.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>
#include <HyperionEngine.hpp>

namespace hyperion {

HYP_DEFINE_LOG_CHANNEL(Editor);

#pragma region RunningEditorTask

Handle<UIObject> RunningEditorTask::CreateUIObject(UIStage* uiStage) const
{
    Assert(uiStage != nullptr);

    Handle<UIPanel> panel = uiStage->CreateUIObject<UIPanel>(NAME("EditorTaskPanel"), Vec2i::Zero(), UIObjectSize({ 100, UIObjectSize::FILL }, { 100, UIObjectSize::FILL }));
    panel->SetBackgroundColor(Color(0xFF0000FF)); // testing

    Handle<UIText> taskTitle = uiStage->CreateUIObject<UIText>(NAME("Task_Title"), Vec2i::Zero(), UIObjectSize(UIObjectSize::AUTO));
    taskTitle->SetTextSize(16.0f);
    taskTitle->SetText(m_task->InstanceClass()->GetName().LookupString());
    panel->AddChildUIObject(taskTitle);

    return panel;
}

#pragma endregion RunningEditorTask

#pragma region GenerateLightmapsEditorTask

GenerateLightmapsEditorTask::GenerateLightmapsEditorTask()
    : m_task(nullptr)
{
}

void GenerateLightmapsEditorTask::Process()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    HYP_LOG(Editor, Info, "Generating lightmaps");

    if (!m_world.IsValid() || !m_scene.IsValid())
    {
        HYP_LOG(Editor, Error, "World or scene not set for GenerateLightmapsEditorTask");

        m_task = nullptr;

        return;
    }

    if (!m_aabb.IsValid() || !m_aabb.IsFinite())
    {
        HYP_LOG(Editor, Error, "Invalid AABB provided for GenerateLightmapsEditorTask");

        m_task = nullptr;

        return;
    }

    LightmapperSubsystem* lightmapperSubsystem = m_world->GetSubsystem<LightmapperSubsystem>();

    if (!lightmapperSubsystem)
    {
        lightmapperSubsystem = m_world->AddSubsystem<LightmapperSubsystem>();
    }

    m_task = lightmapperSubsystem->GenerateLightmaps(m_scene, m_aabb);
}

void GenerateLightmapsEditorTask::Cancel()
{
    if (m_task != nullptr)
    {
        m_task->Cancel();
    }
}

bool GenerateLightmapsEditorTask::IsCompleted() const
{
    return m_task == nullptr || m_task->IsCompleted();
}

void GenerateLightmapsEditorTask::Tick(float delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    if (m_task != nullptr)
    {
        if (m_task->IsCompleted())
        {
            m_task = nullptr;
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
    if (m_node.IsValid())
    {
        return;
    }

    m_node = Load_Internal();

    if (!m_node.IsValid())
    {
        HYP_LOG(Editor, Warning, "Failed to create manipulation widget node for \"{}\"", InstanceClass()->GetName());

        return;
    }

    m_node->UnlockTransform();

    m_node->SetFlags(
        m_node->GetFlags()
        | NodeFlags::HIDE_IN_SCENE_OUTLINE // don't display transform widget in the outline
        | NodeFlags::TRANSIENT             // should not ever be serialized to disk
    );

    SetReady(true);
}

void EditorManipulationWidgetBase::Shutdown()
{
    if (m_node.IsValid())
    {
        // Remove from scene
        m_node->Remove();

        m_node.Reset();
    }

    m_focusedNode.Reset();

    SetReady(false);
}

void EditorManipulationWidgetBase::UpdateWidget(const Handle<Node>& focusedNode)
{
    m_focusedNode = focusedNode;

    if (!focusedNode.IsValid())
    {
        return;
    }

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

    if (!node->IsA<Entity>() || !node->GetScene() || !node->GetScene()->GetEntityManager())
    {
        return;
    }

    MeshComponent* meshComponent = node->GetScene()->GetEntityManager()->TryGetComponent<MeshComponent>(ObjCast<Entity>(node.Get()));

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

    const Ray ray { camera->GetWorldTranslation(), rayDirection.GetXYZ() };

    RayHit planeRayHit;

    if (Optional<RayHit> planeRayHitOpt = ray.TestPlane(dragData.planePoint, dragData.planeNormal))
    {
        planeRayHit = *planeRayHitOpt;
    }
    else
    {
        HYP_LOG_TEMP("Ray plane test returned no hit. plane point : {}, plane normal {}", dragData.planePoint, dragData.planeNormal);
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
    if (!node->IsA<Entity>() || !node->GetScene() || !node->GetScene()->GetEntityManager())
    {
        return false;
    }

    MeshComponent* meshComponent = node->GetScene()->GetEntityManager()->TryGetComponent<MeshComponent>(ObjCast<Entity>(node.Get()));

    if (!meshComponent || !meshComponent->material)
    {
        return false;
    }

    meshComponent->material->SetParameter(
        Material::MATERIAL_KEY_ALBEDO,
        Vec4f(1.0f, 1.0f, 0.0, 1.0));

    return true;
}

bool TranslateEditorManipulationWidget::OnMouseLeave(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node)
{
    if (!node->IsA<Entity>() || !node->GetScene() || !node->GetScene()->GetEntityManager())
    {
        return false;
    }

    MeshComponent* meshComponent = node->GetScene()->GetEntityManager()->TryGetComponent<MeshComponent>(ObjCast<Entity>(node.Get()));

    if (!meshComponent || !meshComponent->material)
    {
        return false;
    }

    if (const NodeTag& tag = node->GetTag("TransformWidgetElementColor"))
    {
        meshComponent->material->SetParameter(
            Material::MATERIAL_KEY_ALBEDO,
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

    if (!node->IsA<Entity>() || !node->GetScene() || !node->GetScene()->GetEntityManager())
    {
        return false;
    }

    if (!m_dragData)
    {
        return false;
    }

    MeshComponent* meshComponent = node->GetScene()->GetEntityManager()->TryGetComponent<MeshComponent>(ObjCast<Entity>(node.Get()));

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

                if (!child->GetScene())
                {
                    HYP_LOG(Editor, Warning, "Manipulation widget child \"{}\" has no scene set", child->GetName());

                    continue;
                }

                EntityManager& entityManager = *child->GetScene()->GetEntityManager();

                Entity* entity = static_cast<Entity*>(child);

                entityManager.RemoveTag<EntityTag::STATIC>(entity);
                entityManager.AddTag<EntityTag::DYNAMIC>(entity);

                VisibilityStateComponent* visibilityState = entityManager.TryGetComponent<VisibilityStateComponent>(entity);

                if (visibilityState)
                {
                    visibilityState->flags |= VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE;
                }
                else
                {
                    entityManager.AddComponent<VisibilityStateComponent>(entity, VisibilityStateComponent { VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE });
                }

                MeshComponent* meshComponent = entityManager.TryGetComponent<MeshComponent>(entity);

                if (!meshComponent)
                {
                    continue;
                }

                MaterialAttributes materialAttributes;
                Material::ParameterTable materialParameters;

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

                // testing
                materialAttributes.stencilFunction = StencilFunction {
                    .passOp = SO_REPLACE,
                    .failOp = SO_REPLACE,
                    .depthFailOp = SO_REPLACE,
                    .compareOp = SCO_ALWAYS,
                    .mask = 0xff,
                    .value = 0x1
                };

                meshComponent->material = MaterialCache::GetInstance()->CreateMaterial(materialAttributes, materialParameters);
                meshComponent->material->SetIsDynamic(true);

                entityManager.AddTag<EntityTag::UPDATE_RENDER_PROXY>(entity);

                child->AddTag(NodeTag(NAME("TransformWidgetElementColor"), Vec4f(meshComponent->material->GetParameter(Material::MATERIAL_KEY_ALBEDO))));
            }

            FileByteWriter byteWriter(GetResourceDirectory() / "models/editor/axis_arrows.hypmodel");
            FBOMWriter writer { FBOMWriterConfig {} };
            writer.Append(*node);

            FBOMResult writeErr = writer.Emit(&byteWriter);

            byteWriter.Close();

            if (writeErr)
            {
                HYP_LOG(Editor, Error, "Failed to write axis arrows to disk: {}", writeErr.message);
            }

            return node;
        }
    }

    HYP_LOG(Editor, Error, "Failed to load axis arrows: {}", result.GetError().GetMessage());

    HYP_BREAKPOINT;

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
    Threads::AssertOnThread(g_gameThread);

    return m_selectedManipulationMode;
}

void EditorManipulationWidgetHolder::SetSelectedManipulationMode(EditorManipulationMode mode)
{
    Threads::AssertOnThread(g_gameThread);

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
    Threads::AssertOnThread(g_gameThread);

    return *m_manipulationWidgets.At(m_selectedManipulationMode);
}

void EditorManipulationWidgetHolder::SetCurrentProject(const WeakHandle<EditorProject>& project)
{
    Threads::AssertOnThread(g_gameThread);

    m_currentProject = project;

    for (auto& it : m_manipulationWidgets)
    {
        it->SetCurrentProject(project);
    }
}

void EditorManipulationWidgetHolder::Initialize()
{
    Threads::AssertOnThread(g_gameThread);

    for (const Handle<EditorManipulationWidgetBase>& widget : m_manipulationWidgets)
    {
        widget->SetEditorSubsystem(m_editorSubsystem);
        widget->SetCurrentProject(m_currentProject);

        InitObject(widget);
    }
}

void EditorManipulationWidgetHolder::Shutdown()
{
    Threads::AssertOnThread(g_gameThread);

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

EditorSubsystem::EditorSubsystem(const Handle<AppContextBase>& appContext)
    : m_appContext(appContext),
      m_editorCameraEnabled(false),
      m_shouldCancelNextClick(false),
      m_manipulationWidgetHolder(this)
{
    m_editorDelegates = new EditorDelegates();

    OnProjectOpened
        .Bind([this](const Handle<EditorProject>& project)
            {
                HYP_LOG(Editor, Info, "Opening project: {}", *project->GetName());

                InitObject(project);

                WeakHandle<Scene> activeScene;

                for (const Handle<Scene>& scene : project->GetScenes())
                {
                    Assert(scene.IsValid());

                    if (!activeScene.IsValid())
                    {
                        activeScene = scene;
                    }

                    GetWorld()->AddScene(scene);

                    m_delegateHandlers.Add(
                        scene->OnRootNodeChanged
                            .Bind([this](const Handle<Node>& newRoot, const Handle<Node>& prevRoot)
                                {
                                    UpdateWatchedNodes();
                                }));
                }

                UpdateWatchedNodes();

                m_delegateHandlers.Add(
                    project->OnSceneAdded.Bind([this, projectWeak = project.ToWeak()](const Handle<Scene>& scene)
                        {
                            Assert(scene.IsValid());

                            Handle<EditorProject> project = projectWeak.Lock();
                            Assert(project.IsValid());

                            HYP_LOG(Editor, Info, "Project {} added scene: {}", *project->GetName(), *scene->GetName());

                            GetWorld()->AddScene(scene);

                            m_delegateHandlers.Add(
                                scene->OnRootNodeChanged
                                    .Bind([this](const Handle<Node>& newRoot, const Handle<Node>& prevRoot)
                                        {
                                            UpdateWatchedNodes();
                                        }));

                            if (!m_activeScene.IsValid())
                            {
                                SetActiveScene(scene);
                            }

                            // reinitialize scene selector on scene add
                            InitActiveSceneSelection();
                        }));

                m_delegateHandlers.Add(
                    project->OnSceneRemoved.Bind([this, projectWeak = project.ToWeak()](const Handle<Scene>& scene)
                        {
                            Assert(scene.IsValid());

                            Handle<EditorProject> project = projectWeak.Lock();
                            Assert(project.IsValid());

                            HYP_LOG(Editor, Info, "Project {} removed scene: {}", *project->GetName(), *scene->GetName());

                            m_delegateHandlers.Remove(&scene->OnRootNodeChanged);

                            StopWatchingNode(scene->GetRoot());

                            GetWorld()->RemoveScene(scene);

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
                            continue;
                        }

                        AddPackageToContentBrowser(package, true);
                    }

                    m_delegateHandlers.Add(
                        NAME("OnPackageAdded"),
                        g_assetManager->GetAssetRegistry()->OnPackageAdded.BindThreaded([this](const Handle<AssetPackage>& package)
                            {
                                Assert(package.IsValid());

                                AddPackageToContentBrowser(package, false);
                            },
                            g_gameThread));

                    m_delegateHandlers.Add(
                        NAME("OnPackageRemoved"),
                        g_assetManager->GetAssetRegistry()->OnPackageRemoved.BindThreaded([this](AssetPackage* package)
                            {
                                RemovePackageFromContentBrowser(package);
                            },
                            g_gameThread));
                }

                m_manipulationWidgetHolder.Initialize();
                m_manipulationWidgetHolder.SetCurrentProject(project);

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
                            case GameStateMode::SIMULATING:
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
                                    world->OnSceneRemoved.Bind([this](World*, const Handle<Scene>& scene)
                                        {
                                            if (!scene.IsValid())
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
                // Shutdown to reinitialize widget holder after project is opened
                m_manipulationWidgetHolder.Shutdown();

                m_focusedNode.Reset();

                if (m_highlightNode.IsValid())
                {
                    m_highlightNode->Remove();
                }

                SetActiveScene(WeakHandle<Scene>::empty);

                for (const Handle<Scene>& scene : project->GetScenes())
                {
                    if (!scene.IsValid())
                    {
                        continue;
                    }

                    HYP_LOG(Editor, Info, "Closing project {} scene: {}", *project->GetName(), *scene->GetName());

                    m_delegateHandlers.Remove(&scene->OnRootNodeChanged);

                    StopWatchingNode(scene->GetRoot());

                    GetWorld()->RemoveScene(scene);
                }

                m_delegateHandlers.Remove(&project->OnSceneAdded);
                m_delegateHandlers.Remove(&project->OnSceneRemoved);

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

    m_editorScene = CreateObject<Scene>(nullptr, SceneFlags::FOREGROUND | SceneFlags::EDITOR);
    m_editorScene->SetName(NAME("EditorScene"));
    GetWorld()->AddScene(m_editorScene);

    m_camera = CreateObject<Camera>();
    m_camera->AddCameraController(CreateObject<EditorCameraController>());
    m_camera->SetName(NAME("EditorCamera"));
    m_camera->SetFlags(CameraFlags::MATCH_WINDOW_SIZE);
    m_camera->SetFOV(70.0f);
    m_camera->SetNear(0.1f);
    m_camera->SetFar(3000.0f);
    InitObject(m_camera);

    m_camera->AddTag<EntityTag::CAMERA_PRIMARY>();

    m_editorScene->GetRoot()->AddChild(m_camera);

    LoadEditorUIDefinitions();

    InitContentBrowser();
    InitViewport();
    InitSceneOutline();
    InitActiveSceneSelection();

    InitDetailView();

    CreateHighlightNode();

    g_engineDriver->GetScriptingService()->OnScriptStateChanged.Bind([](const ManagedScript& script)
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

    // auto result = EditorProject::Load(GetResourceDirectory() / "projects" / "UntitledProject9");

    // if (!result)
    // {
    //     HYP_BREAKPOINT;
    // }

    // OpenProject(*result);

    if (!dotnet::DotNetSystem::GetInstance().IsEnabled())
    {
        AddDebugOverlay(CreateObject<TextEditorDebugOverlay>(".NET runtime is not enabled - editor features may be missing or non-functional", Color::Red()));
    }
}

void EditorSubsystem::OnRemovedFromWorld()
{
    HYP_SCOPE;

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

    Threads::AssertOnThread(g_gameThread | ThreadCategory::THREAD_CATEGORY_TASK);

    m_editorDelegates->Update();

    UpdateCamera(delta);
    UpdateTasks(delta);
    UpdateDebugOverlays(delta);

#if 0
    if (m_focusedNode.IsValid()) {
        g_engineDriver->GetDebugDrawer()->box(m_focusedNode->GetWorldTranslation(), m_focusedNode->GetWorldAABB().GetExtent() + Vec3f(1.0001f), Color(0.0f, 0.0f, 1.0f, 1.0f));
        g_engineDriver->GetDebugDrawer()->box(m_focusedNode->GetWorldTranslation(), m_focusedNode->GetWorldAABB().GetExtent(), Color(1.0f), RenderableAttributeSet(
            MeshAttributes {
                .vertexAttributes = staticMeshVertexAttributes
            },
            MaterialAttributes {
                .bucket             = RB_TRANSLUCENT,
                .fillMode          = FM_FILL,
                .blendFunction     = BlendFunction::None(),
                .flags              = MAF_DEPTH_TEST,
                .stencilFunction   = StencilFunction {
                    .passOp        = SO_REPLACE,
                    .failOp        = SO_REPLACE,
                    .depthFailOp  = SO_REPLACE,
                    .compareOp     = SCO_NEVER,
                    .mask           = 0xFF,
                    .value          = 0x1
                }
            }
        ));
    }
#endif
}

void EditorSubsystem::OnSceneAttached(const Handle<Scene>& scene)
{
    HYP_SCOPE;
}

void EditorSubsystem::OnSceneDetached(const Handle<Scene>& scene)
{
    HYP_SCOPE;
}

void EditorSubsystem::LoadEditorUIDefinitions()
{
    UISubsystem* uiSubsystem = GetWorld()->GetSubsystem<UISubsystem>();
    Assert(uiSubsystem != nullptr);

    TResult<RC<FontAtlas>> fontAtlasResult = CreateFontAtlas();

    if (fontAtlasResult.HasError())
    {
        HYP_FAIL("Failed to create font atlas for editor UI: %s", *fontAtlasResult.GetError().GetMessage());
    }

    uiSubsystem->GetUIStage()->SetDefaultFontAtlas(*fontAtlasResult);

    auto loadedUiAsset = AssetManager::GetInstance()->Load<UIObject>("ui/Editor.Main.ui.xml");
    Assert(loadedUiAsset.HasValue(), "Failed to load editor UI definitions!");

    Handle<UIStage> loadedUi = ObjCast<UIStage>(loadedUiAsset->Result());
    Assert(loadedUi.IsValid(), "Failed to load editor UI");

    loadedUi->SetDefaultFontAtlas(*fontAtlasResult);

    uiSubsystem->GetUIStage()->AddChildUIObject(loadedUi);
}

void EditorSubsystem::CreateHighlightNode()
{
    // m_highlightNode = Handle<Node>(CreateObject<Node>("Editor_Highlight"));
    // m_highlightNode->SetFlags(m_highlightNode->GetFlags() | NodeFlags::HIDE_IN_SCENE_OUTLINE);

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
    //         { Material::MATERIAL_KEY_ALBEDO, Vec4f(1.0f, 1.0f, 0.0f, 1.0f) },
    //         { Material::MATERIAL_KEY_ROUGHNESS, 0.0f },
    //         { Material::MATERIAL_KEY_METALNESS, 0.0f }
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
    //         VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE
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
    for (const Handle<View>& view : m_views)
    {
        GetWorld()->RemoveView(view);
    }

    m_views.Clear();

    UISubsystem* uiSubsystem = GetWorld()->GetSubsystem<UISubsystem>();
    Assert(uiSubsystem != nullptr);

    Handle<UIObject> sceneImageObject = uiSubsystem->GetUIStage()->FindChildUIObject(NAME("Scene_Image"));
    if (!sceneImageObject)
    {
        HYP_LOG(Editor, Error, "Scene image object not found in UI stage!");
        return;
    }

    uiSubsystem->GetUIStage()->UpdateSize(true);

    HYP_LOG(Editor, Debug, "scene surface : {}", uiSubsystem->GetUIStage()->GetSurfaceSize());
    HYP_LOG(Editor, Debug, "scene image object size : {}", sceneImageObject->GetActualSize());

    Vec2u viewportSize = MathUtil::Max(Vec2u(sceneImageObject->GetActualSize()), Vec2u::One());
    ViewDesc viewDesc {
        .flags = ViewFlags::DEFAULT | ViewFlags::GBUFFER | ViewFlags::ENABLE_READBACK,
        .viewport = Viewport { .extent = viewportSize, .position = Vec2i::Zero() },
        .outputTargetDesc = { .extent = viewportSize },
        .camera = m_camera,
        .readbackTextureFormat = TF_R11G11B10F
    };

    Handle<View> view = CreateObject<View>(viewDesc);
    InitObject(view);

    HYP_LOG(Editor, Info, "Creating editor viewport with size: {}", viewportSize);

    GetWorld()->AddView(view);

    m_views.PushBack(view);

    m_delegateHandlers.Remove(&sceneImageObject->OnSizeChange);
    m_delegateHandlers.Add(sceneImageObject->OnSizeChange.Bind([this, sceneImageObjectWeak = sceneImageObject.ToWeak(), viewWeak = view.ToWeak()]()
        {
            Handle<UIObject> sceneImageObject = sceneImageObjectWeak.Lock();
            if (!sceneImageObject)
            {
                HYP_LOG(Editor, Warning, "Scene image object is no longer valid!");
                return UIEventHandlerResult::ERR;
            }

            Handle<View> view = viewWeak.Lock();
            if (!view)
            {
                HYP_LOG(Editor, Warning, "View is no longer valid!");
                return UIEventHandlerResult::ERR;
            }

            Vec2u viewportSize = MathUtil::Max(Vec2u(sceneImageObject->GetActualSize()), Vec2u::One());

            view->SetViewport(Viewport { .extent = viewportSize, .position = Vec2i::Zero() });

            HYP_LOG(Editor, Info, "Main editor view viewport size changed to {}", viewportSize);

            return UIEventHandlerResult::OK;
        }));

    Handle<UIImage> uiImage = ObjCast<UIImage>(sceneImageObject);
    Assert(uiImage.IsValid());

    Handle<Texture> readbackTexture = view->GetReadbackTexture();
    Assert(readbackTexture != nullptr);

    m_delegateHandlers.Remove("OnReadbackTextureChanged");
    m_delegateHandlers.Add(NAME("OnReadbackTextureChanged"), view->OnReadbackTextureChanged.Bind([uiImageWeak = uiImage.ToWeak()](const Handle<Texture>& readbackTexture)
                                                                 {
                                                                     Handle<UIImage> uiImage = uiImageWeak.Lock();

                                                                     if (!uiImage)
                                                                     {
                                                                         return;
                                                                     }

                                                                     uiImage->SetTexture(readbackTexture);
                                                                 }));

    m_delegateHandlers.Remove(&uiImage->OnClick);
    m_delegateHandlers.Add(uiImage->OnClick.Bind([this](const MouseEvent& event)
        {
            if (m_shouldCancelNextClick)
            {
                return UIEventHandlerResult::STOP_BUBBLING;
            }

            if (m_camera->GetCameraController()->GetInputHandler()->OnClick(event))
            {
                return UIEventHandlerResult::STOP_BUBBLING;
            }

            if (GetWorld()->GetGameState().IsEditor())
            {
                if (IsHoveringManipulationWidget())
                {
                    return UIEventHandlerResult::STOP_BUBBLING;
                }

                const Vec4f mouseWorld = m_camera->TransformScreenToWorld(event.position);
                const Vec4f rayDirection = mouseWorld.Normalized();

                const Ray ray { m_camera->GetWorldTranslation(), rayDirection.GetXYZ() };

                RayTestResults results;

                bool hasHits = false;
                for (const Handle<View>& view : m_views)
                {
                    if (view->TestRay(ray, results, /* useBvh */ true))
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
                            EntityManager* entityManager = entity->GetEntityManager();

                            if (!entityManager)
                            {
                                continue;
                            }

                            if (NodeLinkComponent* nodeLinkComponent = entityManager->TryGetComponent<NodeLinkComponent>(entity))
                            {
                                if (Handle<Node> node = nodeLinkComponent->node.Lock())
                                {
                                    SetFocusedNode(Handle<Node>(node), true);

                                    break;
                                }
                            }
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

                const Ray ray { m_camera->GetWorldTranslation(), rayDirection.GetXYZ() };

                RayTestResults results;

                EditorManipulationWidgetBase& manipulationWidget = m_manipulationWidgetHolder.GetSelectedManipulationWidget();
                bool hitManipulationWidget = false;

                if (manipulationWidget.GetNode()->TestRay(ray, results, /* useBvh */ true))
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

                        NodeLinkComponent* nodeLinkComponent = m_editorScene->GetEntityManager()->TryGetComponent<NodeLinkComponent>(entity);

                        if (!nodeLinkComponent)
                        {
                            continue;
                        }

                        Handle<Node> node = nodeLinkComponent->node.Lock();

                        if (!node)
                        {
                            continue;
                        }

                        if (node == m_hoveredManipulationWidgetNode)
                        {

                            return UIEventHandlerResult::STOP_BUBBLING;
                        }

                        if (manipulationWidget.OnMouseHover(m_camera, event, Handle<Node>(node)))
                        {
                            SetHoveredManipulationWidget(event, &manipulationWidget, Handle<Node>(node));

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

                    const Ray ray { m_camera->GetWorldTranslation(), rayDirection.GetXYZ() };

                    RayTestResults results;

                    if (node->TestRay(ray, results, /* useBvh */ true))
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

            if (event.keyCode == KeyCode::TILDE)
            {
                const bool isConsoleOpen = m_consoleUi && m_consoleUi->IsVisible();

                if (isConsoleOpen)
                {
                    m_consoleUi->SetIsVisible(false);

                    if (m_debugOverlayUiObject)
                    {
                        m_debugOverlayUiObject->SetIsVisible(true);
                    }
                }
                else
                {
                    m_consoleUi->SetIsVisible(true);

                    if (m_debugOverlayUiObject)
                    {
                        m_debugOverlayUiObject->SetIsVisible(false);
                    }
                }

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

static void AddNodeToSceneOutline(const Handle<UIListView>& listView, Node* node)
{
    Assert(node != nullptr);

    // if (node->GetFlags() & NodeFlags::HIDE_IN_SCENE_OUTLINE)
    // {
    //     return;
    // }

    if (!listView)
    {
        return;
    }

    if (UIDataSourceBase* dataSource = listView->GetDataSource())
    {
        WeakHandle<Node> editorNodeWeak = node->WeakHandleFromThis();

        UUID parentNodeUuid = UUID::Invalid();

        if (Node* parentNode = node->GetParent())
        {
            parentNodeUuid = parentNode->GetUUID();

            AssertDebug(dataSource->Get(parentNodeUuid) != nullptr);
        }

        dataSource->Push(node->GetUUID(), HypData(std::move(editorNodeWeak)), parentNodeUuid);
    }

    for (Node* child : node->GetChildren())
    {
        // if (child->GetFlags() & NodeFlags::HIDE_IN_SCENE_OUTLINE)
        // {
        //     continue;
        // }

        AddNodeToSceneOutline(listView, child);
    }
};

void EditorSubsystem::StartWatchingNode(const Handle<Node>& node)
{
    if (!node.IsValid())
    {
        return;
    }

    if (GetWorld()->GetGameState().IsEditor())
    {
        if (g_showOnlyActiveScene && node->GetScene() != m_activeScene.GetUnsafe())
        {
            return;
        }
    }

    Assert(node->GetScene() != nullptr);

    UISubsystem* uiSubsystem = GetWorld()->GetSubsystem<UISubsystem>();
    AssertDebug(uiSubsystem != nullptr);

    Handle<UIListView> listView = ObjCast<UIListView>(uiSubsystem->GetUIStage()->FindChildUIObject(NAME("Outline_ListView")));
    AssertDebug(listView.IsValid());

    HYP_LOG(Editor, Debug, "Start watching node: {}", *node->GetName());

    m_editorDelegates->AddNodeWatcher(
        NAME("SceneView"),
        node.Get(),
        { Node::Class()->GetProperty(NAME("Name")), 1 },
        [this, listViewWeak = listView.ToWeak()](Node* node, const HypProperty* property)
        {
            // Update name in list view
            // @TODO: Ensure game thread

            // if (node->GetFlags() & NodeFlags::HIDE_IN_SCENE_OUTLINE)
            // {
            //     return;
            // }

            HYP_LOG(Editor, Debug, "Node {} property changed : {}", *node->GetName(), *property->GetName());

            Handle<UIListView> listView = listViewWeak.Lock();

            if (!listView)
            {
                return;
            }

            if (UIDataSourceBase* dataSource = listView->GetDataSource())
            {
                const UIDataSourceElement* dataSourceElement = dataSource->Get(node->GetUUID());
                Assert(dataSourceElement != nullptr);

                dataSource->ForceUpdate(node->GetUUID());
            }
        });

    AddNodeToSceneOutline(listView, node.Get());

    m_delegateHandlers.Remove(&node->GetDelegates()->OnChildAdded);

    m_delegateHandlers.Add(node->GetDelegates()->OnChildAdded.Bind([this, listViewWeak = listView.ToWeak()](Node* node, bool isDirect)
        {
            if (!isDirect)
            {
                return;
            }

            Assert(node != nullptr);

            // if (node->GetFlags() & NodeFlags::HIDE_IN_SCENE_OUTLINE)
            // {
            //     return;
            // }

            Handle<UIListView> listView = listViewWeak.Lock();

            AddNodeToSceneOutline(listView, node);
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
        for (const Handle<Scene>& scene : GetWorld()->GetScenes())
        {
            if ((scene->GetFlags() & (SceneFlags::FOREGROUND | SceneFlags::DETACHED | SceneFlags::EDITOR | SceneFlags::UI)) == SceneFlags::FOREGROUND)
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
            for (const Handle<Scene>& scene : m_currentProject->GetScenes())
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
    m_delegateHandlers.Add(OnFocusedNodeChanged.Bind([this, hypClass = Node::Class(), detailsListViewWeak = detailsListView.ToWeak(), outlineListViewWeak = outlineListView.ToWeak()](const Handle<Node>& node, const Handle<Node>& previousNode, bool shouldSelectInOutline)
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

            HashMap<String, HypProperty*> propertiesByName;

            for (auto it = hypClass->GetMembers(HypMemberType::TYPE_PROPERTY).Begin(); it != hypClass->GetMembers(HypMemberType::TYPE_PROPERTY).End(); ++it)
            {
                if (HypProperty* property = dynamic_cast<HypProperty*>(&*it))
                {
                    if (!property->GetAttribute("editor"))
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

                if (const HypClassAttributeValue& attr = it.second->GetAttribute("label"))
                {
                    nodePropertyRef.title = attr.GetString();
                }
                else
                {
                    nodePropertyRef.title = it.first;
                }

                if (const HypClassAttributeValue& attr = it.second->GetAttribute("description"))
                {
                    nodePropertyRef.description = attr.GetString();
                }

                dataSource->Push(UUID(), HypData(std::move(nodePropertyRef)));
            }

            m_editorDelegates->AddNodeWatcher(
                NAME("DetailView"),
                node,
                {},
                Proc<void(Node*, const HypProperty*)> {
                    [this, hypClass = Node::Class(), detailsListViewWeak](Node* node, const HypProperty* property)
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

    m_debugOverlayUiObject = uiSubsystem->GetUIStage()->CreateUIObject<UIListView>(NAME("DebugOverlay"), Vec2i::Zero(), UIObjectSize({ 100, UIObjectSize::FILL }, { 0, UIObjectSize::AUTO }));
    m_debugOverlayUiObject->SetDepth(100);
    m_debugOverlayUiObject->SetBackgroundColor(Color(0.0f, 0.0f, 0.0f, 0.0f));
    m_debugOverlayUiObject->SetParentAlignment(UIObjectAlignment::BOTTOM_LEFT);
    m_debugOverlayUiObject->SetOriginAlignment(UIObjectAlignment::BOTTOM_LEFT);

    m_debugOverlayUiObject->OnClick.RemoveAllDetached();
    m_debugOverlayUiObject->OnKeyDown.RemoveAllDetached();

    for (const Handle<EditorDebugOverlayBase>& debugOverlay : m_debugOverlays)
    {
        debugOverlay->Initialize(m_debugOverlayUiObject.Get());

        if (const Handle<UIObject>& uiObject = debugOverlay->GetUIObject())
        {
            m_debugOverlayUiObject->AddChildUIObject(uiObject);
        }
    }

    if (Handle<UIImage> sceneImage = ObjCast<UIImage>(uiSubsystem->GetUIStage()->FindChildUIObject(NAME("Scene_Image"))))
    {
        sceneImage->AddChildUIObject(m_debugOverlayUiObject);
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

        // @TODO Add icon
        manipulationWidgetMenuItem->SetText("<widget>");

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
    for (const Handle<Scene>& scene : m_currentProject->GetScenes())
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

    if (Handle<UIObject> importButton = uiSubsystem->GetUIStage()->FindChildUIObject("ContentBrowser_Import_Button"))
    {
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
}

void EditorSubsystem::AddPackageToContentBrowser(const Handle<AssetPackage>& package, bool nested)
{
    HYP_SCOPE;

    Assert(package.IsValid());

    if (package->IsHidden())
    {
        return;
    }

    if (UIDataSourceBase* dataSource = m_contentBrowserDirectoryList->GetDataSource())
    {
        Handle<AssetPackage> parentPackage = package->GetParentPackage().Lock();

        UUID parentPackageUuid = parentPackage.IsValid() ? parentPackage->GetUUID() : UUID::Invalid();

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

                    m_contentBrowserContents->GetDataSource()->Push(assetObject->GetUUID(), HypData(assetObject->HandleFromThis()));
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

TResult<RC<FontAtlas>> EditorSubsystem::CreateFontAtlas()
{
    HYP_SCOPE;

    const FilePath serializedFileDirectory = GetResourceDirectory() / "data" / "fonts";
    const FilePath serializedFilePath = serializedFileDirectory / "Roboto.hyp";

    if (!serializedFileDirectory.Exists())
    {
        serializedFileDirectory.MkDir();
    }

    // if (serializedFilePath.Exists())
    // {
    //     HypData loadedFontAtlasData;

    //     FBOMReader reader({});

    //     if (FBOMResult err = reader.LoadFromFile(serializedFilePath, loadedFontAtlasData))
    //     {
    //         return HYP_MAKE_ERROR(Error, "Failed to load font atlas from file: {}", 0, err.message);
    //     }

    //     return loadedFontAtlasData.Get<RC<FontAtlas>>();
    // }

    auto fontFaceAsset = AssetManager::GetInstance()->Load<RC<FontFace>>("fonts/Roboto/Roboto-Regular.ttf");

    if (fontFaceAsset.HasError())
    {
        return HYP_MAKE_ERROR(Error, "Failed to load font face! Error: {}", 0, fontFaceAsset.GetError().GetMessage());
    }

    RC<FontAtlas> atlas = MakeRefCountedPtr<FontAtlas>(std::move(fontFaceAsset->Result()));

    if (auto renderAtlasResult = atlas->RenderAtlasTextures(); renderAtlasResult.HasError())
    {
        return renderAtlasResult.GetError();
    }

    FileByteWriter byteWriter { serializedFilePath };
    FBOMWriter writer { FBOMWriterConfig {} };
    writer.Append(*atlas);
    auto writeErr = writer.Emit(&byteWriter);
    byteWriter.Close();

    if (writeErr != FBOMResult::FBOM_OK)
    {
        return HYP_MAKE_ERROR(Error, "Failed to save font atlas! {}", 0, writeErr.message);
    }

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

    Threads::AssertOnThread(g_gameThread);

    if (project == m_currentProject)
    {
        return;
    }

    if (m_currentProject)
    {
        OnProjectClosing(m_currentProject);

        m_currentProject->SetEditorSubsystem(WeakHandle<EditorSubsystem>::empty);

        m_currentProject->Close();
    }

    if (project)
    {
        m_currentProject = project;

        if (project.IsValid())
        {
            project->SetEditorSubsystem(WeakHandleFromThis());
        }

        if (Result saveResult = project->Save(); saveResult.HasError())
        {
            HYP_LOG(Editor, Error, "Failed to save newly created project: {}", saveResult.GetError().GetMessage());
        }

        OnProjectOpened(m_currentProject);

        g_editorState->SetCurrentProject(m_currentProject);
    }
}

void EditorSubsystem::ShowOpenProjectDialog()
{
    HYP_SCOPE;

    ShowOpenFileDialog(
        "Select the project file to open",
        GetResourceDirectory(),
        { "hyp" },
        /* allowMultiple */ false, /* allowDirectories */ true,
        [](TResult<Array<FilePath>>&& result)
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

            Threads::GetThread(g_gameThread)->GetScheduler().Enqueue([projectFilepath = std::move(result.GetValue()[0])]()
                {
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

                    if (EditorSubsystem* editorSubsystem = g_engineDriver->GetCurrentWorld()->GetSubsystem<EditorSubsystem>())
                    {
                        editorSubsystem->OpenProject(project);

                        return;
                    }

                    HYP_LOG(Editor, Fatal, "EditorSubsystem does not exist!");
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
                batch->Add(file.Basename(), FilePath::Relative(file, GetExecutablePath()));
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

    Threads::AssertOnThread(g_gameThread);

    UISubsystem* uiSubsystem = GetWorld()->GetSubsystem<UISubsystem>();
    Assert(uiSubsystem != nullptr);

    // @TODO Auto remove tasks that aren't on the game thread when they have completed

    RunningEditorTask& runningTask = m_tasks.EmplaceBack(task);

    Handle<UIMenuItem> tasksMenuItem = ObjCast<UIMenuItem>(uiSubsystem->GetUIStage()->FindChildUIObject(NAME("Tasks_MenuItem")));

    if (tasksMenuItem != nullptr)
    {
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
                    [taskWeak = task.ToWeak()](...)
                    {
                        if (Handle<EditorTaskBase> task = taskWeak.Lock())
                        {
                            task->Cancel();
                        }

                        return UIEventHandlerResult::OK;
                    })
                .Detach();
            taskGridColumnRight->AddChildUIObject(cancelButton);

            runningTask.m_uiObject = taskGrid;

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

    // For long running tasks, enqueues the task in the scheduler
    task->Commit();

    // auto CreateTaskUIObject = [](const Handle<UIObject> &parent, const HypClass *taskHypClass) -> Handle<UIObject>
    // {
    //     Assert(parent != nullptr);
    //     Assert(taskHypClass != nullptr);

    //     return parent->CreateUIObject<UIPanel>(Vec2i::Zero(), UIObjectSize({ 400, UIObjectSize::PIXEL }, { 300, UIObjectSize::PIXEL }));
    // };

    // const Handle<UIObject> &uiObject = task->GetUIObject();

    // if (!uiObject) {
    //     if (Threads::IsOnThread(ThreadName::THREAD_GAME)) {
    //         Threads::GetThread(ThreadName::THREAD_GAME)->GetScheduler().Enqueue([this, task, CreateTaskUIObject]()
    //         {
    //             task->SetUIObject(CreateTaskUIObject(GetUIStage(), task->InstanceClass()));
    //         });
    //     } else {
    //         task->SetUIObject(CreateTaskUIObject(GetUIStage(), task->InstanceClass()));
    //     }
    // }

    // Mutex::Guard guard(m_tasksMutex);

    // m_tasks.Insert(UUID(), task);
    // m_numTasks.Increment(1, MemoryOrder::RELAXED);
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
            if (Handle<Entity> entity = ObjCast<Entity>(focusedNode))
            {
                focusedNode->GetScene()->GetEntityManager()->AddTag<EntityTag::EDITOR_FOCUSED>(entity);
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

    if (previousFocusedNode.IsValid() && previousFocusedNode->GetScene() != nullptr)
    {
        if (Handle<Entity> entity = ObjCast<Entity>(previousFocusedNode))
        {
            previousFocusedNode->GetScene()->GetEntityManager()->RemoveTag<EntityTag::EDITOR_FOCUSED>(entity);
        }
    }

    OnFocusedNodeChanged(focusedNode, previousFocusedNode, shouldSelectInOutline);
}

void EditorSubsystem::AddDebugOverlay(const Handle<EditorDebugOverlayBase>& debugOverlay)
{
    HYP_SCOPE;

    if (!debugOverlay)
    {
        return;
    }

    Threads::AssertOnThread(g_gameThread);

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

    if (m_debugOverlayUiObject != nullptr)
    {
        debugOverlay->Initialize(uiSubsystem->GetUIStage());

        if (const Handle<UIObject>& object = debugOverlay->GetUIObject())
        {
            Handle<UIListViewItem> listViewItem = uiSubsystem->GetUIStage()->CreateUIObject<UIListViewItem>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
            listViewItem->SetBackgroundColor(Color(0.0f, 0.0f, 0.0f, 0.0f));
            listViewItem->AddChildUIObject(object);

            m_debugOverlayUiObject->AddChildUIObject(listViewItem);
        }
    }
}

bool EditorSubsystem::RemoveDebugOverlay(WeakName name)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_gameThread);

    auto it = m_debugOverlays.FindIf([name](const auto& item)
        {
            return item->GetName() == name;
        });

    if (it == m_debugOverlays.End())
    {
        return false;
    }

    if (m_debugOverlayUiObject != nullptr)
    {
        if (const Handle<UIObject>& object = (*it)->GetUIObject())
        {
            Handle<UIListViewItem> listViewItem = object->GetClosestParentUIObject<UIListViewItem>();

            if (listViewItem)
            {
                m_debugOverlayUiObject->RemoveChildUIObject(listViewItem);
            }
            else
            {
                object->RemoveFromParent();
            }
        }
    }

    m_debugOverlays.Erase(it);

    return true;
}

Handle<Node> EditorSubsystem::GetFocusedNode() const
{
    return m_focusedNode.Lock();
}

void EditorSubsystem::UpdateCamera(float delta)
{
    HYP_SCOPE;
}

void EditorSubsystem::UpdateTasks(float delta)
{
    HYP_SCOPE;

    for (auto it = m_tasks.Begin(); it != m_tasks.End();)
    {
        auto& task = it->GetTask();

        if (task->IsCommitted())
        {
            if (TickableEditorTask* tickableTask = ObjCast<TickableEditorTask>(task.Get()))
            {
                if (tickableTask->GetTimer().Waiting())
                {
                    ++it;

                    continue;
                }

                tickableTask->GetTimer().NextTick();
                tickableTask->Tick(tickableTask->GetTimer().delta);
            }

            if (task->IsCompleted())
            {
                task->OnComplete();

                // Remove the UIObject for the task from this stage
                if (const Handle<UIObject>& taskUiObject = it->GetUIObject())
                {
                    taskUiObject->RemoveFromParent();
                }

                it = m_tasks.Erase(it);

                continue;
            }
        }

        ++it;
    }
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
        m_hoveredManipulationWidget = manipulationWidget->WeakHandleFromThis();
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
