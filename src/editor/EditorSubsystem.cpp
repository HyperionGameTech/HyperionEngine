/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorSubsystem.hpp>
#include <editor/EditorDelegates.hpp>
#include <editor/EditorCamera.hpp>
#include <editor/EditorTask.hpp>
#include <editor/EditorProject.hpp>
#include <editor/EditorActionStack.hpp>
#include <editor/EditorAction.hpp>

#include <editor/ui/debug/EditorDebugOverlay.hpp>

#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/Mesh.hpp>
#include <scene/View.hpp>
#include <scene/Light.hpp>
#include <scene/Texture.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/NodeLinkComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/CameraComponent.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

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

#include <core/object/HypClassUtils.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/io/ByteWriter.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <rendering/UIRenderer.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/debug/DebugDrawer.hpp>

#include <rendering/subsystems/ScreenCapture.hpp>

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

#include <Engine.hpp>
#include <HyperionEngine.hpp>

namespace hyperion {

HYP_DEFINE_LOG_CHANNEL(Editor);

#pragma region RunningEditorTask

Handle<UIObject> RunningEditorTask::CreateUIObject(UIStage* ui_stage) const
{
    AssertThrow(ui_stage != nullptr);

    Handle<UIPanel> panel = ui_stage->CreateUIObject<UIPanel>(NAME("EditorTaskPanel"), Vec2i::Zero(), UIObjectSize({ 100, UIObjectSize::FILL }, { 100, UIObjectSize::FILL }));
    panel->SetBackgroundColor(Color(0xFF0000FF)); // testing

    Handle<UIText> task_title = ui_stage->CreateUIObject<UIText>(NAME("Task_Title"), Vec2i::Zero(), UIObjectSize(UIObjectSize::AUTO));
    task_title->SetTextSize(16.0f);
    task_title->SetText(m_task->InstanceClass()->GetName().LookupString());
    panel->AddChildUIObject(task_title);

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
    Threads::AssertOnThread(g_game_thread);

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

    LightmapperSubsystem* lightmapper_subsystem = m_world->GetSubsystem<LightmapperSubsystem>();

    if (!lightmapper_subsystem)
    {
        lightmapper_subsystem = m_world->AddSubsystem<LightmapperSubsystem>();
    }

    m_task = lightmapper_subsystem->GenerateLightmaps(m_scene, m_aabb);
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
    Threads::AssertOnThread(g_game_thread);

    if (m_task != nullptr)
    {
        if (m_task->IsCompleted())
        {
            m_task = nullptr;
        }
    }

    HYP_LOG(Editor, Info, "GenerateLightmapsEditorTask ticked");
}

#pragma endregion GenerateLightmapsEditorTask

#pragma region EditorManipulationWidgetBase

EditorManipulationWidgetBase::EditorManipulationWidgetBase()
    : m_is_dragging(false)
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

    m_focused_node.Reset();

    SetReady(false);
}

void EditorManipulationWidgetBase::UpdateWidget(const Handle<Node>& focused_node)
{
    m_focused_node = focused_node;

    if (!focused_node.IsValid())
    {
        return;
    }

    if (!m_node.IsValid())
    {
        return;
    }

    m_node->SetWorldTranslation(focused_node->GetWorldAABB().GetCenter());
}

void EditorManipulationWidgetBase::OnDragStart(const Handle<Camera>& camera, const MouseEvent& mouse_event, const Handle<Node>& node, const Vec3f& hitpoint)
{
    m_is_dragging = true;
}

void EditorManipulationWidgetBase::OnDragEnd(const Handle<Camera>& camera, const MouseEvent& mouse_event, const Handle<Node>& node)
{
    m_is_dragging = false;
}

Handle<EditorProject> EditorManipulationWidgetBase::GetCurrentProject() const
{
    return m_current_project.Lock();
}

#pragma endregion EditorManipulationWidgetBase

#pragma region TranslateEditorManipulationWidget

void TranslateEditorManipulationWidget::OnDragStart(const Handle<Camera>& camera, const MouseEvent& mouse_event, const Handle<Node>& node, const Vec3f& hitpoint)
{
    EditorManipulationWidgetBase::OnDragStart(camera, mouse_event, node, hitpoint);

    m_drag_data.Unset();

    if (!node->GetEntity() || !node->GetScene() || !node->GetScene()->GetEntityManager())
    {
        return;
    }

    MeshComponent* mesh_component = node->GetScene()->GetEntityManager()->TryGetComponent<MeshComponent>(node->GetEntity());

    if (!mesh_component || !mesh_component->material)
    {
        return;
    }

    const NodeTag& axis_tag = node->GetTag("TransformWidgetAxis");

    if (!axis_tag)
    {
        return;
    }

    int axis = axis_tag.data.TryGet<int>(-1);

    if (axis == -1)
    {
        return;
    }

    Handle<Node> focused_node = m_focused_node.Lock();

    if (!focused_node.IsValid())
    {
        return;
    }

    DragData drag_data {
        .axis_direction = Vec3f::Zero(),
        .plane_normal = Vec3f::Zero(),
        .plane_point = m_node->GetWorldTranslation(),
        .hitpoint_origin = hitpoint,
        .node_origin = focused_node->GetWorldTranslation()
    };

    drag_data.axis_direction[axis] = 1.0f;

    drag_data.plane_normal = drag_data.axis_direction.Cross(camera->GetDirection()).Normalize();

    if (drag_data.plane_normal.LengthSquared() < 1e6f)
    {
        drag_data.plane_normal = drag_data.axis_direction.Cross(camera->GetUpVector()).Normalize();
    }

    const Vec4f mouse_world = camera->TransformScreenToWorld(mouse_event.position);
    const Vec4f ray_direction = mouse_world.Normalized();

    const Ray ray { camera->GetTranslation(), ray_direction.GetXYZ() };

    RayHit plane_ray_hit;

    if (Optional<RayHit> plane_ray_hit_opt = ray.TestPlane(drag_data.plane_point, drag_data.plane_normal))
    {
        plane_ray_hit = *plane_ray_hit_opt;
    }
    else
    {
        return;
    }

    m_drag_data = drag_data;

    HYP_LOG(Editor, Info, "Drag data: axis_direction = {}, plane_normal = {}, plane_point = {}, node_origin = {}",
        m_drag_data->axis_direction,
        m_drag_data->plane_normal,
        m_drag_data->plane_point,
        m_drag_data->node_origin);
}

void TranslateEditorManipulationWidget::OnDragEnd(const Handle<Camera>& camera, const MouseEvent& mouse_event, const Handle<Node>& node)
{
    EditorManipulationWidgetBase::OnDragEnd(camera, mouse_event, node);

    // Commit editor transaction
    if (Handle<EditorProject> project = GetCurrentProject())
    {
        if (Handle<Node> focused_node = m_focused_node.Lock())
        {
            project->GetActionStack()->Push(CreateObject<FunctionalEditorAction>(
                NAME("Translate"),
                [manipulation_mode = GetManipulationMode(), focused_node, node = m_node, final_position = focused_node->GetWorldTranslation(), origin = m_drag_data->node_origin]() -> EditorActionFunctions
                {
                    return {
                        [&](EditorSubsystem* editor_subsystem, EditorProject* editor_project)
                        {
                            NodeUnlockTransformScope unlock_transform_scope(*focused_node);
                            focused_node->SetWorldTranslation(final_position);

                            if (Node* parent = node->FindParentWithName("TranslateWidget"))
                            {
                                parent->SetWorldTranslation(final_position);
                            }

                            editor_subsystem->GetManipulationWidgetHolder().SetSelectedManipulationMode(manipulation_mode);

                            editor_subsystem->SetFocusedNode(focused_node, true);
                        },
                        [&](EditorSubsystem* editor_subsystem, EditorProject* editor_project)
                        {
                            NodeUnlockTransformScope unlock_transform_scope(*focused_node);
                            focused_node->SetWorldTranslation(origin);

                            if (Node* parent = node->FindParentWithName("TranslateWidget"))
                            {
                                parent->SetWorldTranslation(origin);
                            }

                            editor_subsystem->GetManipulationWidgetHolder().SetSelectedManipulationMode(manipulation_mode);

                            editor_subsystem->SetFocusedNode(focused_node, true);
                        }
                    };
                }));
        }
    }

    m_drag_data.Unset();
}

bool TranslateEditorManipulationWidget::OnMouseHover(const Handle<Camera>& camera, const MouseEvent& mouse_event, const Handle<Node>& node)
{
    if (!node->GetEntity() || !node->GetScene() || !node->GetScene()->GetEntityManager())
    {
        return false;
    }

    MeshComponent* mesh_component = node->GetScene()->GetEntityManager()->TryGetComponent<MeshComponent>(node->GetEntity());

    if (!mesh_component || !mesh_component->material)
    {
        return false;
    }

    mesh_component->material->SetParameter(
        Material::MATERIAL_KEY_ALBEDO,
        Vec4f(1.0f, 1.0f, 0.0, 1.0));

    return true;
}

bool TranslateEditorManipulationWidget::OnMouseLeave(const Handle<Camera>& camera, const MouseEvent& mouse_event, const Handle<Node>& node)
{
    if (!node->GetEntity() || !node->GetScene() || !node->GetScene()->GetEntityManager())
    {
        return false;
    }

    MeshComponent* mesh_component = node->GetScene()->GetEntityManager()->TryGetComponent<MeshComponent>(node->GetEntity());

    if (!mesh_component || !mesh_component->material)
    {
        return false;
    }

    if (const NodeTag& tag = node->GetTag("TransformWidgetElementColor"))
    {
        mesh_component->material->SetParameter(
            Material::MATERIAL_KEY_ALBEDO,
            tag.data.TryGet<Vec4f>(Vec4f::Zero()));
    }

    return true;
}

