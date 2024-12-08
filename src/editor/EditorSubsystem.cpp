/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorSubsystem.hpp>
#include <editor/EditorDelegates.hpp>
#include <editor/EditorCamera.hpp>
#include <editor/EditorTask.hpp>

#include <scene/Scene.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/NodeLinkComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>

#include <asset/Assets.hpp>
#include <asset/serialization/fbom/FBOMReader.hpp>
#include <asset/serialization/fbom/FBOMWriter.hpp>

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

#include <core/system/AppContext.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <rendering/UIRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/render_components/ScreenCapture.hpp>

#include <rendering/font/FontAtlas.hpp>

// temp
#include <rendering/lightmapper/LightmapperSubsystem.hpp>
#include <rendering/lightmapper/LightmapUVBuilder.hpp>

#include <math/MathUtil.hpp>

#include <scripting/ScriptingService.hpp>

#include <Engine.hpp>

#include <util/profiling/ProfileScope.hpp>
#include <util/MeshBuilder.hpp>

namespace hyperion {

HYP_DEFINE_LOG_CHANNEL(Editor);

#pragma region RunningEditorTask

RC<UIObject> RunningEditorTask::CreateUIObject(UIStage *ui_stage) const
{
    AssertThrow(ui_stage != nullptr);

    RC<UIPanel> panel = ui_stage->CreateUIObject<UIPanel>(NAME("EditorTaskPanel"), Vec2i::Zero(), UIObjectSize({ 100, UIObjectSize::FILL }, { 100, UIObjectSize::FILL }));
    panel->SetBackgroundColor(Color(0xFF0000FF)); // testing

    RC<UIText> task_title = ui_stage->CreateUIObject<UIText>(NAME("Task_Title"), Vec2i::Zero(), UIObjectSize(UIObjectSize::AUTO));
    task_title->SetTextSize(16.0f);
    task_title->SetText(m_task->InstanceClass()->GetName().LookupString());
    panel->AddChildUIObject(task_title);

    return panel;
}

#pragma endregion RunningEditorTask

#pragma region GenerateLightmapsEditorTask

GenerateLightmapsEditorTask::GenerateLightmapsEditorTask(const Handle<World> &world, const Handle<Scene> &scene)
    : m_world(world),
      m_scene(scene),
      m_task(nullptr)
{
    AssertThrow(m_world.IsValid());
    AssertThrow(m_scene.IsValid());
}

void GenerateLightmapsEditorTask::Process()
{
    HYP_SCOPE;
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    HYP_LOG(Editor, LogLevel::INFO, "Generating lightmaps");

    LightmapperSubsystem *lightmapper_subsystem = m_world->GetSubsystem<LightmapperSubsystem>();
    
    if (!lightmapper_subsystem) {
        lightmapper_subsystem = m_world->AddSubsystem<LightmapperSubsystem>();
    }

    m_task = lightmapper_subsystem->GenerateLightmaps(m_scene);
}

void GenerateLightmapsEditorTask::Cancel_Impl()
{
    if (m_task != nullptr) {
        m_task->Cancel();
    }
}

bool GenerateLightmapsEditorTask::IsCompleted_Impl() const
{
    return m_task == nullptr || m_task->IsCompleted();
}

void GenerateLightmapsEditorTask::Tick_Impl(float delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    if (m_task != nullptr) {
        if (m_task->IsCompleted()) {
            m_task = nullptr;
        }
    }
}

#pragma endregion GenerateLightmapsEditorTask

#pragma region EditorSubsystem

#ifdef HYP_EDITOR

EditorSubsystem::EditorSubsystem(const RC<AppContext> &app_context, const Handle<Scene> &scene, const Handle<Camera> &camera, const RC<UIStage> &ui_stage)
    : m_app_context(app_context),
      m_scene(scene),
      m_camera(camera),
      m_ui_stage(ui_stage),
      m_editor_camera_enabled(false),
      m_should_cancel_next_click(false)
{
    m_editor_delegates = new EditorDelegates();
}

EditorSubsystem::~EditorSubsystem()
{
    delete m_editor_delegates;
}

void EditorSubsystem::Initialize()
{
    HYP_SCOPE;

    m_camera->SetCameraController(MakeRefCountedPtr<EditorCameraController>());
    m_scene->GetEnvironment()->AddRenderComponent<UIRenderer>(NAME("EditorUIRenderer"), m_ui_stage);

    const Vec2i window_size = m_app_context->GetMainWindow()->GetDimensions();

    RC<ScreenCaptureRenderComponent> screen_capture_component = m_scene->GetEnvironment()->AddRenderComponent<ScreenCaptureRenderComponent>(NAME("EditorSceneCapture"), Vec2u(window_size));
    m_scene_texture = screen_capture_component->GetTexture();

    CreateEditorUI();
    CreateHighlightNode();

    if (Handle<AssetCollector> base_asset_collector = g_asset_manager->GetBaseAssetCollector()) {
        base_asset_collector->StartWatching();
    }

    g_asset_manager->OnAssetCollectorAdded.Bind([](const Handle<AssetCollector> &asset_collector)
    {
        asset_collector->StartWatching();
    });

    g_asset_manager->OnAssetCollectorRemoved.Bind([](const Handle<AssetCollector> &asset_collector)
    {
        asset_collector->StopWatching();
    });
}

void EditorSubsystem::Shutdown()
{
    HYP_SCOPE;

    m_scene->GetEnvironment()->RemoveRenderComponent<ScreenCaptureRenderComponent>(NAME("EditorSceneCapture"));
    m_scene->GetEnvironment()->RemoveRenderComponent<UIRenderer>(NAME("EditorUIRenderer"));
}

void EditorSubsystem::Update(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_GAME | ThreadName::THREAD_TASK);

    m_editor_delegates->Update();

    UpdateCamera(delta);
    UpdateTasks(delta);
}

void EditorSubsystem::OnSceneAttached(const Handle<Scene> &scene)
{
    HYP_SCOPE;
}

void EditorSubsystem::OnSceneDetached(const Handle<Scene> &scene)
{
    HYP_SCOPE;
}

void EditorSubsystem::CreateEditorUI()
{
    RC<FontAtlas> font_atlas = CreateFontAtlas();
    m_ui_stage->SetDefaultFontAtlas(font_atlas);

    if (auto loaded_ui_asset = AssetManager::GetInstance()->Load<RC<UIObject>>("ui/Editor.Main.ui.xml"); loaded_ui_asset.IsOK()) {
        RC<UIStage> loaded_ui = loaded_ui_asset.Result().Cast<UIStage>();
        AssertThrowMsg(loaded_ui != nullptr, "Failed to load editor UI");

        loaded_ui->SetOwnerThreadID(ThreadID::Current());
        loaded_ui->SetDefaultFontAtlas(font_atlas);

        if (RC<UIObject> scene_image_object = loaded_ui->FindChildUIObject(NAME("Scene_Image"))) {
            RC<UIImage> ui_image = scene_image_object.Cast<UIImage>();
            
            if (ui_image != nullptr) {
                ui_image->OnClick.Bind([this](const MouseEvent &event)
                {
                    HYP_LOG(Editor, LogLevel::DEBUG, "Click at : {}", event.position);

                    if (m_should_cancel_next_click) {
                        return UIEventHandlerResult::STOP_BUBBLING;
                    }

                    if (m_camera->GetCameraController()->GetInputHandler()->OnClick(event)) {
                        return UIEventHandlerResult::STOP_BUBBLING;
                    }

                    const Vec4f mouse_world = m_camera->TransformScreenToWorld(event.position);

                    const Vec4f ray_direction = mouse_world.Normalized();

                    const Ray ray { m_camera->GetTranslation(), ray_direction.GetXYZ() };
                    RayTestResults results;

                    if (m_scene->GetOctree().TestRay(ray, results)) {
                        for (const RayHit &hit : results) {
                            if (ID<Entity> entity = ID<Entity>(hit.id)) {
                                HYP_LOG(Editor, LogLevel::INFO, "Hit: {}", entity.Value());

                                if (NodeLinkComponent *node_link_component = m_scene->GetEntityManager()->TryGetComponent<NodeLinkComponent>(entity)) {
                                    if (RC<Node> node = node_link_component->node.Lock()) {
                                        SetFocusedNode(NodeProxy(node));

                                        break;
                                    }
                                }
                            }
                        }

                        return UIEventHandlerResult::STOP_BUBBLING;
                    }

                    return UIEventHandlerResult::OK;
                }).Detach();

                ui_image->OnMouseDrag.Bind([this, ui_image = ui_image.Get()](const MouseEvent &event)
                {
                    m_camera->GetCameraController()->GetInputHandler()->OnMouseDrag(event);

                    // prevent click being triggered on release once mouse has been dragged
                    m_should_cancel_next_click = true;

                    if (m_camera->GetCameraController()->IsMouseLocked()) {
                        const Vec2f position = ui_image->GetAbsolutePosition();
                        const Vec2i size = ui_image->GetActualSize();
                        const Vec2i window_size = { m_camera->GetWidth(), m_camera->GetHeight() };

                        // Set mouse position to previous position to keep it stationary while rotating
                        event.input_manager->SetMousePosition(Vec2i(position + event.previous_position * Vec2f(size)));
                    }

                    return UIEventHandlerResult::OK;
                }).Detach();

                ui_image->OnMouseDown.Bind([this, ui_image_weak = ui_image.ToWeak()](const MouseEvent &event)
                {
                    HYP_LOG(Editor, LogLevel::DEBUG, "Mouse down on UI image, computed depth: {}", ui_image_weak.Lock()->GetComputedDepth());
                    
                    m_camera->GetCameraController()->GetInputHandler()->OnMouseDown(event);

                    m_should_cancel_next_click = false;

                    return UIEventHandlerResult::OK;
                }).Detach();

                ui_image->OnMouseUp.Bind([this](const MouseEvent &event)
                {
                    m_camera->GetCameraController()->GetInputHandler()->OnMouseUp(event);

                    m_should_cancel_next_click = false;

                    return UIEventHandlerResult::OK;
                }).Detach();

                ui_image->OnKeyDown.Bind([this](const KeyboardEvent &event)
                {
                    if (m_camera->GetCameraController()->GetInputHandler()->OnKeyDown(event)) {
                        return UIEventHandlerResult::STOP_BUBBLING;
                    }

                    return UIEventHandlerResult::OK;
                }).Detach();

                ui_image->OnGainFocus.Bind([this](const MouseEvent &event)
                {
                    m_editor_camera_enabled = true;

                    return UIEventHandlerResult::OK;
                }).Detach();

                ui_image->OnLoseFocus.Bind([this](const MouseEvent &event)
                {
                    m_editor_camera_enabled = false;

                    return UIEventHandlerResult::OK;
                }).Detach();

                ui_image->SetTexture(m_scene_texture);
            }
        } else {
            HYP_FAIL("Failed to find Scene_Image element");
        }
        
        GetUIStage()->AddChildUIObject(loaded_ui);

        // test generate lightmap
        if (RC<UIObject> generate_lightmaps_button = loaded_ui->FindChildUIObject(NAME("Generate_Lightmaps_Button"))) {

            generate_lightmaps_button->OnClick.RemoveAll();
            generate_lightmaps_button->OnClick.Bind([this](...)
            {
                HYP_LOG(Editor, LogLevel::INFO, "Generate lightmaps clicked!");

                AddTask(MakeRefCountedPtr<GenerateLightmapsEditorTask>(GetWorld()->HandleFromThis(), m_scene));

                // LightmapperSubsystem *lightmapper_subsystem = GetWorld()->GetSubsystem<LightmapperSubsystem>();

                // if (!lightmapper_subsystem) {
                //     lightmapper_subsystem = GetWorld()->AddSubsystem<LightmapperSubsystem>();
                // }

                // Task<void> *generate_lightmaps_task = lightmapper_subsystem->GenerateLightmaps(m_scene);

                return UIEventHandlerResult::OK;
            }).Detach();
        }

        InitSceneOutline();
        InitDetailView();
    }

    g_engine->GetScriptingService()->OnScriptStateChanged.Bind([](const ManagedScript &script)
    {
        DebugLog(LogType::Debug, "Script state changed: now is %u\n", script.state);
    }).Detach();
}