bool TranslateEditorManipulationWidget::OnMouseMove(const Handle<Camera>& camera, const MouseEvent& mouse_event, const Handle<Node>& node)
{
    if (!mouse_event.mouse_buttons[MouseButtonState::LEFT])
    {
        return false;
    }

    if (!node->GetEntity() || !node->GetScene() || !node->GetScene()->GetEntityManager())
    {
        return false;
    }

    if (!m_drag_data)
    {
        return false;
    }

    MeshComponent* mesh_component = node->GetScene()->GetEntityManager()->TryGetComponent<MeshComponent>(node->GetEntity());

    if (!mesh_component || !mesh_component->material)
    {
        return false;
    }

    const NodeTag& axis_tag = node->GetTag("TransformWidgetAxis");

    if (!axis_tag)
    {
        return false;
    }
    const Vec4f mouse_world = camera->TransformScreenToWorld(mouse_event.position);
    const Vec4f ray_direction = mouse_world.Normalized();

    const Ray ray { camera->GetTranslation(), ray_direction.GetXYZ() };

    // const Ray ray_view_space { camera->GetViewMatrix() * ray.position, (camera->GetViewMatrix() * Vec4f(ray.direction, 0.0f)).GetXYZ() };

    // Vec4f mouse_view = camera->GetViewMatrix() * mouse_world;
    // mouse_view /= mouse_view.w;

    RayHit plane_ray_hit;

    if (Optional<RayHit> plane_ray_hit_opt = ray.TestPlane(m_drag_data->node_origin, m_drag_data->plane_normal))
    {
        plane_ray_hit = *plane_ray_hit_opt;
    }
    else
    {
        return true;
    }

    const float t = (plane_ray_hit.hitpoint - m_drag_data->hitpoint_origin).Dot(m_drag_data->axis_direction);
    const Vec3f translation = m_drag_data->node_origin + (m_drag_data->axis_direction * t);

    Handle<Node> focused_node = m_focused_node.Lock();

    if (!focused_node.IsValid())
    {
        return false;
    }

    NodeUnlockTransformScope unlock_transform_scope(*focused_node);
    focused_node->SetWorldTranslation(translation);

    if (Node* parent = node->FindParentWithName("TranslateWidget"))
    {
        parent->SetWorldTranslation(translation);
    }

    return true;
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
                if (!child->GetEntity().IsValid())
                {
                    continue;
                }

                if (!child->GetScene())
                {
                    HYP_LOG(Editor, Warning, "Manipulation widget child \"{}\" has no scene set", child->GetName());

                    continue;
                }

                EntityManager& entity_manager = *child->GetScene()->GetEntityManager();

                entity_manager.RemoveTag<EntityTag::STATIC>(child->GetEntity());
                entity_manager.AddTag<EntityTag::DYNAMIC>(child->GetEntity());

                VisibilityStateComponent* visibility_state = entity_manager.TryGetComponent<VisibilityStateComponent>(child->GetEntity());

                if (visibility_state)
                {
                    visibility_state->flags |= VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE;
                }
                else
                {
                    entity_manager.AddComponent<VisibilityStateComponent>(child->GetEntity(), VisibilityStateComponent { VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE });
                }

                MeshComponent* mesh_component = entity_manager.TryGetComponent<MeshComponent>(child->GetEntity());

                if (!mesh_component)
                {
                    continue;
                }

                MaterialAttributes material_attributes;
                Material::ParameterTable material_parameters;

                if (mesh_component->material.IsValid())
                {
                    material_attributes = mesh_component->material->GetRenderAttributes();
                    material_parameters = mesh_component->material->GetParameters();
                }
                else
                {
                    material_parameters = Material::DefaultParameters();
                }

                // disable depth write and depth test
                material_attributes.flags &= ~(MaterialAttributeFlags::DEPTH_WRITE | MaterialAttributeFlags::DEPTH_TEST);
                material_attributes.bucket = RB_DEBUG;

                // testing
                material_attributes.stencil_function = StencilFunction {
                    .pass_op = SO_REPLACE,
                    .fail_op = SO_REPLACE,
                    .depth_fail_op = SO_REPLACE,
                    .compare_op = SCO_ALWAYS,
                    .mask = 0xff,
                    .value = 0x1
                };

                mesh_component->material = MaterialCache::GetInstance()->CreateMaterial(material_attributes, material_parameters);
                mesh_component->material->SetIsDynamic(true);

                entity_manager.AddTag<EntityTag::UPDATE_RENDER_PROXY>(child->GetEntity());

                child->AddTag(NodeTag(NAME("TransformWidgetElementColor"), Vec4f(mesh_component->material->GetParameter(Material::MATERIAL_KEY_ALBEDO))));
            }

            FileByteWriter byte_writer(GetResourceDirectory() / "models/editor/axis_arrows.hypmodel");
            FBOMWriter writer { FBOMWriterConfig {} };
            writer.Append(*node);

            FBOMResult write_err = writer.Emit(&byte_writer);

            byte_writer.Close();

            if (write_err)
            {
                HYP_LOG(Editor, Error, "Failed to write axis arrows to disk: {}", write_err.message);
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

EditorManipulationWidgetHolder::EditorManipulationWidgetHolder(EditorSubsystem* editor_subsystem)
    : m_editor_subsystem(editor_subsystem),
      m_selected_manipulation_mode(EditorManipulationMode::NONE)
{
    m_manipulation_widgets.Insert(CreateObject<NullEditorManipulationWidget>());
    m_manipulation_widgets.Insert(CreateObject<TranslateEditorManipulationWidget>());
}

EditorManipulationMode EditorManipulationWidgetHolder::GetSelectedManipulationMode() const
{
    Threads::AssertOnThread(g_game_thread);

    return m_selected_manipulation_mode;
}

void EditorManipulationWidgetHolder::SetSelectedManipulationMode(EditorManipulationMode mode)
{
    Threads::AssertOnThread(g_game_thread);

    if (mode == m_selected_manipulation_mode)
    {
        return;
    }

    EditorManipulationWidgetBase& new_widget = *m_manipulation_widgets.At(mode);
    EditorManipulationWidgetBase& prev_widget = *m_manipulation_widgets.At(m_selected_manipulation_mode);

    OnSelectedManipulationWidgetChange(new_widget, prev_widget);

    m_selected_manipulation_mode = mode;
}

EditorManipulationWidgetBase& EditorManipulationWidgetHolder::GetSelectedManipulationWidget() const
{
    Threads::AssertOnThread(g_game_thread);

    return *m_manipulation_widgets.At(m_selected_manipulation_mode);
}

void EditorManipulationWidgetHolder::SetCurrentProject(const WeakHandle<EditorProject>& project)
{
    Threads::AssertOnThread(g_game_thread);

    m_current_project = project;

    for (auto& it : m_manipulation_widgets)
    {
        it->SetCurrentProject(project);
    }
}

void EditorManipulationWidgetHolder::Initialize()
{
    Threads::AssertOnThread(g_game_thread);

    for (const Handle<EditorManipulationWidgetBase>& widget : m_manipulation_widgets)
    {
        widget->SetEditorSubsystem(m_editor_subsystem);
        widget->SetCurrentProject(m_current_project);

        InitObject(widget);
    }
}

void EditorManipulationWidgetHolder::Shutdown()
{
    Threads::AssertOnThread(g_game_thread);

    if (m_selected_manipulation_mode != EditorManipulationMode::NONE)
    {
        OnSelectedManipulationWidgetChange(
            *m_manipulation_widgets.At(EditorManipulationMode::NONE),
            *m_manipulation_widgets.At(m_selected_manipulation_mode));

        m_selected_manipulation_mode = EditorManipulationMode::NONE;
    }

    for (auto& it : m_manipulation_widgets)
    {
        it->Shutdown();
    }
}

#pragma endregion EditorManipulationWidgetHolder

#pragma region EditorSubsystem

static constexpr bool g_show_only_active_scene = true; // @TODO: Make this configurable

#ifdef HYP_EDITOR

EditorSubsystem::EditorSubsystem(const Handle<AppContextBase>& app_context)
    : m_app_context(app_context),
      m_editor_camera_enabled(false),
      m_should_cancel_next_click(false),
      m_manipulation_widget_holder(this)
{
    m_editor_delegates = new EditorDelegates();

    OnProjectOpened
        .Bind([this](const Handle<EditorProject>& project)
            {
                HYP_LOG(Editor, Info, "Opening project: {}", *project->GetName());

                InitObject(project);

                WeakHandle<Scene> active_scene;

                for (const Handle<Scene>& scene : project->GetScenes())
                {
                    AssertThrow(scene.IsValid());

                    if (!active_scene.IsValid())
                    {
                        active_scene = scene;
                    }

                    GetWorld()->AddScene(scene);

                    m_delegate_handlers.Add(
                        scene->OnRootNodeChanged
                            .Bind([this](const Handle<Node>& new_root, const Handle<Node>& prev_root)
                                {
                                    UpdateWatchedNodes();
                                }));
                }

                UpdateWatchedNodes();

                m_delegate_handlers.Add(
                    project->OnSceneAdded.Bind([this, project_weak = project.ToWeak()](const Handle<Scene>& scene)
                        {
                            AssertThrow(scene.IsValid());

                            Handle<EditorProject> project = project_weak.Lock();
                            AssertThrow(project.IsValid());

                            HYP_LOG(Editor, Info, "Project {} added scene: {}", *project->GetName(), *scene->GetName());

                            GetWorld()->AddScene(scene);

                            m_delegate_handlers.Add(
                                scene->OnRootNodeChanged
                                    .Bind([this](const Handle<Node>& new_root, const Handle<Node>& prev_root)
                                        {
                                            UpdateWatchedNodes();
                                        }));

                            if (!m_active_scene.IsValid())
                            {
                                SetActiveScene(scene);
                            }

                            // reinitialize scene selector on scene add
                            InitActiveSceneSelection();
                        }));

                m_delegate_handlers.Add(
                    project->OnSceneRemoved.Bind([this, project_weak = project.ToWeak()](const Handle<Scene>& scene)
                        {
                            AssertThrow(scene.IsValid());

                            Handle<EditorProject> project = project_weak.Lock();
                            AssertThrow(project.IsValid());

                            HYP_LOG(Editor, Info, "Project {} removed scene: {}", *project->GetName(), *scene->GetName());

                            m_delegate_handlers.Remove(&scene->OnRootNodeChanged);

                            StopWatchingNode(scene->GetRoot());

                            GetWorld()->RemoveScene(scene);

                            // reinitialize scene selector on scene remove
                            InitActiveSceneSelection();
                        }));

                m_delegate_handlers.Remove("OnPackageAdded");

                if (m_content_browser_directory_list && m_content_browser_directory_list->GetDataSource())
                {
                    m_content_browser_directory_list->GetDataSource()->Clear();

                    for (const Handle<AssetPackage>& package : project->GetAssetRegistry()->GetPackages())
                    {
                        AddPackageToContentBrowser(package, true);
                    }

                    m_delegate_handlers.Add(
                        NAME("OnPackageAdded"),
                        project->GetAssetRegistry()->OnPackageAdded.Bind([this](const Handle<AssetPackage>& package)
                            {
                                AddPackageToContentBrowser(package, false);
                            }));
                }

                m_manipulation_widget_holder.Initialize();
                m_manipulation_widget_holder.SetCurrentProject(project);

                m_delegate_handlers.Add(
                    NAME("OnGameStateChange"),
                    GetWorld()->OnGameStateChange.Bind([this](World* world, GameStateMode previous_game_state_mode, GameStateMode current_game_state_mode)
                        {
                            UpdateWatchedNodes();

                            switch (current_game_state_mode)
                            {
                            case GameStateMode::EDITOR:
                            {
                                m_delegate_handlers.Remove("World_SceneAddedDuringSimulation");
                                m_delegate_handlers.Remove("World_SceneRemovedDuringSimulation");

                                break;
                            }
                            case GameStateMode::SIMULATING:
                            {
                                // unset manipulation widgets
                                m_manipulation_widget_holder.SetSelectedManipulationMode(EditorManipulationMode::NONE);

                                m_delegate_handlers.Add(
                                    NAME("World_SceneAddedDuringSimulation"),
                                    world->OnSceneAdded.Bind([this](World*, const Handle<Scene>& scene)
                                        {
                                            if (!scene.IsValid())
                                            {
                                                return;
                                            }

                                            StartWatchingNode(scene->GetRoot());
                                        }));

                                m_delegate_handlers.Add(
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

                SetActiveScene(active_scene);
            })
        .Detach();

    OnProjectClosing
        .Bind([this](const Handle<EditorProject>& project)
            {
                // Shutdown to reinitialize widget holder after project is opened
                m_manipulation_widget_holder.Shutdown();

                m_focused_node.Reset();

                if (m_highlight_node.IsValid())
                {
                    m_highlight_node->Remove();
                }

                SetActiveScene(WeakHandle<Scene>::empty);

                for (const Handle<Scene>& scene : project->GetScenes())
                {
                    if (!scene.IsValid())
                    {
                        continue;
                    }

                    HYP_LOG(Editor, Info, "Closing project {} scene: {}", *project->GetName(), *scene->GetName());

                    m_delegate_handlers.Remove(&scene->OnRootNodeChanged);

                    StopWatchingNode(scene->GetRoot());

                    GetWorld()->RemoveScene(scene);
                }

                m_delegate_handlers.Remove(&project->OnSceneAdded);
                m_delegate_handlers.Remove(&project->OnSceneRemoved);

                ScreenCaptureRenderSubsystem* subsystem = GetWorld()->GetSubsystem<ScreenCaptureRenderSubsystem>();

                if (subsystem)
                {
                    m_delegate_handlers.Remove(&subsystem->OnTextureResize);

                    bool removed = GetWorld()->RemoveSubsystem(subsystem);
                    AssertThrow(removed);
                }

                m_delegate_handlers.Remove("SetBuildBVHFlag");
                m_delegate_handlers.Remove("OnPackageAdded");
                m_delegate_handlers.Remove("OnGameStateChange");

                if (m_content_browser_directory_list && m_content_browser_directory_list->GetDataSource())
                {
                    m_content_browser_directory_list->GetDataSource()->Clear();
                }

                // reinitialize scene selector
                InitActiveSceneSelection();
            })
        .Detach();

    m_manipulation_widget_holder.OnSelectedManipulationWidgetChange
        .Bind([this](EditorManipulationWidgetBase& new_widget, EditorManipulationWidgetBase& prev_widget)
            {
                SetHoveredManipulationWidget(MouseEvent {}, nullptr, Handle<Node>::empty);

                if (prev_widget.GetManipulationMode() != EditorManipulationMode::NONE)
                {
                    if (prev_widget.GetNode().IsValid())
                    {
                        prev_widget.GetNode()->Remove();
                    }

                    prev_widget.UpdateWidget(Handle<Node>::empty);
                }

                if (new_widget.GetManipulationMode() != EditorManipulationMode::NONE)
                {
                    new_widget.UpdateWidget(m_focused_node.Lock());

                    if (!new_widget.GetNode().IsValid())
                    {
                        HYP_LOG(Editor, Warning, "Manipulation widget has no valid node; cannot attach to scene");

                        return;
                    }

                    m_editor_scene->GetRoot()->AddChild(new_widget.GetNode());
                }
            })
        .Detach();
}

EditorSubsystem::~EditorSubsystem()
{
    if (m_current_project)
    {
        m_current_project->SetEditorSubsystem(WeakHandle<EditorSubsystem>::empty);
        m_current_project->Close();

        m_current_project.Reset();
    }

    delete m_editor_delegates;
}

void EditorSubsystem::OnAddedToWorld()
{
    HYP_SCOPE;

    if (!GetWorld()->GetSubsystem<UISubsystem>())
    {
        HYP_FAIL("EditorSubsystem requires UISubsystem to be initialized");
    }

    m_editor_scene = CreateObject<Scene>(nullptr, SceneFlags::FOREGROUND | SceneFlags::EDITOR);
    m_editor_scene->SetName(NAME("EditorScene"));
    g_engine->GetWorld()->AddScene(m_editor_scene);

    m_camera = CreateObject<Camera>();
    m_camera->AddCameraController(CreateObject<EditorCameraController>());
    m_camera->SetName(NAME("EditorCamera"));
    m_camera->SetFlags(CameraFlags::MATCH_WINDOW_SIZE);
    m_camera->SetFOV(70.0f);
    m_camera->SetNear(0.1f);
    m_camera->SetFar(3000.0f);

    Handle<Node> camera_node = m_editor_scene->GetRoot()->AddChild();
    camera_node->SetName(m_camera->GetName());

    Handle<Entity> camera_entity = m_editor_scene->GetEntityManager()->AddEntity();
    m_editor_scene->GetEntityManager()->AddTag<EntityTag::CAMERA_PRIMARY>(camera_entity);
    m_editor_scene->GetEntityManager()->AddComponent<CameraComponent>(camera_entity, CameraComponent { m_camera });

    camera_node->SetEntity(camera_entity);

    LoadEditorUIDefinitions();

    InitContentBrowser();
    InitViewport();
    InitSceneOutline();
    InitActiveSceneSelection();

    InitDetailView();

    CreateHighlightNode();

    g_engine->GetScriptingService()->OnScriptStateChanged.Bind([](const ManagedScript& script)
                                                             {
                                                                 DebugLog(LogType::Debug, "Script state changed: now is %u\n", script.state);
                                                             })
        .Detach();

    if (Handle<AssetCollector> base_asset_collector = g_asset_manager->GetBaseAssetCollector())
    {
        base_asset_collector->StartWatching();
    }

    g_asset_manager->OnAssetCollectorAdded
        .Bind([](const Handle<AssetCollector>& asset_collector)
            {
                asset_collector->StartWatching();
            })
        .Detach();

    g_asset_manager->OnAssetCollectorRemoved
        .Bind([](const Handle<AssetCollector>& asset_collector)
            {
                asset_collector->StopWatching();
            })
        .Detach();

    NewProject();

    // auto result = EditorProject::Load(GetResourceDirectory() / "projects" / "UntitledProject9");

    // if (!result)
    // {
    //     HYP_BREAKPOINT;
    // }

    // OpenProject(*result);
}

void EditorSubsystem::OnRemovedFromWorld()
{
    HYP_SCOPE;

    g_engine->GetWorld()->RemoveScene(m_editor_scene);

    if (m_current_project)
    {
        OnProjectClosing(m_current_project);

        m_current_project->Close();

        m_current_project.Reset();
    }
}

void EditorSubsystem::Update(float delta)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    m_editor_delegates->Update();

    UpdateCamera(delta);
    UpdateTasks(delta);
    UpdateDebugOverlays(delta);

#if 0
    if (m_focused_node.IsValid()) {
        g_engine->GetDebugDrawer()->Box(m_focused_node->GetWorldTranslation(), m_focused_node->GetWorldAABB().GetExtent() + Vec3f(1.0001f), Color(0.0f, 0.0f, 1.0f, 1.0f));
        g_engine->GetDebugDrawer()->Box(m_focused_node->GetWorldTranslation(), m_focused_node->GetWorldAABB().GetExtent(), Color(1.0f), RenderableAttributeSet(
            MeshAttributes {
                .vertex_attributes = static_mesh_vertex_attributes
            },
            MaterialAttributes {
                .bucket             = RB_TRANSLUCENT,
                .fill_mode          = FM_FILL,
                .blend_function     = BlendFunction::None(),
                .flags              = MaterialAttributeFlags::DEPTH_TEST,
                .stencil_function   = StencilFunction {
                    .pass_op        = SO_REPLACE,
                    .fail_op        = SO_REPLACE,
                    .depth_fail_op  = SO_REPLACE,
                    .compare_op     = SCO_NEVER,
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
    UISubsystem* ui_subsystem = GetWorld()->GetSubsystem<UISubsystem>();
    AssertThrow(ui_subsystem != nullptr);

    RC<FontAtlas> font_atlas = CreateFontAtlas();
    ui_subsystem->GetUIStage()->SetDefaultFontAtlas(font_atlas);

    auto loaded_ui_asset = AssetManager::GetInstance()->Load<UIObject>("ui/Editor.Main.ui.xml");
    AssertThrowMsg(loaded_ui_asset.HasValue(), "Failed to load editor UI definitions!");

    Handle<UIStage> loaded_ui = loaded_ui_asset->Result().Cast<UIStage>();
    AssertThrowMsg(loaded_ui != nullptr, "Failed to load editor UI");

    loaded_ui->SetDefaultFontAtlas(font_atlas);

    ui_subsystem->GetUIStage()->AddChildUIObject(loaded_ui);
}

void EditorSubsystem::CreateHighlightNode()
{
    // m_highlight_node = Handle<Node>(CreateObject<Node>("Editor_Highlight"));
    // m_highlight_node->SetFlags(m_highlight_node->GetFlags() | NodeFlags::HIDE_IN_SCENE_OUTLINE);

    // const Handle<Entity> entity = m_scene->GetEntityManager()->AddEntity();

    // Handle<Mesh> mesh = MeshBuilder::Cube();
    // InitObject(mesh);

    // Handle<Material> material = g_material_system->GetOrCreate(
    //     {
    //         .shader_definition = ShaderDefinition {
    //             NAME("Forward"),
    //             ShaderProperties(mesh->GetVertexAttributes())
    //         },
    //         .bucket = RB_TRANSLUCENT,
    //         // .flags = MaterialAttributeFlags::NONE, // temp
    //         .stencil_function = StencilFunction {
    //             .pass_op        = SO_REPLACE,
    //             .fail_op        = SO_KEEP,
    //             .depth_fail_op  = SO_KEEP,
    //             .compare_op     = SCO_NOT_EQUAL,
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

    // m_highlight_node->SetEntity(entity);
}

void EditorSubsystem::InitViewport()
{
    for (const Handle<View>& view : m_views)
    {
        GetWorld()->RemoveView(view);
    }

    m_views.Clear();

    UISubsystem* ui_subsystem = GetWorld()->GetSubsystem<UISubsystem>();
    AssertThrow(ui_subsystem != nullptr);

    Handle<UIObject> scene_image_object = ui_subsystem->GetUIStage()->FindChildUIObject(NAME("Scene_Image"));
    if (!scene_image_object)
    {
        HYP_LOG(Editor, Error, "Scene image object not found in UI stage!");
        return;
    }

    ui_subsystem->GetUIStage()->UpdateSize(true);

    HYP_LOG(Editor, Debug, "scene surface : {}", ui_subsystem->GetUIStage()->GetSurfaceSize());
    HYP_LOG(Editor, Debug, "scene image object size : {}", scene_image_object->GetActualSize());

    Vec2u viewport_size = MathUtil::Max(Vec2u(scene_image_object->GetActualSize()), Vec2u::One());

    Handle<View> view = CreateObject<View>(ViewDesc {
        .flags = ViewFlags::DEFAULT | ViewFlags::GBUFFER,
        .viewport = Viewport { .extent = viewport_size, .position = Vec2i::Zero() },
        .output_target_desc = { .extent = viewport_size },
        .camera = m_camera });

    InitObject(view);

    HYP_LOG(Editor, Info, "Creating editor viewport with size: {}", viewport_size);

    Handle<ScreenCaptureRenderSubsystem> screen_capture_render_subsystem = GetWorld()->AddSubsystem<ScreenCaptureRenderSubsystem>(view);
    // m_delegate_handlers.Add(
    //     screen_capture_render_subsystem->OnTextureResize.Bind([this, scene_image_object_weak = scene_image_object.ToWeak()](const Handle<Texture>& texture)
    //         {
    //             if (Handle<UIObject> scene_image_object = scene_image_object_weak.Lock())
    //             {
    //                 if (Handle<UIImage> ui_image = scene_image_object.Cast<UIImage>())
    //                 {
    //                     ui_image->SetTexture(Handle<Texture>::empty);
    //                     ui_image->SetTexture(texture);
    //                 }
    //             }
    //         },
    //         g_game_thread));

    GetWorld()->AddView(view);

    m_views.PushBack(view);

    m_delegate_handlers.Remove(&scene_image_object->OnSizeChange);
    m_delegate_handlers.Add(scene_image_object->OnSizeChange.Bind([this, scene_image_object_weak = scene_image_object.ToWeak(), view_weak = view.ToWeak()]()
        {
            Handle<UIObject> scene_image_object = scene_image_object_weak.Lock();
            if (!scene_image_object)
            {
                HYP_LOG(Editor, Warning, "Scene image object is no longer valid!");
                return UIEventHandlerResult::ERR;
            }

            Handle<View> view = view_weak.Lock();
            if (!view)
            {
                HYP_LOG(Editor, Warning, "View is no longer valid!");
                return UIEventHandlerResult::ERR;
            }

            Vec2u viewport_size = MathUtil::Max(Vec2u(scene_image_object->GetActualSize()), Vec2u::One());

            view->SetViewport(Viewport { .extent = viewport_size, .position = Vec2i::Zero() });

            HYP_LOG(Editor, Info, "Main editor view viewport size changed to {}", viewport_size);

            return UIEventHandlerResult::OK;
        }));

    Handle<UIImage> ui_image = scene_image_object.Cast<UIImage>();
    AssertThrow(ui_image != nullptr);

    m_scene_texture.Reset();

    m_scene_texture = screen_capture_render_subsystem->GetTexture();
    AssertThrow(m_scene_texture.IsValid());

    m_delegate_handlers.Remove(&ui_image->OnClick);
    m_delegate_handlers.Add(ui_image->OnClick.Bind([this](const MouseEvent& event)
        {
            if (m_should_cancel_next_click)
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

                const Vec4f mouse_world = m_camera->TransformScreenToWorld(event.position);
                const Vec4f ray_direction = mouse_world.Normalized();

                const Ray ray { m_camera->GetTranslation(), ray_direction.GetXYZ() };

                RayTestResults results;

                bool has_hits = false;
                for (const Handle<View>& view : m_views)
                {
                    if (view->TestRay(ray, results, /* use_bvh */ true))
                    {
                        has_hits = true;
                    }
                }

                if (has_hits)
                {
                    for (const RayHit& hit : results)
                    {
                        /// \FIXME: Can't do TypeId::ForType<Entity>, there may be derived types of Entity
                        if (ObjId<Entity> entity_id = ObjId<Entity>(ObjIdBase { TypeId::ForType<Entity>(), hit.id }))
                        {
                            Handle<Entity> entity { entity_id };
                            EntityManager* entity_manager = entity->GetEntityManager();

                            if (!entity_manager)
                            {
                                continue;
                            }

                            if (NodeLinkComponent* node_link_component = entity_manager->TryGetComponent<NodeLinkComponent>(entity))
                            {
                                if (Handle<Node> node = node_link_component->node.Lock())
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

    m_delegate_handlers.Remove(&ui_image->OnMouseLeave);
    m_delegate_handlers.Add(ui_image->OnMouseLeave.Bind([this](const MouseEvent& event)
        {
            if (IsHoveringManipulationWidget())
            {
                SetHoveredManipulationWidget(event, nullptr, Handle<Node>::empty);
            }

            return UIEventHandlerResult::OK;
        }));

    m_delegate_handlers.Remove(&ui_image->OnMouseDrag);
    m_delegate_handlers.Add(ui_image->OnMouseDrag.Bind([this, ui_image = ui_image.Get()](const MouseEvent& event)
        {
            // prevent click being triggered on release once mouse has been dragged
            m_should_cancel_next_click = true;

            // If the mouse is currently over a manipulation widget, don't allow camera to handle the event
            if (IsHoveringManipulationWidget())
            {
                Handle<EditorManipulationWidgetBase> manipulation_widget = m_hovered_manipulation_widget.Lock();
                Handle<Node> node = m_hovered_manipulation_widget_node.Lock();

                if (!manipulation_widget || !node)
                {
                    HYP_LOG(Editor, Warning, "Failed to lock hovered manipulation widget or node");

                    return UIEventHandlerResult::ERR;
                }

                if (manipulation_widget->OnMouseMove(m_camera, event, Handle<Node>(node)))
                {
                    return UIEventHandlerResult::STOP_BUBBLING;
                }
            }

            m_camera->GetCameraController()->GetInputHandler()->OnMouseDrag(event);

            return UIEventHandlerResult::OK;
        }));

    m_delegate_handlers.Remove(&ui_image->OnMouseMove);
    m_delegate_handlers.Add(ui_image->OnMouseMove.Bind([this, ui_image = ui_image.Get()](const MouseEvent& event)
        {
            HYP_LOG(Editor, Debug, "Mouse moved in editor viewport: {}, {}", event.position.x, event.position.y);

            m_camera->GetCameraController()->GetInputHandler()->OnMouseMove(event);

            if (event.input_manager->IsMouseLocked())
            {
                const Vec2f position = ui_image->GetAbsolutePosition();
                const Vec2i size = ui_image->GetActualSize();

                // Set mouse position to previous position to keep it stationary while rotating
                event.input_manager->SetMousePosition(Vec2i(position + event.previous_position * Vec2f(size)));

                return UIEventHandlerResult::OK;
            }

            // Hover over a manipulation widget when mouse is not down
            if (!event.mouse_buttons[MouseButtonState::LEFT]
                && GetWorld()->GetGameState().IsEditor()
                && m_manipulation_widget_holder.GetSelectedManipulationMode() != EditorManipulationMode::NONE)
            {
                // Ray test the widget

                const Vec4f mouse_world = m_camera->TransformScreenToWorld(event.position);
                const Vec4f ray_direction = mouse_world.Normalized();

                const Ray ray { m_camera->GetTranslation(), ray_direction.GetXYZ() };

                RayTestResults results;

                EditorManipulationWidgetBase& manipulation_widget = m_manipulation_widget_holder.GetSelectedManipulationWidget();
                bool hit_manipulation_widget = false;

                if (manipulation_widget.GetNode()->TestRay(ray, results, /* use_bvh */ true))
                {
                    for (const RayHit& ray_hit : results)
                    {
                        ObjId<Entity> entity_id = ObjId<Entity>(ObjIdBase { TypeId::ForType<Entity>(), ray_hit.id });

                        if (!entity_id.IsValid())
                        {
                            continue;
                        }

                        Handle<Entity> entity { entity_id };
                        AssertThrow(entity.IsValid());

                        NodeLinkComponent* node_link_component = m_editor_scene->GetEntityManager()->TryGetComponent<NodeLinkComponent>(entity);

                        if (!node_link_component)
                        {
                            continue;
                        }

                        Handle<Node> node = node_link_component->node.Lock();

                        if (!node)
                        {
                            continue;
                        }

                        if (node == m_hovered_manipulation_widget_node)
                        {
                            return UIEventHandlerResult::STOP_BUBBLING;
                        }

                        if (manipulation_widget.OnMouseHover(m_camera, event, Handle<Node>(node)))
                        {
                            SetHoveredManipulationWidget(event, &manipulation_widget, Handle<Node>(node));

                            return UIEventHandlerResult::STOP_BUBBLING;
                        }
                    }
                }

                SetHoveredManipulationWidget(event, nullptr, Handle<Node>::empty);
            }

            return UIEventHandlerResult::OK;
        }));

    m_delegate_handlers.Remove(&ui_image->OnMouseDown);
    m_delegate_handlers.Add(ui_image->OnMouseDown.Bind([this, ui_image_weak = ui_image.ToWeak()](const MouseEvent& event)
        {
            if (IsHoveringManipulationWidget())
            {
                Handle<EditorManipulationWidgetBase> manipulation_widget = m_hovered_manipulation_widget.Lock();
                Handle<Node> node = m_hovered_manipulation_widget_node.Lock();

                if (!manipulation_widget || !node)
                {
                    HYP_LOG(Editor, Warning, "Failed to lock hovered manipulation widget or node");

                    return UIEventHandlerResult::ERR;
                }

                if (!manipulation_widget->IsDragging())
                {

                    const Vec4f mouse_world = m_camera->TransformScreenToWorld(event.position);
                    const Vec4f ray_direction = mouse_world.Normalized();

                    const Ray ray { m_camera->GetTranslation(), ray_direction.GetXYZ() };

                    Vec3f hitpoint;

                    RayTestResults results;

                    if (manipulation_widget->GetNode()->TestRay(ray, results, /* use_bvh */ true))
                    {
                        for (const RayHit& ray_hit : results)
                        {
                            hitpoint = ray_hit.hitpoint;

                            break;
                        }
                    }

                    manipulation_widget->OnDragStart(m_camera, event, node, hitpoint);
                }

                return UIEventHandlerResult::STOP_BUBBLING;
            }

            m_camera->GetCameraController()->GetInputHandler()->OnMouseDown(event);

            m_should_cancel_next_click = false;

            return UIEventHandlerResult::OK;
        }));

    m_delegate_handlers.Remove(&ui_image->OnMouseUp);
    m_delegate_handlers.Add(ui_image->OnMouseUp.Bind([this](const MouseEvent& event)
        {
            if (IsHoveringManipulationWidget())
            {
                Handle<EditorManipulationWidgetBase> manipulation_widget = m_hovered_manipulation_widget.Lock();
                Handle<Node> node = m_hovered_manipulation_widget_node.Lock();

                if (!manipulation_widget || !node)
                {
                    HYP_LOG(Editor, Warning, "Failed to lock hovered manipulation widget or node");

                    return UIEventHandlerResult::ERR;
                }

                if (manipulation_widget->IsDragging())
                {
                    manipulation_widget->OnDragEnd(m_camera, event, Handle<Node>(node));
                }

                return UIEventHandlerResult::STOP_BUBBLING;
            }

            m_camera->GetCameraController()->GetInputHandler()->OnMouseUp(event);

            m_should_cancel_next_click = false;

            return UIEventHandlerResult::OK;
        }));

    m_delegate_handlers.Remove(&ui_image->OnKeyDown);
    m_delegate_handlers.Add(ui_image->OnKeyDown.Bind([this](const KeyboardEvent& event)
        {
            // On escape press, stop simulating if we're currently simulating
            if (event.key_code == KeyCode::ESC && GetWorld()->GetGameState().IsSimulating())
            {
                GetWorld()->StopSimulating();

                return UIEventHandlerResult::STOP_BUBBLING;
            }

            if (event.key_code == KeyCode::TILDE)
            {
                const bool is_console_open = m_console_ui && m_console_ui->IsVisible();

                if (is_console_open)
                {
                    m_console_ui->SetIsVisible(false);

                    if (m_debug_overlay_ui_object)
                    {
                        m_debug_overlay_ui_object->SetIsVisible(true);
                    }
                }
                else
                {
                    m_console_ui->SetIsVisible(true);

                    if (m_debug_overlay_ui_object)
                    {
                        m_debug_overlay_ui_object->SetIsVisible(false);
                    }
                }

                return UIEventHandlerResult::STOP_BUBBLING;
            }

            if (m_camera->GetCameraController()->GetInputHandler()->OnKeyDown(event))
            {
                return UIEventHandlerResult::STOP_BUBBLING;
            }

            return UIEventHandlerResult::OK;
        }));

    m_delegate_handlers.Remove(&ui_image->OnKeyUp);
    m_delegate_handlers.Add(ui_image->OnKeyUp.Bind([this](const KeyboardEvent& event)
        {
            if (m_camera->GetCameraController()->GetInputHandler()->OnKeyUp(event))
            {
                return UIEventHandlerResult::STOP_BUBBLING;
            }

            return UIEventHandlerResult::OK;
        }));

    m_delegate_handlers.Remove(&ui_image->OnGainFocus);
    m_delegate_handlers.Add(ui_image->OnGainFocus.Bind([this](const MouseEvent& event)
        {
            m_editor_camera_enabled = true;

            return UIEventHandlerResult::OK;
        }));

    m_delegate_handlers.Remove(&ui_image->OnLoseFocus);
    m_delegate_handlers.Add(ui_image->OnLoseFocus.Bind([this](const MouseEvent& event)
        {
            m_editor_camera_enabled = false;

            return UIEventHandlerResult::OK;
        }));

    ui_image->SetTexture(m_scene_texture);

    InitConsoleUI();
    InitDebugOverlays();
    InitManipulationWidgetSelection();
}

void EditorSubsystem::InitSceneOutline()
{
    UISubsystem* ui_subsystem = GetWorld()->GetSubsystem<UISubsystem>();
    AssertThrow(ui_subsystem != nullptr);

    Handle<UIListView> list_view = ui_subsystem->GetUIStage()->FindChildUIObject(NAME("Outline_ListView")).Cast<UIListView>();
    AssertThrow(list_view != nullptr);

    list_view->SetInnerSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    list_view->SetDataSource(CreateObject<UIDataSource>(TypeWrapper<WeakHandle<Node>> {}));

    if (g_show_only_active_scene)
    {
        OnActiveSceneChanged
            .Bind([this](const Handle<Scene>& active_scene)
                {
                    UpdateWatchedNodes();
                })
            .Detach();
    }

    m_delegate_handlers.Remove(&list_view->OnSelectedItemChange);
    m_delegate_handlers.Add(list_view->OnSelectedItemChange.Bind([this, list_view_weak = list_view.ToWeak()](UIListViewItem* list_view_item)
        {
            Handle<UIListView> list_view = list_view_weak.Lock();

            if (!list_view)
            {
                return UIEventHandlerResult::ERR;
            }

            HYP_LOG(Editor, Debug, "Selected item changed: {}", list_view_item != nullptr ? list_view_item->GetName() : Name());

            if (!list_view_item)
            {
                SetFocusedNode(Handle<Node>::empty, false);

                return UIEventHandlerResult::OK;
            }

            const UUID data_source_element_uuid = list_view_item->GetDataSourceElementUUID();

            if (data_source_element_uuid == UUID::Invalid())
            {
                return UIEventHandlerResult::ERR;
            }

            if (!list_view->GetDataSource())
            {
                return UIEventHandlerResult::ERR;
            }

            const UIDataSourceElement* data_source_element_value = list_view->GetDataSource()->Get(data_source_element_uuid);

            if (!data_source_element_value)
            {
                return UIEventHandlerResult::ERR;
            }

            const WeakHandle<Node>& node_weak = data_source_element_value->GetValue().Get<WeakHandle<Node>>();
            Handle<Node> node = node_weak.Lock();

            SetFocusedNode(node, false);

            return UIEventHandlerResult::OK;
        }));
}

static void AddNodeToSceneOutline(const Handle<UIListView>& list_view, Node* node)
{
    AssertThrow(node != nullptr);

    // if (node->GetFlags() & NodeFlags::HIDE_IN_SCENE_OUTLINE)
    // {
    //     return;
    // }

    if (!list_view)
    {
        return;
    }

    if (UIDataSourceBase* data_source = list_view->GetDataSource())
    {
        WeakHandle<Node> editor_node_weak = node->WeakHandleFromThis();

        UUID parent_node_uuid = UUID::Invalid();

        if (Node* parent_node = node->GetParent())
        {
            parent_node_uuid = parent_node->GetUUID();

            AssertDebug(data_source->Get(parent_node_uuid) != nullptr);
        }

        data_source->Push(node->GetUUID(), HypData(std::move(editor_node_weak)), parent_node_uuid);
    }

    for (Node* child : node->GetChildren())
    {
        // if (child->GetFlags() & NodeFlags::HIDE_IN_SCENE_OUTLINE)
        // {
        //     continue;
        // }

        AddNodeToSceneOutline(list_view, child);
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
        if (g_show_only_active_scene && node->GetScene() != m_active_scene.GetUnsafe())
        {
            return;
        }
    }

    AssertThrow(node->GetScene() != nullptr);

    UISubsystem* ui_subsystem = GetWorld()->GetSubsystem<UISubsystem>();
    AssertThrow(ui_subsystem != nullptr);

    Handle<UIListView> list_view = ui_subsystem->GetUIStage()->FindChildUIObject(NAME("Outline_ListView")).Cast<UIListView>();
    AssertThrow(list_view != nullptr);

    HYP_LOG(Editor, Debug, "Start watching node: {}", *node->GetName());

    m_editor_delegates->AddNodeWatcher(
        NAME("SceneView"),
        node.Get(),
        { Node::Class()->GetProperty(NAME("Name")), 1 },
        [this, list_view_weak = list_view.ToWeak()](Node* node, const HypProperty* property)
        {
            // Update name in list view
            // @TODO: Ensure game thread

            // if (node->GetFlags() & NodeFlags::HIDE_IN_SCENE_OUTLINE)
            // {
            //     return;
            // }

            HYP_LOG(Editor, Debug, "Node {} property changed : {}", *node->GetName(), *property->GetName());

            Handle<UIListView> list_view = list_view_weak.Lock();

            if (!list_view)
            {
                return;
            }

            if (UIDataSourceBase* data_source = list_view->GetDataSource())
            {
                const UIDataSourceElement* data_source_element = data_source->Get(node->GetUUID());
                AssertThrow(data_source_element != nullptr);

                data_source->ForceUpdate(node->GetUUID());
            }
        });

    AddNodeToSceneOutline(list_view, node.Get());

    m_delegate_handlers.Remove(&node->GetDelegates()->OnChildAdded);

    m_delegate_handlers.Add(node->GetDelegates()->OnChildAdded.Bind([this, list_view_weak = list_view.ToWeak()](Node* node, bool is_direct)
        {
            if (!is_direct)
            {
                return;
            }

            AssertThrow(node != nullptr);

            // if (node->GetFlags() & NodeFlags::HIDE_IN_SCENE_OUTLINE)
            // {
            //     return;
            // }

            Handle<UIListView> list_view = list_view_weak.Lock();

            AddNodeToSceneOutline(list_view, node);
        }));

    m_delegate_handlers.Remove(&node->GetDelegates()->OnChildRemoved);

    m_delegate_handlers.Add(node->GetDelegates()->OnChildRemoved.Bind([this, list_view_weak = list_view.ToWeak()](Node* node, bool)
        {
            // If the node being removed is the focused node, clear the focused node
            if (node == m_focused_node.GetUnsafe())
            {
                SetFocusedNode(Handle<Node>::empty, true);
            }

            if (!node)
            {
                return;
            }

            Handle<UIListView> list_view = list_view_weak.Lock();

            if (!list_view)
            {
                return;
            }

            if (UIDataSourceBase* data_source = list_view->GetDataSource())
            {
                data_source->Remove(node->GetUUID());
            }
        }));
}

void EditorSubsystem::StopWatchingNode(const Handle<Node>& node)
{
    if (!node.IsValid())
    {
        return;
    }

    UISubsystem* ui_subsystem = GetWorld()->GetSubsystem<UISubsystem>();
    AssertThrow(ui_subsystem != nullptr);

    Handle<UIListView> list_view = ui_subsystem->GetUIStage()->FindChildUIObject(NAME("Outline_ListView")).Cast<UIListView>();
    AssertThrow(list_view != nullptr);

    if (const Handle<UIDataSourceBase>& data_source = list_view->GetDataSource())
    {
        data_source->Remove(node->GetUUID());
    }

    // Keep ref alive to node to prevent it from being destroyed while we're removing the watchers
    Handle<Node> node_copy = node;

    m_delegate_handlers.Remove(&node->GetDelegates()->OnChildAdded);
    m_delegate_handlers.Remove(&node->GetDelegates()->OnChildRemoved);

    m_editor_delegates->RemoveNodeWatcher(NAME("SceneView"), node.Get());
}

void EditorSubsystem::ClearWatchedNodes()
{
    HYP_SCOPE;

    UISubsystem* ui_subsystem = GetWorld()->GetSubsystem<UISubsystem>();
    AssertThrow(ui_subsystem != nullptr);

    Handle<UIListView> list_view = ui_subsystem->GetUIStage()->FindChildUIObject(NAME("Outline_ListView")).Cast<UIListView>();
    AssertThrow(list_view != nullptr);

    if (const Handle<UIDataSourceBase>& data_source = list_view->GetDataSource())
    {
        data_source->Clear();
    }

    m_editor_delegates->RemoveNodeWatchers("SceneView");
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
        if (g_show_only_active_scene)
        {
            if (Handle<Scene> active_scene = m_active_scene.Lock())
            {
                StartWatchingNode(active_scene->GetRoot());
            }

            return;
        }

        if (m_current_project.IsValid())
        {
            for (const Handle<Scene>& scene : m_current_project->GetScenes())
            {
                StartWatchingNode(scene->GetRoot());
            }

            return;
        }
    }
}

void EditorSubsystem::InitDetailView()
{
    UISubsystem* ui_subsystem = GetWorld()->GetSubsystem<UISubsystem>();
    AssertThrow(ui_subsystem != nullptr);

    Handle<UIListView> details_list_view = ui_subsystem->GetUIStage()->FindChildUIObject(NAME("Detail_View_ListView")).Cast<UIListView>();
    AssertThrow(details_list_view != nullptr);

    Handle<UIListView> outline_list_view = ui_subsystem->GetUIStage()->FindChildUIObject(NAME("Outline_ListView")).Cast<UIListView>();
    AssertThrow(outline_list_view != nullptr);

    details_list_view->SetInnerSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

    m_editor_delegates->RemoveNodeWatchers("DetailView");

    m_delegate_handlers.Remove(&OnFocusedNodeChanged);
    m_delegate_handlers.Add(OnFocusedNodeChanged.Bind([this, hyp_class = Node::Class(), details_list_view_weak = details_list_view.ToWeak(), outline_list_view_weak = outline_list_view.ToWeak()](const Handle<Node>& node, const Handle<Node>& previous_node, bool should_select_in_outline)
        {
            m_editor_delegates->RemoveNodeWatchers("DetailView");

            if (should_select_in_outline)
            {
                if (Handle<UIListView> outline_list_view = outline_list_view_weak.Lock())
                {
                    if (node.IsValid())
                    {
                        if (UIListViewItem* outline_list_view_item = outline_list_view->FindListViewItem(node->GetUUID()))
                        {
                            outline_list_view->SetSelectedItem(outline_list_view_item);
                        }
                    }
                    else
                    {
                        outline_list_view->SetSelectedItem(nullptr);
                    }
                }
            }

            Handle<UIListView> details_list_view = details_list_view_weak.Lock();

            if (!details_list_view)
            {
                return;
            }

            if (!node.IsValid())
            {
                details_list_view->SetDataSource(nullptr);

                return;
            }

            details_list_view->SetDataSource(CreateObject<UIDataSource>(TypeWrapper<EditorNodePropertyRef> {}));

            UIDataSourceBase* data_source = details_list_view->GetDataSource();

            HashMap<String, HypProperty*> properties_by_name;

            for (auto it = hyp_class->GetMembers(HypMemberType::TYPE_PROPERTY).Begin(); it != hyp_class->GetMembers(HypMemberType::TYPE_PROPERTY).End(); ++it)
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

                    properties_by_name[property->GetName().LookupString()] = property;
                }
                else
                {
                    HYP_UNREACHABLE();
                }
            }

            for (auto& it : properties_by_name)
            {
                EditorNodePropertyRef node_property_ref;
                node_property_ref.node = node.ToWeak();
                node_property_ref.property = it.second;

                if (const HypClassAttributeValue& attr = it.second->GetAttribute("label"))
                {
                    node_property_ref.title = attr.GetString();
                }
                else
                {
                    node_property_ref.title = it.first;
                }

                if (const HypClassAttributeValue& attr = it.second->GetAttribute("description"))
                {
                    node_property_ref.description = attr.GetString();
                }

                data_source->Push(UUID(), HypData(std::move(node_property_ref)));
            }

            m_editor_delegates->AddNodeWatcher(
                NAME("DetailView"),
                node,
                {},
                Proc<void(Node*, const HypProperty*)> {
                    [this, hyp_class = Node::Class(), details_list_view_weak](Node* node, const HypProperty* property)
                    {
                        HYP_LOG(Editor, Debug, "(detail) Node property changed: {}", property->GetName());

                        // Update name in list view

                        Handle<UIListView> details_list_view = details_list_view_weak.Lock();

                        if (!details_list_view)
                        {
                            HYP_LOG(Editor, Error, "Failed to get strong reference to list view!");

                            return;
                        }

                        if (UIDataSourceBase* data_source = details_list_view->GetDataSource())
                        {
                            UIDataSourceElement* data_source_element = data_source->FindWithPredicate([node, property](const UIDataSourceElement* item)
                                {
                                    return item->GetValue().Get<EditorNodePropertyRef>().property == property;
                                });

                            if (!data_source_element)
                            {
                                return;
                            }

                            data_source->ForceUpdate(data_source_element->GetUUID());
                        }
                    } });
        }));
}

void EditorSubsystem::InitConsoleUI()
{
    HYP_SCOPE;

    UISubsystem* ui_subsystem = GetWorld()->GetSubsystem<UISubsystem>();
    AssertThrow(ui_subsystem != nullptr);

    if (m_console_ui != nullptr)
    {
        m_console_ui->RemoveFromParent();
    }

    m_console_ui = ui_subsystem->GetUIStage()->CreateUIObject<ConsoleUI>(NAME("Console"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::FILL }, { 100, UIObjectSize::PIXEL }));
    AssertThrow(m_console_ui.IsValid());

    m_console_ui->SetDepth(150);
    m_console_ui->SetIsVisible(false);

    if (Handle<UIObject> scene_image_object = ui_subsystem->GetUIStage()->FindChildUIObject(NAME("Scene_Image")))
    {
        Handle<UIImage> scene_image = scene_image_object.Cast<UIImage>();
        AssertThrow(scene_image != nullptr);

        scene_image->AddChildUIObject(m_console_ui);
    }
    else
    {
        HYP_FAIL("Failed to find Scene_Image element; cannot add console UI");
    }
}

void EditorSubsystem::InitDebugOverlays()
{
    HYP_SCOPE;

    UISubsystem* ui_subsystem = GetWorld()->GetSubsystem<UISubsystem>();
    AssertThrow(ui_subsystem != nullptr);

    m_debug_overlay_ui_object = ui_subsystem->GetUIStage()->CreateUIObject<UIListView>(NAME("DebugOverlay"), Vec2i::Zero(), UIObjectSize({ 100, UIObjectSize::FILL }, { 0, UIObjectSize::AUTO }));
    m_debug_overlay_ui_object->SetDepth(100);
    m_debug_overlay_ui_object->SetBackgroundColor(Color(0.0f, 0.0f, 0.0f, 0.0f));
    m_debug_overlay_ui_object->SetParentAlignment(UIObjectAlignment::BOTTOM_LEFT);
    m_debug_overlay_ui_object->SetOriginAlignment(UIObjectAlignment::BOTTOM_LEFT);

    m_debug_overlay_ui_object->OnClick.RemoveAllDetached();
    m_debug_overlay_ui_object->OnKeyDown.RemoveAllDetached();

    for (const Handle<EditorDebugOverlayBase>& debug_overlay : m_debug_overlays)
    {
        debug_overlay->Initialize(m_debug_overlay_ui_object.Get());

        if (const Handle<UIObject>& ui_object = debug_overlay->GetUIObject())
        {
            m_debug_overlay_ui_object->AddChildUIObject(ui_object);
        }
    }

    if (Handle<UIImage> scene_image = ui_subsystem->GetUIStage()->FindChildUIObject(NAME("Scene_Image")).Cast<UIImage>())
    {
        scene_image->AddChildUIObject(m_debug_overlay_ui_object);
    }
}

void EditorSubsystem::InitManipulationWidgetSelection()
{
    HYP_SCOPE;

    UISubsystem* ui_subsystem = GetWorld()->GetSubsystem<UISubsystem>();
    AssertThrow(ui_subsystem != nullptr);

    Handle<UIObject> manipulation_widget_selection = ui_subsystem->GetUIStage()->FindChildUIObject(NAME("ManipulationWidgetSelection"));
    if (manipulation_widget_selection != nullptr)
    {
        manipulation_widget_selection->RemoveFromParent();
    }

    manipulation_widget_selection = ui_subsystem->GetUIStage()->CreateUIObject<UIMenuBar>(NAME("ManipulationWidgetSelection"), Vec2i { 0, 0 }, UIObjectSize({ 80, UIObjectSize::PIXEL }, { 12, UIObjectSize::PIXEL }));
    AssertThrow(manipulation_widget_selection != nullptr);

    manipulation_widget_selection->SetDepth(150);
    manipulation_widget_selection->SetTextSize(8.0f);
    manipulation_widget_selection->SetBackgroundColor(Color(0.0f, 0.0f, 0.0f, 0.5f));
    manipulation_widget_selection->SetTextColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
    manipulation_widget_selection->SetBorderFlags(UIObjectBorderFlags::ALL);
    manipulation_widget_selection->SetBorderRadius(5.0f);
    manipulation_widget_selection->SetPosition(Vec2i { 5, 5 });

    Array<Pair<int, Handle<UIObject>>> manipulation_widget_menu_items;
    manipulation_widget_menu_items.Reserve(m_manipulation_widget_holder.GetManipulationWidgets().Size());

    // add each manipulation widget to the selection menu
    for (const Handle<EditorManipulationWidgetBase>& manipulation_widget : m_manipulation_widget_holder.GetManipulationWidgets())
    {
        if (manipulation_widget->GetManipulationMode() == EditorManipulationMode::NONE)
        {
            continue;
        }

        Handle<UIObject> manipulation_widget_menu_item = manipulation_widget_selection->CreateUIObject<UIMenuItem>(manipulation_widget->InstanceClass()->GetName(), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::FILL }, { 100, UIObjectSize::PIXEL }));
        AssertThrow(manipulation_widget_menu_item != nullptr);

        // @TODO Add icon
        manipulation_widget_menu_item->SetText("<widget>");

        auto it = std::lower_bound(manipulation_widget_menu_items.Begin(), manipulation_widget_menu_items.End(), manipulation_widget->GetPriority(), [](const Pair<int, Handle<UIObject>>& a, int b)
            {
                return a.first < b;
            });

        manipulation_widget_menu_items.Insert(it, Pair<int, Handle<UIObject>> { manipulation_widget->GetPriority(), std::move(manipulation_widget_menu_item) });
    }

    for (Pair<int, Handle<UIObject>>& manipulation_widget_menu_item : manipulation_widget_menu_items)
    {
        manipulation_widget_selection->AddChildUIObject(std::move(manipulation_widget_menu_item.second));
    }

    if (Handle<UIObject> scene_image_object = ui_subsystem->GetUIStage()->FindChildUIObject(NAME("Scene_Image")))
    {
        Handle<UIImage> scene_image = scene_image_object.Cast<UIImage>();
        AssertThrow(scene_image != nullptr);

        scene_image->AddChildUIObject(manipulation_widget_selection);
    }
    else
    {
        HYP_FAIL("Failed to find Scene_Image element; cannot add manipulation widget selection UI");
    }
}

void EditorSubsystem::InitActiveSceneSelection()
{
    HYP_SCOPE;

    UISubsystem* ui_subsystem = GetWorld()->GetSubsystem<UISubsystem>();
    AssertThrow(ui_subsystem != nullptr);

    Handle<UIObject> active_scene_selection = ui_subsystem->GetUIStage()->FindChildUIObject(NAME("SetActiveScene_MenuItem"));
    if (!active_scene_selection.IsValid())
    {
        HYP_LOG(Editor, Warning, "Failed to find SetActiveScene_MenuItem element; creating a new one");
        return;
    }

    Handle<UIMenuItem> active_scene_menu_item = active_scene_selection.Cast<UIMenuItem>();

    if (!active_scene_menu_item.IsValid())
    {
        HYP_LOG(Editor, Error, "Failed to cast SetActiveScene_MenuItem to UIMenuItem");
        return;
    }

    // active_scene_menu_item->RemoveAllChildUIObjects();

    if (Handle<Scene> active_scene = m_active_scene.Lock())
    {
        active_scene_menu_item->SetText(HYP_FORMAT("Active Scene: {}", active_scene->GetName()));
    }
    else
    {
        active_scene_menu_item->SetText("Active Scene: None");
    }

    // OnActiveSceneChanged
    //     .Bind([this, active_scene_menu_item_weak = active_scene_menu_item.ToWeak()](const Handle<Scene>& scene)
    //         {
    //             Handle<UIMenuItem> active_scene_menu_item = active_scene_menu_item_weak.Lock();
    //             if (!active_scene_menu_item)
    //             {
    //                 HYP_LOG(Editor, Error, "Failed to lock active scene menu item from weak reference in OnActiveSceneChanged");
    //                 return UIEventHandlerResult::ERR;
    //             }

    //             if (scene.IsValid())
    //             {
    //                 Handle<UIObject> selected_sub_item = active_scene_menu_item->FindChildUIObject([&scene](UIObject* ui_object)
    //                     {
    //                         if (ui_object->IsInstanceOf<UIMenuItem>())
    //                         {
    //                             const NodeTag& tag = ui_object->GetNodeTag("Scene");
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

    //                 if (selected_sub_item.IsValid())
    //                 {
    //                     active_scene_menu_item->SetSelectedSubItem(selected_sub_item.Cast<UIMenuItem>());
    //                     active_scene_menu_item->SetText(HYP_FORMAT("Active Scene: {}", scene->GetName()));

    //                     return UIEventHandlerResult::OK;
    //                 }
    //                 else
    //                 {
    //                     HYP_LOG(Editor, Warning, "Failed to find active scene menu item for scene: {}", scene->GetName());
    //                 }
    //             }

    //             active_scene_menu_item->SetSelectedSubItem(Handle<UIMenuItem>::empty);
    //             active_scene_menu_item->SetText("Active Scene: None");

    //             return UIEventHandlerResult::OK;
    //         })
    //     .Detach();

    if (!m_current_project.IsValid())
    {
        return;
    }

    // Build each scene menu item
    for (const Handle<Scene>& scene : m_current_project->GetScenes())
    {
        if (!scene.IsValid())
        {
            continue;
        }

        Handle<UIMenuItem> scene_menu_item = active_scene_menu_item->CreateUIObject<UIMenuItem>(scene->GetName(), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::FILL }, { 100, UIObjectSize::PIXEL }));
        AssertThrow(scene_menu_item != nullptr);

        scene_menu_item->SetNodeTag(NodeTag(NAME("Scene"), scene->GetUUID()));

        scene_menu_item->SetText(scene->GetName().LookupString());

        scene_menu_item->OnClick
            .Bind([this, active_scene_menu_item_weak = active_scene_menu_item.ToWeak(), scene_menu_item_weak = scene_menu_item.ToWeak(), scene_weak = scene.ToWeak()](const MouseEvent&)
                {
                    Handle<Scene> scene = scene_weak.Lock();
                    if (!scene.IsValid())
                    {
                        HYP_LOG(Editor, Error, "Failed to lock scene from weak reference in SetActiveScene");
                        return UIEventHandlerResult::ERR;
                    }

                    SetActiveScene(scene);

                    if (Handle<UIMenuItem> active_scene_menu_item = active_scene_menu_item_weak.Lock())
                    {
                        if (Handle<UIMenuItem> scene_menu_item = scene_menu_item_weak.Lock())
                        {
                            active_scene_menu_item->SetSelectedSubItem(scene_menu_item);
                            active_scene_menu_item->SetText(HYP_FORMAT("Active Scene: {}", scene->GetName()));
                        }
                    }

                    return UIEventHandlerResult::OK;
                })
            .Detach();

        active_scene_menu_item->AddChildUIObject(std::move(scene_menu_item));
    }
}

void EditorSubsystem::InitContentBrowser()
{
    HYP_SCOPE;

    UISubsystem* ui_subsystem = GetWorld()->GetSubsystem<UISubsystem>();
    AssertThrow(ui_subsystem != nullptr);

    m_content_browser_directory_list = ui_subsystem->GetUIStage()->FindChildUIObject("ContentBrowser_Directory_List").Cast<UIListView>();
    AssertThrow(m_content_browser_directory_list != nullptr);

    m_content_browser_directory_list->SetDataSource(CreateObject<UIDataSource>(TypeWrapper<AssetPackage> {}));

    m_delegate_handlers.Remove(NAME("SelectContentDirectory"));
    m_delegate_handlers.Add(
        NAME("SelectContentDirectory"),
        m_content_browser_directory_list->OnSelectedItemChange
            .Bind([this](UIListViewItem* list_view_item)
                {
                    if (list_view_item != nullptr)
                    {
                        if (const NodeTag& asset_package_tag = list_view_item->GetNodeTag("AssetPackage"); asset_package_tag.IsValid())
                        {
                            if (Handle<AssetPackage> asset_package = GetCurrentProject()->GetAssetRegistry()->GetPackageFromPath(asset_package_tag.ToString(), /* create_if_not_exist */ false))
                            {
                                SetSelectedPackage(asset_package);

                                return;
                            }
                            else
                            {
                                HYP_LOG(Editor, Error, "Failed to get asset package from path: {}", asset_package_tag.ToString());
                            }
                        }
                    }

                    SetSelectedPackage(Handle<AssetPackage>::empty);
                }));

    m_content_browser_contents = ui_subsystem->GetUIStage()->FindChildUIObject("ContentBrowser_Contents").Cast<UIGrid>();
    AssertThrow(m_content_browser_contents != nullptr);
    m_content_browser_contents->SetDataSource(CreateObject<UIDataSource>(TypeWrapper<AssetObject> {}));
    m_content_browser_contents->SetIsVisible(false);

    m_content_browser_contents_empty = ui_subsystem->GetUIStage()->FindChildUIObject("ContentBrowser_Contents_Empty");
    AssertThrow(m_content_browser_contents_empty != nullptr);
    m_content_browser_contents_empty->SetIsVisible(true);

    Handle<UIObject> import_button = ui_subsystem->GetUIStage()->FindChildUIObject("ContentBrowser_Import_Button");
    AssertThrow(import_button != nullptr);

    m_delegate_handlers.Remove(NAME("ImportClicked"));
    m_delegate_handlers.Add(
        NAME("ImportClicked"),
        import_button->OnClick
            .Bind([this, stage_weak = ui_subsystem->GetUIStage().ToWeak()](...)
                {
                    // temp deebugging:

                    // remove a light!
                    Light* light_entity = nullptr;
                    for (auto [entity, _] : m_active_scene.GetUnsafe()->GetEntityManager()->GetEntitySet<EntityType<Light>>())
                    {
                        Light* l = static_cast<Light*>(entity);

                        if (l->GetLightType() == LT_POINT)
                        {
                            light_entity = l;
                            break;
                        }
                    }

                    if (light_entity != nullptr)
                    {
                        NodeLinkComponent* nlc = light_entity->GetEntityManager()->TryGetComponent<NodeLinkComponent>(light_entity);

                        if (nlc)
                        {
                            if (Handle<Node> node = nlc->node.Lock())
                            {
                                node->Remove();

                                HYP_LOG(Editor, Debug, "Removed light node: {}", *node->GetName());
                            }
                        }
                    }

                    return UIEventHandlerResult::STOP_BUBBLING;

                    // if (Handle<UIStage> stage = stage_weak.Lock())
                    // {
                    //     auto loaded_ui_asset = AssetManager::GetInstance()->Load<UIObject>("ui/dialog/FileBrowserDialog.ui.xml");

                    //     if (loaded_ui_asset.HasValue())
                    //     {
                    //         Handle<UIObject> loaded_ui = loaded_ui_asset->Result();

                    //         if (Handle<UIObject> file_browser_dialog = loaded_ui->FindChildUIObject("File_Browser_Dialog"))
                    //         {
                    //             stage->AddChildUIObject(file_browser_dialog);

                    //             return UIEventHandlerResult::STOP_BUBBLING;
                    //         }
                    //     }

                    //     HYP_LOG(Editor, Error, "Failed to load file browser dialog! Error: {}", loaded_ui_asset.GetError().GetMessage());
                    // }

                    // return UIEventHandlerResult::ERR;
                }));
}

void EditorSubsystem::AddPackageToContentBrowser(const Handle<AssetPackage>& package, bool nested)
{
    HYP_SCOPE;

    if (UIDataSourceBase* data_source = m_content_browser_directory_list->GetDataSource())
    {
        Handle<AssetPackage> parent_package = package->GetParentPackage().Lock();

        UUID parent_package_uuid = parent_package.IsValid() ? parent_package->GetUUID() : UUID::Invalid();

        data_source->Push(package->GetUUID(), HypData(package), parent_package_uuid);
    }

    if (nested)
    {
        for (const Handle<AssetPackage>& subpackage : package->GetSubpackages())
        {
            AddPackageToContentBrowser(subpackage, true);
        }
    }
}

void EditorSubsystem::SetSelectedPackage(const Handle<AssetPackage>& package)
{
    HYP_SCOPE;

    if (m_selected_package == package)
    {
        return;
    }

    m_delegate_handlers.Remove(NAME("OnAssetObjectAdded"));
    m_delegate_handlers.Remove(NAME("OnAssetObjectRemoved"));

    m_selected_package = package;

    m_content_browser_contents->GetDataSource()->Clear();

    if (package.IsValid())
    {
        m_delegate_handlers.Add(
            NAME("OnAssetObjectAdded"),
            package->OnAssetObjectAdded.Bind([this](AssetObject* asset_object)
                {
                    m_content_browser_contents->GetDataSource()->Push(asset_object->GetUUID(), HypData(asset_object->HandleFromThis()));
                }));

        m_delegate_handlers.Add(
            NAME("OnAssetObjectRemoved"),
            package->OnAssetObjectRemoved.Bind([this](AssetObject* asset_object)
                {
                    m_content_browser_contents->GetDataSource()->Remove(asset_object->GetUUID());
                }));

        package->ForEachAssetObject([&](const Handle<AssetObject>& asset_object)
            {
                m_content_browser_contents->GetDataSource()->Push(asset_object->GetUUID(), HypData(asset_object));

                return IterationResult::CONTINUE;
            });
    }

    HYP_LOG(Editor, Debug, "Num assets in package: {}", m_content_browser_contents->GetDataSource()->Size());

    if (m_content_browser_contents->GetDataSource()->Size() == 0)
    {
        m_content_browser_contents->SetIsVisible(false);
        m_content_browser_contents_empty->SetIsVisible(true);
    }
    else
    {
        m_content_browser_contents->SetIsVisible(true);
        m_content_browser_contents_empty->SetIsVisible(false);
    }
}

RC<FontAtlas> EditorSubsystem::CreateFontAtlas()
{
    HYP_SCOPE;

    const FilePath serialized_file_directory = GetResourceDirectory() / "data" / "fonts";
    const FilePath serialized_file_path = serialized_file_directory / "Roboto.hyp";

    if (!serialized_file_directory.Exists())
    {
        serialized_file_directory.MkDir();
    }

    if (serialized_file_path.Exists())
    {
        HypData loaded_font_atlas_data;

        FBOMReader reader({});

        if (FBOMResult err = reader.LoadFromFile(serialized_file_path, loaded_font_atlas_data))
        {
            HYP_FAIL("failed to load: %s", *err.message);
        }

        return loaded_font_atlas_data.Get<RC<FontAtlas>>();
    }

    auto font_face_asset = AssetManager::GetInstance()->Load<RC<FontFace>>("fonts/Roboto/Roboto-Regular.ttf");

    if (font_face_asset.HasError())
    {
        HYP_LOG(Editor, Error, "Failed to load font face! Error: {}", font_face_asset.GetError().GetMessage());

        return nullptr;
    }

    RC<FontAtlas> atlas = MakeRefCountedPtr<FontAtlas>(std::move(font_face_asset->Result()));
    atlas->Render();

    FileByteWriter byte_writer { serialized_file_path };
    FBOMWriter writer { FBOMWriterConfig {} };
    writer.Append(*atlas);
    auto write_err = writer.Emit(&byte_writer);
    byte_writer.Close();

    if (write_err != FBOMResult::FBOM_OK)
    {
        HYP_FAIL("Failed to save font atlas: %s", write_err.message.Data());
    }

    return atlas;
}

void EditorSubsystem::NewProject()
{
    OpenProject(CreateObject<EditorProject>());
}

void EditorSubsystem::OpenProject(const Handle<EditorProject>& project)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread);

    if (project == m_current_project)
    {
        return;
    }

    if (m_current_project)
    {
        OnProjectClosing(m_current_project);

        m_current_project->SetEditorSubsystem(WeakHandle<EditorSubsystem>::empty);

        m_current_project->Close();
    }

    if (project)
    {
        m_current_project = project;

        if (project.IsValid())
        {
            project->SetEditorSubsystem(WeakHandle<EditorSubsystem>(WeakHandleFromThis()));
        }

        OnProjectOpened(m_current_project);
    }
}

void EditorSubsystem::AddTask(const Handle<EditorTaskBase>& task)
{
    HYP_SCOPE;

    if (!task)
    {
        return;
    }

    Threads::AssertOnThread(g_game_thread);

    UISubsystem* ui_subsystem = GetWorld()->GetSubsystem<UISubsystem>();
    AssertThrow(ui_subsystem != nullptr);

    const ThreadType thread_type = task->GetRunnableThreadType();

    // @TODO Auto remove tasks that aren't on the game thread when they have completed

    RunningEditorTask& running_task = m_tasks_by_thread_type[thread_type].EmplaceBack(task);

    Handle<UIMenuItem> tasks_menu_item = ui_subsystem->GetUIStage()->FindChildUIObject(NAME("Tasks_MenuItem")).Cast<UIMenuItem>();

    if (tasks_menu_item != nullptr)
    {
        if (UIObjectSpawnContext context { tasks_menu_item })
        {
            Handle<UIGrid> task_grid = context.CreateUIObject<UIGrid>(NAME("Task_Grid"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));
            task_grid->SetNumColumns(12);

            Handle<UIGridRow> task_grid_row = task_grid->AddRow();
            task_grid_row->SetSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));

            Handle<UIGridColumn> task_grid_column_left = task_grid_row->AddColumn();
            task_grid_column_left->SetColumnSize(8);
            task_grid_column_left->AddChildUIObject(running_task.CreateUIObject(ui_subsystem->GetUIStage()));

            Handle<UIGridColumn> task_grid_column_right = task_grid_row->AddColumn();
            task_grid_column_right->SetColumnSize(4);

            Handle<UIButton> cancel_button = context.CreateUIObject<UIButton>(NAME("Task_Cancel"), Vec2i::Zero(), UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            cancel_button->SetText("Cancel");
            cancel_button->OnClick
                .Bind(
                    [task_weak = task.ToWeak()](...)
                    {
                        if (Handle<EditorTaskBase> task = task_weak.Lock())
                        {
                            task->Cancel();
                        }

                        return UIEventHandlerResult::OK;
                    })
                .Detach();
            task_grid_column_right->AddChildUIObject(cancel_button);

            running_task.m_ui_object = task_grid;

            tasks_menu_item->AddChildUIObject(task_grid);

            // testing
            Handle<Texture> dummy_icon_texture;

            if (auto dummy_icon_texture_asset = AssetManager::GetInstance()->Load<Texture>("textures/editor/icons/loading.png"))
            {
                dummy_icon_texture = dummy_icon_texture_asset->Result();
            }

            tasks_menu_item->SetIconTexture(dummy_icon_texture);
        }
    }

    // For long running tasks, enqueues the task in the scheduler
    task->Commit();

    // auto CreateTaskUIObject = [](const Handle<UIObject> &parent, const HypClass *task_hyp_class) -> Handle<UIObject>
    // {
    //     AssertThrow(parent != nullptr);
    //     AssertThrow(task_hyp_class != nullptr);

    //     return parent->CreateUIObject<UIPanel>(Vec2i::Zero(), UIObjectSize({ 400, UIObjectSize::PIXEL }, { 300, UIObjectSize::PIXEL }));
    // };

    // const Handle<UIObject> &ui_object = task->GetUIObject();

    // if (!ui_object) {
    //     if (Threads::IsOnThread(ThreadName::THREAD_GAME)) {
    //         Threads::GetThread(ThreadName::THREAD_GAME)->GetScheduler().Enqueue([this, task, CreateTaskUIObject]()
    //         {
    //             task->SetUIObject(CreateTaskUIObject(GetUIStage(), task->InstanceClass()));
    //         });
    //     } else {
    //         task->SetUIObject(CreateTaskUIObject(GetUIStage(), task->InstanceClass()));
    //     }
    // }

    // Mutex::Guard guard(m_tasks_mutex);

    // m_tasks.Insert(UUID(), task);
    // m_num_tasks.Increment(1, MemoryOrder::RELAXED);
}

void EditorSubsystem::SetFocusedNode(const Handle<Node>& focused_node, bool should_select_in_outline)
{
    if (focused_node == m_focused_node)
    {
        return;
    }

    const Handle<Node> previous_focused_node = m_focused_node.Lock();

    m_focused_node = focused_node;

    if (Handle<Node> focused_node = m_focused_node.Lock())
    {
        if (focused_node->GetScene() != nullptr)
        {
            if (const Handle<Entity>& entity = focused_node->GetEntity())
            {
                focused_node->GetScene()->GetEntityManager()->AddTag<EntityTag::EDITOR_FOCUSED>(entity);
            }
        }

        // @TODO watch for transform changes and update the highlight node

        // m_scene->GetRoot()->AddChild(m_highlight_node);
        // m_highlight_node->SetWorldScale(m_focused_node->GetWorldAABB().GetExtent() * 0.5f);
        // m_highlight_node->SetWorldTranslation(m_focused_node->GetWorldTranslation());

        // HYP_LOG(Editor, Debug, "Set focused node: {}\t{}", m_focused_node->GetName(), m_focused_node->GetWorldTranslation());
        // HYP_LOG(Editor, Debug, "Set highlight node translation: {}", m_highlight_node->GetWorldTranslation());

        if (m_manipulation_widget_holder.GetSelectedManipulationMode() == EditorManipulationMode::NONE)
        {
            m_manipulation_widget_holder.SetSelectedManipulationMode(EditorManipulationMode::TRANSLATE);
        }

        EditorManipulationWidgetBase& manipulation_widget = m_manipulation_widget_holder.GetSelectedManipulationWidget();
        manipulation_widget.UpdateWidget(focused_node);
    }

    if (previous_focused_node.IsValid() && previous_focused_node->GetScene() != nullptr)
    {
        if (const Handle<Entity>& entity = previous_focused_node->GetEntity())
        {
            previous_focused_node->GetScene()->GetEntityManager()->RemoveTag<EntityTag::EDITOR_FOCUSED>(entity);
        }
    }

    OnFocusedNodeChanged(focused_node, previous_focused_node, should_select_in_outline);
}

void EditorSubsystem::AddDebugOverlay(const Handle<EditorDebugOverlayBase>& debug_overlay)
{
    HYP_SCOPE;

    if (!debug_overlay)
    {
        return;
    }

    Threads::AssertOnThread(g_game_thread);

    UISubsystem* ui_subsystem = GetWorld()->GetSubsystem<UISubsystem>();
    AssertThrow(ui_subsystem != nullptr);

    auto it = m_debug_overlays.FindIf([name = debug_overlay->GetName()](const auto& item)
        {
            return item->GetName() == name;
        });

    if (it != m_debug_overlays.End())
    {
        return;
    }

    m_debug_overlays.PushBack(debug_overlay);

    if (m_debug_overlay_ui_object != nullptr)
    {
        debug_overlay->Initialize(ui_subsystem->GetUIStage());

        if (const Handle<UIObject>& object = debug_overlay->GetUIObject())
        {
            Handle<UIListViewItem> list_view_item = ui_subsystem->GetUIStage()->CreateUIObject<UIListViewItem>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
            list_view_item->SetBackgroundColor(Color(0.0f, 0.0f, 0.0f, 0.0f));
            list_view_item->AddChildUIObject(object);

            m_debug_overlay_ui_object->AddChildUIObject(list_view_item);
        }
    }
}

bool EditorSubsystem::RemoveDebugOverlay(WeakName name)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread);

    auto it = m_debug_overlays.FindIf([name](const auto& item)
        {
            return item->GetName() == name;
        });

    if (it == m_debug_overlays.End())
    {
        return false;
    }

    if (m_debug_overlay_ui_object != nullptr)
    {
        if (const Handle<UIObject>& object = (*it)->GetUIObject())
        {
            Handle<UIListViewItem> list_view_item = object->GetClosestParentUIObject<UIListViewItem>();

            if (list_view_item)
            {
                m_debug_overlay_ui_object->RemoveChildUIObject(list_view_item);
            }
            else
            {
                object->RemoveFromParent();
            }
        }
    }

    m_debug_overlays.Erase(it);

    return true;
}

Handle<Node> EditorSubsystem::GetFocusedNode() const
{
    return m_focused_node.Lock();
}

void EditorSubsystem::UpdateCamera(float delta)
{
    HYP_SCOPE;
}

void EditorSubsystem::UpdateTasks(float delta)
{
    HYP_SCOPE;

    for (uint32 thread_type_index = 0; thread_type_index < m_tasks_by_thread_type.Size(); thread_type_index++)
    {
        auto& tasks = m_tasks_by_thread_type[thread_type_index];

        for (auto it = tasks.Begin(); it != tasks.End();)
        {
            auto& task = it->GetTask();

            if (task->IsCommitted())
            {
                // Can only tick tasks that run on the game thread
                if (task->GetRunnableThreadType() == ThreadType::THREAD_TYPE_GAME)
                {
                    task->Tick(delta);
                }

                if (task->IsCompleted())
                {
                    task->OnComplete();

                    // Remove the UIObject for the task from this stage
                    if (const Handle<UIObject>& task_ui_object = it->GetUIObject())
                    {
                        task_ui_object->RemoveFromParent();
                    }

                    it = tasks.Erase(it);

                    continue;
                }
            }

            ++it;
        }
    }
}

void EditorSubsystem::UpdateDebugOverlays(float delta)
{
    HYP_SCOPE;

    for (const Handle<EditorDebugOverlayBase>& debug_overlay : m_debug_overlays)
    {
        if (!debug_overlay->IsEnabled())
        {
            continue;
        }

        debug_overlay->Update(delta);
    }
}

void EditorSubsystem::SetHoveredManipulationWidget(
    const MouseEvent& event,
    EditorManipulationWidgetBase* manipulation_widget,
    const Handle<Node>& manipulation_widget_node)
{
    // @TODO: On game state change, unset hovered manipulation widget

    HYP_LOG(Editor, Debug, "Set hovered manipulation widget: {}", manipulation_widget != nullptr ? manipulation_widget->InstanceClass()->GetName() : Name());

    if (m_hovered_manipulation_widget.IsValid() && m_hovered_manipulation_widget_node.IsValid())
    {
        Handle<Node> hovered_manipulation_widget_node = m_hovered_manipulation_widget_node.Lock();
        Handle<EditorManipulationWidgetBase> hovered_manipulation_widget = m_hovered_manipulation_widget.Lock();

        if (hovered_manipulation_widget_node && hovered_manipulation_widget)
        {
            hovered_manipulation_widget->OnMouseLeave(m_camera, event, Handle<Node>(hovered_manipulation_widget_node));
        }
    }

    if (manipulation_widget != nullptr)
    {
        m_hovered_manipulation_widget = manipulation_widget->WeakHandleFromThis();
    }
    else
    {
        m_hovered_manipulation_widget.Reset();
    }

    m_hovered_manipulation_widget_node = manipulation_widget_node;
}

void EditorSubsystem::SetActiveScene(const WeakHandle<Scene>& scene)
{
    HYP_SCOPE;

    if (scene == m_active_scene)
    {
        return;
    }

    m_active_scene = scene;
    HYP_LOG(Editor, Debug, "Set active scene: {}", m_active_scene.IsValid() ? m_active_scene.Lock()->GetName() : Name::Invalid());

    OnActiveSceneChanged(m_active_scene.Lock());
}

#endif

#pragma endregion EditorSubsystem

} // namespace hyperion