void EditorSubsystem::CreateHighlightNode()
{
    m_highlight_node = NodeProxy(MakeRefCountedPtr<Node>("Editor_Highlight"));

    const Handle<Entity> entity = m_scene->GetEntityManager()->AddEntity();

    Handle<Mesh> mesh = MeshBuilder::Cube();
    InitObject(mesh);

    Handle<Material> material = g_material_system->GetOrCreate(
        {
            .shader_definition = ShaderDefinition {
                NAME("Forward"),
                ShaderProperties(mesh->GetVertexAttributes())
            },
            .bucket = Bucket::BUCKET_TRANSLUCENT
        },
        {
            { Material::MATERIAL_KEY_ALBEDO, Vec4f(1.0f) },
            { Material::MATERIAL_KEY_ROUGHNESS, 0.0f },
            { Material::MATERIAL_KEY_METALNESS, 0.0f }
        }
    );

    InitObject(material);

    m_scene->GetEntityManager()->AddComponent<MeshComponent>(
        entity,
        MeshComponent {
            mesh,
            material
        }
    );

    m_scene->GetEntityManager()->AddComponent<TransformComponent>(
        entity,
        TransformComponent { }
    );

    m_scene->GetEntityManager()->AddComponent<VisibilityStateComponent>(
        entity,
        VisibilityStateComponent {
            VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE
        }
    );

    m_scene->GetEntityManager()->AddComponent<BoundingBoxComponent>(
        entity,
        BoundingBoxComponent {
            mesh->GetAABB()
        }
    );

    m_highlight_node->SetEntity(entity);
}

void EditorSubsystem::InitSceneOutline()
{
    RC<UIListView> list_view = GetUIStage()->FindChildUIObject(NAME("Scene_Outline_ListView")).Cast<UIListView>();
    AssertThrow(list_view != nullptr);

    list_view->SetInnerSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    
    list_view->SetDataSource(MakeRefCountedPtr<UIDataSource>(TypeWrapper<Weak<Node>> { }));
    
    list_view->OnSelectedItemChange.Bind([this, list_view_weak = list_view.ToWeak()](UIListViewItem *list_view_item)
    {
        RC<UIListView> list_view = list_view_weak.Lock();

        if (!list_view) {
            return UIEventHandlerResult::ERR;
        }

        HYP_LOG(Editor, LogLevel::DEBUG, "Selected item changed: {}", list_view_item != nullptr ? list_view_item->GetName() : Name());

        if (!list_view_item) {
            SetFocusedNode(NodeProxy::empty);

            return UIEventHandlerResult::OK;
        }

        const UUID data_source_element_uuid = list_view_item->GetDataSourceElementUUID();

        if (data_source_element_uuid == UUID::Invalid()) {
            return UIEventHandlerResult::ERR;
        }

        if (!list_view->GetDataSource()) {
            return UIEventHandlerResult::ERR;
        }

        const UIDataSourceElement *data_source_element_value = list_view->GetDataSource()->Get(data_source_element_uuid);

        if (!data_source_element_value) {
            return UIEventHandlerResult::ERR;
        }

        const Weak<Node> &node_weak = data_source_element_value->GetValue().Get<Weak<Node>>();
        RC<Node> node_rc = node_weak.Lock();

        // const UUID &associated_node_uuid = data_source_element_value->GetValue

        // NodeProxy associated_node = GetScene()->GetRoot()->FindChildByUUID(associated_node_uuid);

        if (!node_rc) {
            return UIEventHandlerResult::ERR;
        }

        SetFocusedNode(NodeProxy(std::move(node_rc)));

        return UIEventHandlerResult::OK;
    }).Detach();

    m_editor_delegates->AddNodeWatcher(NAME("SceneView"), m_scene->GetRoot().Get(), { Node::Class()->GetProperty(NAME("Name")), 1 }, [this, hyp_class = GetClass<Node>(), list_view_weak = list_view.ToWeak()](Node *node, const HypProperty *property)
    {
       HYP_LOG(Editor, LogLevel::DEBUG, "Property changed for Node {}: {}", node->GetName(), property->GetName());

       // Update name in list view
       // @TODO: Ensure game thread

       RC<UIListView> list_view = list_view_weak.Lock();

       if (!list_view) {
           return;
       }

       if (UIDataSourceBase *data_source = list_view->GetDataSource()) {
           const UIDataSourceElement *data_source_element = data_source->Get(node->GetUUID());
           AssertThrow(data_source_element != nullptr);

           data_source->Set(node->GetUUID(), HypData(node->WeakRefCountedPtrFromThis()));
       }
    });

    m_editor_delegates->AddNodeWatcher(NAME("SceneView"), m_scene->GetRoot(), { Node::Class()->GetProperty(NAME("Flags")), 1 }, [this, hyp_class = GetClass<Node>(), list_view_weak = list_view.ToWeak()](Node *node, const HypProperty *property)
    {
       AssertThrow(node != nullptr);

       RC<UIListView> list_view = list_view_weak.Lock();

       if (!list_view) {
           return;
       }

       if (UIDataSourceBase *data_source = list_view->GetDataSource()) {
           const UIDataSourceElement *data_source_element = data_source->Get(node->GetUUID());

           if (node->GetFlags() & NodeFlags::HIDE_IN_SCENE_OUTLINE) {
               if (!data_source_element) {
                   return;
               }

               data_source->Remove(node->GetUUID());
           } else {
               if (data_source_element) {
                   return;
               }

               Weak<Node> editor_node_weak = node->WeakRefCountedPtrFromThis();

               UUID parent_node_uuid = UUID::Invalid();

               if (Node *parent_node = node->GetParent(); parent_node && !parent_node->IsRoot()) {
                   parent_node_uuid = parent_node->GetUUID();
               }

               data_source->Push(node->GetUUID(), HypData(std::move(editor_node_weak)), parent_node_uuid);
           }
       }
    });

    m_scene->GetRoot()->GetDelegates()->OnChildAdded.Bind([this, list_view_weak = list_view.ToWeak()](Node *node, bool)
    {
        AssertThrow(node != nullptr);

        if (node->GetFlags() & NodeFlags::HIDE_IN_SCENE_OUTLINE) {
            return;
        }

        HYP_LOG(Editor, LogLevel::DEBUG, "Node added: {}", node->GetName());

        RC<UIListView> list_view = list_view_weak.Lock();

        if (!list_view) {
            return;
        }

        if (node->IsRoot()) {
            return;
        }

        if (UIDataSourceBase *data_source = list_view->GetDataSource()) {
            Weak<Node> editor_node_weak = node->WeakRefCountedPtrFromThis();

            UUID parent_node_uuid = UUID::Invalid();

            if (Node *parent_node = node->GetParent(); parent_node && !parent_node->IsRoot()) {
                parent_node_uuid = parent_node->GetUUID();
            }

            data_source->Push(node->GetUUID(), HypData(std::move(editor_node_weak)), parent_node_uuid);
        }
    }).Detach();

    m_scene->GetRoot()->GetDelegates()->OnChildRemoved.Bind([list_view_weak = list_view.ToWeak()](Node *node, bool)
    {
        if (!node || (node->GetFlags() & NodeFlags::HIDE_IN_SCENE_OUTLINE)) {
            return;
        }

        RC<UIListView> list_view = list_view_weak.Lock();

        if (!list_view) {
            return;
        }

        if (UIDataSourceBase *data_source = list_view->GetDataSource()) {
            AssertThrow(data_source->Remove(node->GetUUID()));
        }
    }).Detach();
}

void EditorSubsystem::InitDetailView()
{
    RC<UIListView> list_view = GetUIStage()->FindChildUIObject(NAME("Detail_View_ListView")).Cast<UIListView>();
    AssertThrow(list_view != nullptr);

    list_view->SetInnerSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

    OnFocusedNodeChanged.Bind([this, hyp_class = Node::Class(), list_view_weak = list_view.ToWeak()](const NodeProxy &node, const NodeProxy &previous_node)
    {
        m_editor_delegates->RemoveNodeWatcher(NAME("DetailView"));

        RC<UIListView> list_view = list_view_weak.Lock();

        if (!list_view) {
            return;
        }

        list_view->SetDataSource(nullptr);

        if (!node.IsValid()) {
            HYP_LOG(Editor, LogLevel::DEBUG, "Focused node is invalid!");

            return;
        }

        HYP_LOG(Editor, LogLevel::DEBUG, "Focused node: {}", node->GetName());

        list_view->SetDataSource(MakeRefCountedPtr<UIDataSource>(TypeWrapper<EditorNodePropertyRef> { }));

        UIDataSourceBase *data_source = list_view->GetDataSource();
        
        HashMap<String, HypProperty *> properties_by_name;

        for (auto it = hyp_class->GetMembers(HypMemberType::TYPE_PROPERTY).Begin(); it != hyp_class->GetMembers(HypMemberType::TYPE_PROPERTY).End(); ++it) {
            if (HypProperty *property = dynamic_cast<HypProperty *>(&*it)) {
                if (!property->CanGet() || !property->CanSet()) {
                    continue;
                }

                properties_by_name[property->GetName().LookupString()] = property;
            } else {
                HYP_UNREACHABLE();
            }
        }

        for (auto &it : properties_by_name) {
            EditorNodePropertyRef node_property_ref;
            node_property_ref.node = node.ToWeak();
            node_property_ref.property = it.second;

            if (const HypClassAttributeValue &attr = it.second->GetAttribute("label")) {
                node_property_ref.title = attr.GetString();
            } else {
                node_property_ref.title = it.first;
            }

            if (const HypClassAttributeValue &attr = it.second->GetAttribute("description")) {
                node_property_ref.description = attr.GetString();
            }

            HYP_LOG(Editor, LogLevel::DEBUG, "Push property {} (title: {}) to data source", it.first, node_property_ref.title);

            data_source->Push(UUID(), HypData(std::move(node_property_ref)));
        }

        m_editor_delegates->RemoveNodeWatcher(NAME("DetailView"));

        m_editor_delegates->AddNodeWatcher(NAME("DetailView"), m_focused_node.Get(), {}, Proc<void, Node *, const HypProperty *> { [this, hyp_class = Node::Class(), list_view_weak](Node *node, const HypProperty *property)
        {
            HYP_LOG(Editor, LogLevel::DEBUG, "(detail) Node property changed: {}", property->GetName());

            // Update name in list view

            RC<UIListView> list_view = list_view_weak.Lock();

            if (!list_view) {
                HYP_LOG(Editor, LogLevel::ERR, "Failed to get strong reference to list view!");

                return;
            }

            if (UIDataSourceBase *data_source = list_view->GetDataSource()) {
                UIDataSourceElement *data_source_element = data_source->FindWithPredicate([node, property](const UIDataSourceElement *item)
                {
                    return item->GetValue().Get<EditorNodePropertyRef>().property == property;
                });

                AssertThrow(data_source_element != nullptr);
                data_source->ForceUpdate(data_source_element->GetUUID());

                // // trigger update to rebuild UI
                // EditorNodePropertyRef &node_property_ref = data_source_element->GetValue().Get<EditorNodePropertyRef>();

                // // temp; sanity check.
                // RC<Node> node_rc = node_property_ref.node.Lock();
                // AssertThrow(node_rc != nullptr);
                
                // data_source->Set(data_source_element->GetUUID(), HypData(&node_property_ref));
            }
        } });
    }).Detach();
}

RC<FontAtlas> EditorSubsystem::CreateFontAtlas()
{
    const FilePath serialized_file_directory = AssetManager::GetInstance()->GetBasePath() / "data" / "fonts";
    const FilePath serialized_file_path = serialized_file_directory / "Roboto.hyp";

    if (!serialized_file_directory.Exists()) {
        serialized_file_directory.MkDir();
    }

    if (serialized_file_path.Exists()) {
        HypData loaded_font_atlas_data;

        fbom::FBOMReader reader({ });

        if (fbom::FBOMResult err = reader.LoadFromFile(serialized_file_path, loaded_font_atlas_data)) {
            HYP_FAIL("failed to load: %s", *err.message);
        }

        return loaded_font_atlas_data.Get<RC<FontAtlas>>();
    }

    auto font_face_asset = AssetManager::GetInstance()->Load<RC<FontFace>>("fonts/Roboto/Roboto-Regular.ttf");

    if (!font_face_asset.IsOK()) {
        HYP_LOG(Editor, LogLevel::ERR, "Failed to load font face!");

        return nullptr;
    }

    RC<FontAtlas> atlas = MakeRefCountedPtr<FontAtlas>(std::move(font_face_asset.Result()));
    atlas->Render();

    FileByteWriter byte_writer { serialized_file_path };
    fbom::FBOMWriter writer { fbom::FBOMWriterConfig { } };
    writer.Append(*atlas);
    auto write_err = writer.Emit(&byte_writer);
    byte_writer.Close();

    if (write_err != fbom::FBOMResult::FBOM_OK) {
        HYP_FAIL("Failed to save font atlas: %s", write_err.message.Data());
    }

    return atlas;
}

void EditorSubsystem::AddTask(const RC<IEditorTask> &task)
{
    if (!task) {
        return;
    }

    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    const ThreadType thread_type = task->GetRunnableThreadType();

    // @TODO Auto remove tasks that aren't on the game thread when they have completed

    RunningEditorTask &running_task = m_tasks_by_thread_type[thread_type].EmplaceBack(task);

    RC<UIMenuItem> tasks_menu_item = m_ui_stage->FindChildUIObject(NAME("Tasks_MenuItem")).Cast<UIMenuItem>();
    AssertThrow(tasks_menu_item != nullptr);

    if (UIObjectSpawnContext context { tasks_menu_item }) {
        RC<UIGrid> task_grid = context.CreateUIObject<UIGrid>(NAME("Task_Grid"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));
        task_grid->SetNumColumns(12);

        RC<UIGridRow> task_grid_row = task_grid->AddRow();
        task_grid_row->SetSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));

        RC<UIGridColumn> task_grid_column_left = task_grid_row->AddColumn();
        task_grid_column_left->SetColumnSize(8);
        task_grid_column_left->AddChildUIObject(running_task.CreateUIObject(m_ui_stage));

        RC<UIGridColumn> task_grid_column_right = task_grid_row->AddColumn();
        task_grid_column_right->SetColumnSize(4);

        RC<UIButton> cancel_button = context.CreateUIObject<UIButton>(NAME("Task_Cancel"), Vec2i::Zero(), UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
        cancel_button->SetText("Cancel");
        cancel_button->OnClick.Bind([task_weak = task.ToWeak()](...)
        {
            if (RC<IEditorTask> task = task_weak.Lock()) {
                task->Cancel();
            }

            return UIEventHandlerResult::OK;
        }).Detach();
        task_grid_column_right->AddChildUIObject(cancel_button);

        running_task.m_ui_object = task_grid;

        tasks_menu_item->AddChildUIObject(task_grid);

        // testing
        Handle<Texture> dummy_icon_texture;

        if (auto dummy_icon_texture_asset = AssetManager::GetInstance()->Load<Texture>("textures/editor/icons/loading.png")) {
            dummy_icon_texture = dummy_icon_texture_asset.Result();
        }

        tasks_menu_item->SetIconTexture(dummy_icon_texture);
    }

    // For long running tasks, enqueues the task in the scheduler
    task->Commit();

    // auto CreateTaskUIObject = [](const RC<UIObject> &parent, const HypClass *task_hyp_class) -> RC<UIObject>
    // {
    //     AssertThrow(parent != nullptr);
    //     AssertThrow(task_hyp_class != nullptr);

    //     return parent->CreateUIObject<UIPanel>(Vec2i::Zero(), UIObjectSize({ 400, UIObjectSize::PIXEL }, { 300, UIObjectSize::PIXEL }));
    // };

    // const RC<UIObject> &ui_object = task->GetUIObject();

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

void EditorSubsystem::SetFocusedNode(const NodeProxy &focused_node)
{
    const NodeProxy previous_focused_node = m_focused_node;

    m_focused_node = focused_node;

    // m_highlight_node.Remove();

    if (m_focused_node.IsValid()) {
        // @TODO watch for transform changes and update the highlight node

        // m_focused_node->AddChild(m_highlight_node);
        m_highlight_node->SetWorldScale(m_focused_node->GetWorldAABB().GetExtent() * 0.5f);
        m_highlight_node->SetWorldTranslation(m_focused_node->GetWorldTranslation());
    }

    OnFocusedNodeChanged(m_focused_node, previous_focused_node);
}

void EditorSubsystem::UpdateCamera(GameCounter::TickUnit delta)
{
    static constexpr float speed = 15.0f;

    if (!m_editor_camera_enabled) {
        return;
    }

    Vec3f translation = m_camera->GetTranslation();

    const Vec3f direction = m_camera->GetDirection();
    const Vec3f dir_cross_y = direction.Cross(m_camera->GetUpVector());

    InputManager *input_manager = m_app_context->GetInputManager().Get();
    AssertThrow(input_manager != nullptr);

    if (input_manager->IsKeyDown(KeyCode::KEY_W)) {
        translation += direction * delta * speed;
    }
    if (input_manager->IsKeyDown(KeyCode::KEY_S)) {
        translation -= direction * delta * speed;
    }
    if (input_manager->IsKeyDown(KeyCode::KEY_A)) {
        translation -= dir_cross_y * delta * speed;
    }
    if (input_manager->IsKeyDown(KeyCode::KEY_D)) {
        translation += dir_cross_y * delta * speed;
    }

    m_camera->SetNextTranslation(translation);
}

void EditorSubsystem::UpdateTasks(GameCounter::TickUnit delta)
{
    for (uint32 thread_type_index = 0; thread_type_index < m_tasks_by_thread_type.Size(); thread_type_index++) {
        auto &tasks = m_tasks_by_thread_type[thread_type_index];

        for (auto it = tasks.Begin(); it != tasks.End();) {
            auto &task = it->GetTask();

            // Can only tick tasks that run on the game thread
            if (task->GetRunnableThreadType() == ThreadType::THREAD_TYPE_GAME) {
                task->Tick(delta);
            }

            if (task->IsCompleted()) {
                // Remove the UIObject for the task from this stage
                if (const RC<UIObject> &task_ui_object = it->GetUIObject()) {
                    task_ui_object->RemoveFromParent();
                }

                it = tasks.Erase(it);
            } else {
                ++it;
            }
        }
    }
}

#endif

#pragma endregion EditorSubsystem

} // namespace hyperion
