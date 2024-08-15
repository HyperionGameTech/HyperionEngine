#include <editor/HyperionEditor.hpp>
#include <editor/EditorCamera.hpp>
#include <editor/EditorObjectProperties.hpp>
#include <editor/EditorDelegates.hpp>

#include <rendering/Texture.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/UIRenderer.hpp>
#include <rendering/render_components/ScreenCapture.hpp>

#include <rendering/debug/DebugDrawer.hpp>

#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/SkyComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/AudioComponent.hpp>
#include <scene/ecs/components/LightComponent.hpp>
#include <scene/ecs/components/ShadowMapComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/TerrainComponent.hpp>
#include <scene/ecs/components/EnvGridComponent.hpp>
#include <scene/ecs/components/RigidBodyComponent.hpp>
#include <scene/ecs/components/BLASComponent.hpp>
#include <scene/ecs/components/ScriptComponent.hpp>

#include <scene/world_grid/terrain/TerrainWorldGridPlugin.hpp>
#include <scene/world_grid/WorldGrid.hpp>

#include <asset/AssetBatch.hpp>
#include <asset/Assets.hpp>

#include <ui/UIObject.hpp>
#include <ui/UIText.hpp>
#include <ui/UIButton.hpp>
#include <ui/UIPanel.hpp>
#include <ui/UITabView.hpp>
#include <ui/UIMenuBar.hpp>
#include <ui/UIGrid.hpp>
#include <ui/UIImage.hpp>
#include <ui/UIDockableContainer.hpp>
#include <ui/UIListView.hpp>
#include <ui/UITextbox.hpp>
#include <ui/UIDataSource.hpp>

#include <core/logging/Logger.hpp>

#include <core/net/HTTPRequest.hpp>

#include <scripting/Script.hpp>
#include <scripting/ScriptingService.hpp>

#include <util/profiling/Profile.hpp>

#include <util/MeshBuilder.hpp>

#include <rendering/lightmapper/LightmapperSubsystem.hpp>
#include <rendering/lightmapper/LightmapUVBuilder.hpp>

#include <core/system/SystemEvent.hpp>

#include <core/config/Config.hpp>

// temp
#include <core/object/HypClass.hpp>
#include <core/object/HypProperty.hpp>
#include <core/object/HypData.hpp>

namespace hyperion {

HYP_DEFINE_LOG_CHANNEL(Editor);

namespace editor {

class Vec3fUIDataSourceElementFactory : public UIDataSourceElementFactory<Vec3f>
{
public:
    virtual RC<UIObject> CreateUIObject_Internal(UIStage *stage, const Vec3f &value) const override
    {
        const HypClass *hyp_class = GetClass<Vec3f>();

        RC<UIGrid> grid = stage->CreateUIObject<UIGrid>(Name::Unique("Vec3fPanel"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
        grid->SetNumColumns(3);

        RC<UIGridRow> row = grid->AddRow();

        {
            RC<UIGridColumn> col = row->AddColumn();

            RC<UIPanel> panel = stage->CreateUIObject<UIPanel>(Name::Unique(), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            panel->SetPadding({ 5, 2 });
            
            RC<UITextbox> textbox = stage->CreateUIObject<UITextbox>(Name::Unique(), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
            textbox->SetText(HYP_FORMAT("{}", value.x));
            panel->AddChildUIObject(textbox); 

            col->AddChildUIObject(panel);
        }

        {
            RC<UIGridColumn> col = row->AddColumn();

            RC<UIPanel> panel = stage->CreateUIObject<UIPanel>(Name::Unique(), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            panel->SetPadding({ 5, 2 });

            RC<UITextbox> textbox = stage->CreateUIObject<UITextbox>(Name::Unique(), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
            textbox->SetText(HYP_FORMAT("{}", value.y));
            panel->AddChildUIObject(textbox);

            col->AddChildUIObject(panel);
        }

        {
            RC<UIGridColumn> col = row->AddColumn();

            RC<UIPanel> panel = stage->CreateUIObject<UIPanel>(Name::Unique(), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            panel->SetPadding({ 5, 2 });

            RC<UITextbox> textbox = stage->CreateUIObject<UITextbox>(Name::Unique(), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
            textbox->SetText(HYP_FORMAT("{}", value.z));
            panel->AddChildUIObject(textbox);

            col->AddChildUIObject(panel);
        }

        return grid;
    }

    virtual void UpdateUIObject_Internal(UIObject *ui_object, const Vec3f &value) const override
    {
        // @TODO
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(Vec3f, Vec3fUIDataSourceElementFactory);


class TransformUIDataSourceElementFactory : public UIDataSourceElementFactory<Transform>
{
public:
    virtual RC<UIObject> CreateUIObject_Internal(UIStage *stage, const Transform &value) const override
    {
        const HypClass *hyp_class = GetClass<Transform>();

        RC<UIPanel> panel = stage->CreateUIObject<UIPanel>(NAME("TransformPanel"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
        
        RC<UITextbox> translation_textbox = stage->CreateUIObject<UITextbox>(NAME("TranslationTextbox"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
        translation_textbox->SetText(HYP_FORMAT("Translation: {}", value.GetTranslation()));
        panel->AddChildUIObject(translation_textbox);

        RC<UITextbox> rotation_textbox = stage->CreateUIObject<UITextbox>(NAME("RotationTextbox"), Vec2i { 0, 20 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
        rotation_textbox->SetText(HYP_FORMAT("Rotation: {}", value.GetRotation()));
        panel->AddChildUIObject(rotation_textbox);

        RC<UITextbox> scale_textbox = stage->CreateUIObject<UITextbox>(NAME("ScaleTextbox"), Vec2i { 0, 40 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
        scale_textbox->SetText(HYP_FORMAT("Scale: {}", value.GetScale()));
        panel->AddChildUIObject(scale_textbox);

        return panel;

        // RC<UIGrid> grid = stage->CreateUIObject<UIGrid>(Name::Unique("TransformPanel"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
        // grid->SetNumColumns(1);

        // for (const HypProperty *property : hyp_class->GetProperties()) {
        //     if (!property->HasGetter()) {
        //         continue;
        //     }

        //     RC<UIGridRow> row = grid->AddRow();

        //     fbom::FBOMData property_value = property->InvokeGetter(value);

        //     if (Optional<fbom::FBOMDeserializedObject &> deserialized_object = property_value.GetDeserializedObject()) {
        //         const TypeID property_value_type_id = deserialized_object->any_value.GetTypeID();

        //         IUIDataSourceElementFactory *element_factory = UIDataSourceElementFactoryRegistry::GetInstance().GetFactory(property_value_type_id);

        //         if (element_factory) {
        //             RC<UIObject> element = element_factory->CreateUIObject(stage, deserialized_object->any_value.ToRef());
        //             row->AddChildUIObject(element);
        //         } else {
        //             HYP_LOG(Editor, LogLevel::ERR, "No UI element factory found for type ID: {}; cannot render element", property_value_type_id.Value());
        //         }
        //     } else {
        //         HYP_LOG(Editor, LogLevel::ERR, "Property value is not a deserialized object; cannot render element");
        //     }
        // }

        // return grid;
    }

    virtual void UpdateUIObject_Internal(UIObject *ui_object, const Transform &value) const override
    {
        ui_object->FindChildUIObject(NAME("TranslationTextbox"))
            .Cast<UITextbox>()
            ->SetText(HYP_FORMAT("Translation: {}", value.GetTranslation()));

        ui_object->FindChildUIObject(NAME("RotationTextbox"))
            .Cast<UITextbox>()
            ->SetText(HYP_FORMAT("Rotation: {}", value.GetRotation()));

        ui_object->FindChildUIObject(NAME("ScaleTextbox"))
            .Cast<UITextbox>()
            ->SetText(HYP_FORMAT("Scale: {}", value.GetScale()));
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(Transform, TransformUIDataSourceElementFactory);

class EditorWeakNodeFactory : public UIDataSourceElementFactory<Weak<Node>>
{
public:
    virtual RC<UIObject> CreateUIObject_Internal(UIStage *stage, const Weak<Node> &value) const override
    {
        String node_name = "Invalid";

        if (RC<Node> node_rc = value.Lock()) {
            node_name = node_rc->GetName();
        }

        RC<UIText> text = stage->CreateUIObject<UIText>(Name::Unique(), Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::AUTO }, { 14, UIObjectSize::PIXEL }));
        text->SetText(node_name);
        return text;
    }

    virtual void UpdateUIObject_Internal(UIObject *ui_object, const Weak<Node> &value) const override
    {
        String node_name = "Invalid";

        if (RC<Node> node_rc = value.Lock()) {
            node_name = node_rc->GetName();
        }

        if (UIText *text = dynamic_cast<UIText *>(ui_object)) {
            text->SetText(node_name);
        }
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(Weak<Node>, EditorWeakNodeFactory);

struct EditorNodePropertyRef
{
    Weak<Node>              node;
    const HypProperty  *property = nullptr;
};

class EditorNodePropertyFactory : public UIDataSourceElementFactory<EditorNodePropertyRef>
{
public:
    virtual RC<UIObject> CreateUIObject_Internal(UIStage *stage, const EditorNodePropertyRef &value) const override
    {
        RC<Node> node_rc = value.node.Lock();
        if (!node_rc) {
            return nullptr;
        }

        // Create panel
        RC<UIPanel> panel = stage->CreateUIObject<UIPanel>(NAME("PropertyPanel"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
        panel->SetBackgroundColor(Vec4f(0.2f, 0.2f, 0.2f, 1.0f));

        {
            RC<UIText> header_text = stage->CreateUIObject<UIText>(NAME("Header"), Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::AUTO }, { 12, UIObjectSize::PIXEL }));
            header_text->SetText(*value.property->name);
            header_text->SetTextColor(Vec4f(1.0f, 1.0f, 1.0f, 1.0f));
            header_text->SetBackgroundColor(Vec4f(0.1f, 0.1f, 0.1f, 1.0f));

            panel->AddChildUIObject(header_text);
        }

        // fbom::FBOMData property_value = value.property->InvokeGetter(*node_rc);

        // if (Optional<fbom::FBOMDeserializedObject &> deserialized_object = property_value.GetDeserializedObject()) {
        //     const TypeID property_value_type_id = deserialized_object->any_value.GetTypeID();

        //     IUIDataSourceElementFactory *element_factory = UIDataSourceElementFactoryRegistry::GetInstance().GetFactory(property_value_type_id);

        //     if (element_factory) {
        //         // RC<UIPanel> sub_panel = stage->CreateUIObject<UIPanel>(NAME("PropertySubPanel"), Vec2i { 0, 25 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

        //         RC<UIObject> element = element_factory->CreateUIObject(stage, deserialized_object->any_value.ToRef());
        //         panel->AddChildUIObject(element);

        //         // panel->AddChildUIObject(sub_panel);
        //     } else {
        //         HYP_LOG(Editor, LogLevel::ERR, "No UI element factory found for type ID: {}; cannot render element", property_value_type_id.Value());
        //     }
        // } else {
        //     HYP_LOG(Editor, LogLevel::ERR, "Property value is not a deserialized object; cannot render element");
        // }

        return panel;
    }

    virtual void UpdateUIObject_Internal(UIObject *ui_object, const EditorNodePropertyRef &value) const override
    {
        RC<Node> node_rc = value.node.Lock();
        if (!node_rc) {
            return;
        }

        // fbom::FBOMData property_value = value.property->InvokeGetter(*node_rc);

        // if (Optional<fbom::FBOMDeserializedObject &> deserialized_object = property_value.GetDeserializedObject()) {
        //     const TypeID property_value_type_id = deserialized_object->any_value.GetTypeID();

        //     IUIDataSourceElementFactory *element_factory = UIDataSourceElementFactoryRegistry::GetInstance().GetFactory(property_value_type_id);

        //     if (element_factory) {
        //         element_factory->UpdateUIObject(ui_object, deserialized_object->any_value.ToRef());
        //     } else {
        //         HYP_LOG(Editor, LogLevel::ERR, "No UI element factory found for type ID: {}; cannot render element", property_value_type_id.Value());
        //     }
        // } else {
        //     HYP_LOG(Editor, LogLevel::ERR, "Property value is not a deserialized object; cannot render element");
        // }
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(EditorNodePropertyRef, EditorNodePropertyFactory);

#pragma region HyperionEditorImpl

class HyperionEditorImpl
{
public:
    HyperionEditorImpl(const Handle<Scene> &scene, const Handle<Camera> &camera, InputManager *input_manager, const RC<UIStage> &ui_stage)
        : m_scene(scene),
          m_camera(camera),
          m_input_manager(input_manager),
          m_ui_stage(ui_stage),
          m_editor_camera_enabled(false),
          m_should_cancel_next_click(false)
    {
    }

    ~HyperionEditorImpl() = default;

    const Handle<Scene> &GetScene() const
        { return m_scene; }

    const Handle<Camera> &GetCamera() const
        { return m_camera; }

    const RC<UIStage> &GetUIStage() const
        { return m_ui_stage; }

    const Handle<Texture> &GetSceneTexture() const
        { return m_scene_texture; }

    void SetSceneTexture(const Handle<Texture> &texture)
        { m_scene_texture = texture; }

    void Initialize();

    void UpdateEditorCamera(GameCounter::TickUnit delta);

private:
    void CreateHighlightNode();

    RC<FontAtlas> CreateFontAtlas();
    void CreateMainPanel();

    void InitSceneOutline();
    void InitDetailView();

    RC<UIObject> CreateSceneOutline();
    RC<UIObject> CreateDetailView();
    RC<UIObject> CreateBottomPanel();

    void CreateInitialState();

    void SetFocusedNode(const NodeProxy &node);

    Handle<Scene>                                           m_scene;
    Handle<Camera>                                          m_camera;
    InputManager                                            *m_input_manager;
    RC<UIStage>                                             m_ui_stage;
    Handle<Texture>                                         m_scene_texture;
    RC<UIObject>                                            m_main_panel;

    NodeProxy                                               m_focused_node;
    // the actual node that displays the highlight for the focused item
    NodeProxy                                               m_highlight_node;

    bool                                                    m_editor_camera_enabled;
    bool                                                    m_should_cancel_next_click;

    Delegate<void, const NodeProxy &, const NodeProxy &>    OnFocusedNodeChanged;
};

void HyperionEditorImpl::CreateHighlightNode()
{
    m_highlight_node = NodeProxy(new Node("Editor_Highlight"));

    const ID<Entity> entity = GetScene()->GetEntityManager()->AddEntity();

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

    GetScene()->GetEntityManager()->AddComponent<MeshComponent>(
        entity,
        MeshComponent {
            mesh,
            material
        }
    );

    GetScene()->GetEntityManager()->AddComponent<TransformComponent>(
        entity,
        TransformComponent { }
    );

    GetScene()->GetEntityManager()->AddComponent<VisibilityStateComponent>(
        entity,
        VisibilityStateComponent {
            VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE
        }
    );

    GetScene()->GetEntityManager()->AddComponent<BoundingBoxComponent>(
        entity,
        BoundingBoxComponent {
            mesh->GetAABB()
        }
    );

    m_highlight_node->SetEntity(entity);

    // temp
    // GetScene()->GetRoot()->AddChild(m_highlight_node);
}

RC<FontAtlas> HyperionEditorImpl::CreateFontAtlas()
{
    auto font_face_asset = AssetManager::GetInstance()->Load<RC<FontFace>>("fonts/Roboto/Roboto-Regular.ttf");

    if (!font_face_asset.IsOK()) {
        HYP_LOG(Editor, LogLevel::ERR, "Failed to load font face!");

        return nullptr;
    }

    RC<FontAtlas> atlas(new FontAtlas(std::move(font_face_asset.Result())));
    atlas->Render();

    return atlas;
}

void HyperionEditorImpl::CreateMainPanel()
{
    RC<FontAtlas> font_atlas = CreateFontAtlas();
    GetUIStage()->SetDefaultFontAtlas(font_atlas);

    if (auto loaded_ui_asset = AssetManager::GetInstance()->Load<RC<UIObject>>("ui/Editor.Main.ui.xml"); loaded_ui_asset.IsOK()) {
        auto loaded_ui = loaded_ui_asset.Result();

        if (loaded_ui.Is<UIStage>()) {
            loaded_ui.Cast<UIStage>()->SetOwnerThreadID(ThreadID::Current());
        }

        loaded_ui.Cast<UIStage>()->SetDefaultFontAtlas(font_atlas);

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

                    const Vec4f mouse_world = GetScene()->GetCamera()->TransformScreenToWorld(event.position);

                    const Vec4f ray_direction = mouse_world.Normalized();

                    const Ray ray { GetScene()->GetCamera()->GetTranslation(), ray_direction.GetXYZ() };
                    RayTestResults results;

                    if (GetScene()->GetOctree().TestRay(ray, results)) {
                        for (const RayHit &hit : results) {
                            if (ID<Entity> entity = ID<Entity>(hit.id)) {
                                HYP_LOG(Editor, LogLevel::INFO, "Hit: {}", entity.Value());

                                if (NodeProxy node = GetScene()->GetRoot()->FindChildWithEntity(entity)) {
                                    HYP_LOG(Editor, LogLevel::INFO, "  Hit name: {}", node->GetName());

                                    SetFocusedNode(node);

                                    break;
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

                ui_image->OnMouseDown.Bind([this](const MouseEvent &event)
                {
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
        }
        
        GetUIStage()->AddChildUIObject(loaded_ui);

        // test generate lightmap
        if (RC<UIObject> generate_lightmaps_button = loaded_ui->FindChildUIObject(NAME("Generate_Lightmaps_Button"))) {

            generate_lightmaps_button->OnClick.RemoveAll();
            generate_lightmaps_button->OnClick.Bind([this](...)
            {
                HYP_LOG(Editor, LogLevel::INFO, "Generate lightmaps clicked!");

                LightmapperSubsystem *lightmapper_subsystem = g_engine->GetWorld()->GetSubsystem<LightmapperSubsystem>();

                if (!lightmapper_subsystem) {
                    lightmapper_subsystem = g_engine->GetWorld()->AddSubsystem<LightmapperSubsystem>();
                }

                lightmapper_subsystem->GenerateLightmaps(m_scene);

                return UIEventHandlerResult::OK;
            }).Detach();
        }

        return;

        // overflowing inner sizes is messing up AABB calculation for higher up parents

        InitSceneOutline();
        InitDetailView();
    }

    g_engine->GetScriptingService()->OnScriptStateChanged.Bind([](const ManagedScript &script)
    {
        DebugLog(LogType::Debug, "Script state changed: now is %u\n", script.state);
    }).Detach();
}

void HyperionEditorImpl::InitSceneOutline()
{
    RC<UIListView> list_view = GetUIStage()->FindChildUIObject(NAME("Scene_Outline_ListView")).Cast<UIListView>();
    AssertThrow(list_view != nullptr);

    list_view->SetInnerSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    
    UniquePtr<UIDataSource<Weak<Node>>> temp_data_source(new UIDataSource<Weak<Node>>());
    list_view->SetDataSource(std::move(temp_data_source));
    
    list_view->OnSelectedItemChange.Bind([this, list_view_weak = list_view.ToWeak()](UIListViewItem *list_view_item)
    {
        RC<UIListView> list_view = list_view_weak.Lock();

        if (!list_view) {
            return UIEventHandlerResult::ERR;
        }

        HYP_LOG(Editor, LogLevel::DEBUG, "Selected item changed: {}", list_view_item->GetName());

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

        const IUIDataSourceElement *data_source_element_value = list_view->GetDataSource()->Get(data_source_element_uuid);

        if (!data_source_element_value) {
            return UIEventHandlerResult::ERR;
        }

        const Weak<Node> &node_weak = data_source_element_value->GetValue<Weak<Node>>();
        RC<Node> node_rc = node_weak.Lock();

        // const UUID &associated_node_uuid = data_source_element_value->GetValue

        // NodeProxy associated_node = GetScene()->GetRoot()->FindChildByUUID(associated_node_uuid);

        if (!node_rc) {
            return UIEventHandlerResult::ERR;
        }

        SetFocusedNode(NodeProxy(std::move(node_rc)));

        return UIEventHandlerResult::OK;
    }).Detach();

    EditorDelegates::GetInstance().AddNodeWatcher(NAME("SceneView"), { NAME("Name") }, [this, hyp_class = GetClass<Node>(), list_view_weak = list_view.ToWeak()](Node *node, Name name, ConstAnyRef value)
    {
        HYP_LOG(Editor, LogLevel::DEBUG, "(scene) Node property changed: {}", name);

        // Update name in list view
        // @TODO: Ensure game thread

        RC<UIListView> list_view = list_view_weak.Lock();

        if (!list_view) {
            return;
        }

        if (UIDataSourceBase *data_source = list_view->GetDataSource()) {
            const IUIDataSourceElement *data_source_element = data_source->FindWithPredicate([node](const IUIDataSourceElement *item)
            {
                return item->GetValue<Weak<Node>>() == node;
            });

            if (data_source_element != nullptr) {
                Weak<Node> node_ref = data_source_element->GetValue<Weak<Node>>();
                
                data_source->Set(
                    data_source_element->GetUUID(),
                    AnyRef(&node_ref)
                );
            }
        }
    });

    GetScene()->GetRoot()->GetDelegates()->OnNestedNodeAdded.Bind([this, list_view_weak = list_view.ToWeak()](const NodeProxy &node, bool)
    {
        RC<UIListView> list_view = list_view_weak.Lock();

        if (!list_view) {
            return;
        }

        AssertThrow(node.IsValid());

        if (UIDataSourceBase *data_source = list_view->GetDataSource()) {
            Weak<Node> editor_node_weak = node.ToWeak();

            data_source->Push(editor_node_weak);
        }

        EditorDelegates::GetInstance().WatchNode(node.Get());
    }).Detach();

    GetScene()->GetRoot()->GetDelegates()->OnNestedNodeRemoved.Bind([list_view_weak = list_view.ToWeak()](const NodeProxy &node, bool)
    {
        EditorDelegates::GetInstance().UnwatchNode(node.Get());

        RC<UIListView> list_view = list_view_weak.Lock();

        if (!list_view) {
            return;
        }

        if (UIDataSourceBase *data_source = list_view->GetDataSource()) {
            data_source->RemoveAllWithPredicate([&node](IUIDataSourceElement *item)
            {
                return item->GetValue<Weak<Node>>() == node.Get();
            });
        }
    }).Detach();
}

void HyperionEditorImpl::InitDetailView()
{
    RC<UIListView> list_view = GetUIStage()->FindChildUIObject(NAME("Detail_View_ListView")).Cast<UIListView>();
    AssertThrow(list_view != nullptr);

    list_view->SetInnerSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

    OnFocusedNodeChanged.Bind([this, hyp_class = GetClass<Node>(), list_view_weak = list_view.ToWeak()](const NodeProxy &node, const NodeProxy &previous_node)
    {
        RC<UIListView> list_view = list_view_weak.Lock();

        if (!list_view) {
            return;
        }

        list_view->SetDataSource(nullptr);

        // stop watching using currently bound function
        EditorDelegates::GetInstance().RemoveNodeWatcher(NAME("DetailView"));

        if (!node.IsValid()) {
            HYP_LOG(Editor, LogLevel::DEBUG, "Focused node is invalid!");

            return;
        }

        { // create new data source
            UniquePtr<UIDataSource<EditorNodePropertyRef>> data_source(new UIDataSource<EditorNodePropertyRef>());
            list_view->SetDataSource(std::move(data_source));
        }

        UIDataSourceBase *data_source = list_view->GetDataSource();

        for (const HypProperty *property : hyp_class->GetProperties()) {
            HYP_LOG(Editor, LogLevel::DEBUG, "Property: {}", property->name.LookupString());

            if (!property->HasGetter()) {
                continue;
            }

            EditorNodePropertyRef node_property_ref { node.ToWeak(), property };

            data_source->Push(&node_property_ref);
        }

        EditorDelegates::GetInstance().AddNodeWatcher(NAME("DetailView"), {}, [this, hyp_class = GetClass<Node>(), list_view_weak](Node *node, Name name, ConstAnyRef value)
        {
            if (node != m_focused_node.Get()) {
                return;
            }

            HYP_LOG(Editor, LogLevel::DEBUG, "(detail) Node property changed: {}", name);

            // Update name in list view

            RC<UIListView> list_view = list_view_weak.Lock();

            if (!list_view) {
                return;
            }

            if (UIDataSourceBase *data_source = list_view->GetDataSource()) {
                const IUIDataSourceElement *data_source_element = data_source->FindWithPredicate([node, name](const IUIDataSourceElement *item)
                {
                    return item->GetValue<EditorNodePropertyRef>().property->name == name;
                });

                AssertThrow(data_source_element != nullptr);

                if (data_source_element != nullptr) {
                    // trigger update to rebuild UI
                    EditorNodePropertyRef node_property_ref = data_source_element->GetValue<EditorNodePropertyRef>();
                    
                    data_source->Set(
                        data_source_element->GetUUID(),
                        AnyRef(&node_property_ref)
                    );
                }
            }
        });
    }).Detach();
}


RC<UIObject> HyperionEditorImpl::CreateSceneOutline()
{
    RC<UIPanel> scene_outline = GetUIStage()->CreateUIObject<UIPanel>(NAME("Scene_Outline"), Vec2i { 0, 0 }, UIObjectSize({ 200, UIObjectSize::PIXEL }, { 100, UIObjectSize::PERCENT }));

    RC<UIPanel> scene_outline_header = GetUIStage()->CreateUIObject<UIPanel>(NAME("Scene_Outline_Header"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 25, UIObjectSize::PIXEL }));
    RC<UIText> scene_outline_header_text = GetUIStage()->CreateUIObject<UIText>(NAME("Scene_Outline_Header_Text"), Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::AUTO }, { 10, UIObjectSize::PIXEL }));
    scene_outline_header_text->SetOriginAlignment(UIObjectAlignment::CENTER);
    scene_outline_header_text->SetParentAlignment(UIObjectAlignment::CENTER);
    scene_outline_header_text->SetText("Scene");
    scene_outline_header_text->SetTextColor(Vec4f::One());
    scene_outline_header->AddChildUIObject(scene_outline_header_text);
    scene_outline->AddChildUIObject(scene_outline_header);

    // @TODO: make tree view
    UniquePtr<UIDataSource<Weak<Node>>> temp_data_source(new UIDataSource<Weak<Node>>());
    RC<UIListView> list_view = GetUIStage()->CreateUIObject<UIListView>(NAME("Scene_Outline_ListView"), Vec2i { 0, 25 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::FILL }));
    list_view->SetInnerSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    
#if 1
    list_view->SetDataSource(std::move(temp_data_source));
    
    list_view->OnSelectedItemChange.Bind([this, list_view_weak = list_view.ToWeak()](UIListViewItem *list_view_item)
    {
        RC<UIListView> list_view = list_view_weak.Lock();

        if (!list_view) {
            return UIEventHandlerResult::ERR;
        }

        HYP_LOG(Editor, LogLevel::DEBUG, "Selected item changed: {}", list_view_item->GetName());

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

        const IUIDataSourceElement *data_source_element_value = list_view->GetDataSource()->Get(data_source_element_uuid);

        if (!data_source_element_value) {
            return UIEventHandlerResult::ERR;
        }

        const Weak<Node> &node_weak = data_source_element_value->GetValue<Weak<Node>>();
        RC<Node> node_rc = node_weak.Lock();

        // const UUID &associated_node_uuid = data_source_element_value->GetValue

        // NodeProxy associated_node = GetScene()->GetRoot()->FindChildByUUID(associated_node_uuid);

        if (!node_rc) {
            return UIEventHandlerResult::ERR;
        }

        SetFocusedNode(NodeProxy(std::move(node_rc)));

        return UIEventHandlerResult::OK;
    }).Detach();

    EditorDelegates::GetInstance().AddNodeWatcher(NAME("SceneView"), { NAME("Name") }, [this, hyp_class = GetClass<Node>(), list_view_weak = list_view.ToWeak()](Node *node, Name name, ConstAnyRef value)
    {
        HYP_LOG(Editor, LogLevel::DEBUG, "(scene) Node property changed: {}", name);

        // Update name in list view
        // @TODO: Ensure game thread

        RC<UIListView> list_view = list_view_weak.Lock();

        if (!list_view) {
            return;
        }

        if (UIDataSourceBase *data_source = list_view->GetDataSource()) {
            const IUIDataSourceElement *data_source_element = data_source->FindWithPredicate([node](const IUIDataSourceElement *item)
            {
                return item->GetValue<Weak<Node>>() == node;
            });

            if (data_source_element != nullptr) {
                Weak<Node> node_ref = data_source_element->GetValue<Weak<Node>>();
                
                data_source->Set(
                    data_source_element->GetUUID(),
                    AnyRef(&node_ref)
                );
            }
        }
    });

    GetScene()->GetRoot()->GetDelegates()->OnNestedNodeAdded.Bind([this, list_view_weak = list_view.ToWeak()](const NodeProxy &node, bool)
    {
        RC<UIListView> list_view = list_view_weak.Lock();

        if (!list_view) {
            return;
        }

        AssertThrow(node.IsValid());

        if (UIDataSourceBase *data_source = list_view->GetDataSource()) {
            Weak<Node> editor_node_weak = node.ToWeak();

            data_source->Push(editor_node_weak);
        }

        EditorDelegates::GetInstance().WatchNode(node.Get());
    }).Detach();

    GetScene()->GetRoot()->GetDelegates()->OnNestedNodeRemoved.Bind([list_view_weak = list_view.ToWeak()](const NodeProxy &node, bool)
    {
        EditorDelegates::GetInstance().UnwatchNode(node.Get());

        RC<UIListView> list_view = list_view_weak.Lock();

        if (!list_view) {
            return;
        }

        if (UIDataSourceBase *data_source = list_view->GetDataSource()) {
            data_source->RemoveAllWithPredicate([&node](IUIDataSourceElement *item)
            {
                return item->GetValue<Weak<Node>>() == node.Get();
            });
        }
    }).Detach();
#endif

    scene_outline->AddChildUIObject(list_view);

    return scene_outline;
}

RC<UIObject> HyperionEditorImpl::CreateDetailView()
{
    RC<UIPanel> detail_view = GetUIStage()->CreateUIObject<UIPanel>(NAME("Detail_View"), Vec2i { 0, 0 }, UIObjectSize({ 200, UIObjectSize::PIXEL }, { 100, UIObjectSize::PERCENT }));

    RC<UIPanel> detail_view_header = GetUIStage()->CreateUIObject<UIPanel>(NAME("Detail_View_Header"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 25, UIObjectSize::PIXEL }));
    RC<UIText> detail_view_header_text = GetUIStage()->CreateUIObject<UIText>(NAME("Detail_View_Header_Text"), Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::AUTO }, { 10, UIObjectSize::PIXEL }));
    detail_view_header_text->SetOriginAlignment(UIObjectAlignment::CENTER);
    detail_view_header_text->SetParentAlignment(UIObjectAlignment::CENTER);
    detail_view_header_text->SetText("Properties");
    detail_view_header_text->SetTextColor(Vec4f::One());
    detail_view_header->AddChildUIObject(detail_view_header_text);
    detail_view->AddChildUIObject(detail_view_header);

    RC<UIListView> list_view = GetUIStage()->CreateUIObject<UIListView>(NAME("Detail_View_ListView"), Vec2i { 0, 25 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::FILL }));
    list_view->SetInnerSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    detail_view->AddChildUIObject(list_view);

    OnFocusedNodeChanged.Bind([this, hyp_class = GetClass<Node>(), list_view_weak = list_view.ToWeak()](const NodeProxy &node, const NodeProxy &previous_node)
    {
        RC<UIListView> list_view = list_view_weak.Lock();

        if (!list_view) {
            return;
        }

        list_view->SetDataSource(nullptr);

        // stop watching using currently bound function
        EditorDelegates::GetInstance().RemoveNodeWatcher(NAME("DetailView"));

        if (!node.IsValid()) {
            HYP_LOG(Editor, LogLevel::DEBUG, "Focused node is invalid!");

            return;
        }

        { // create new data source
            UniquePtr<UIDataSource<EditorNodePropertyRef>> data_source(new UIDataSource<EditorNodePropertyRef>());
            list_view->SetDataSource(std::move(data_source));
        }

        UIDataSourceBase *data_source = list_view->GetDataSource();

        for (const HypProperty *property : hyp_class->GetProperties()) {
            HYP_LOG(Editor, LogLevel::DEBUG, "Property: {}", property->name.LookupString());

            if (!property->HasGetter()) {
                continue;
            }

            EditorNodePropertyRef node_property_ref { node.ToWeak(), property };

            data_source->Push(&node_property_ref);
        }

        EditorDelegates::GetInstance().AddNodeWatcher(NAME("DetailView"), {}, [this, hyp_class = GetClass<Node>(), list_view_weak](Node *node, Name name, ConstAnyRef value)
        {
            if (node != m_focused_node.Get()) {
                return;
            }

            HYP_LOG(Editor, LogLevel::DEBUG, "(detail) Node property changed: {}", name);

            // Update name in list view

            RC<UIListView> list_view = list_view_weak.Lock();

            if (!list_view) {
                return;
            }

            if (UIDataSourceBase *data_source = list_view->GetDataSource()) {
                const IUIDataSourceElement *data_source_element = data_source->FindWithPredicate([node, name](const IUIDataSourceElement *item)
                {
                    return item->GetValue<EditorNodePropertyRef>().property->name == name;
                });

                AssertThrow(data_source_element != nullptr);

                if (data_source_element != nullptr) {
                    // trigger update to rebuild UI
                    EditorNodePropertyRef node_property_ref = data_source_element->GetValue<EditorNodePropertyRef>();
                    
                    data_source->Set(
                        data_source_element->GetUUID(),
                        AnyRef(&node_property_ref)
                    );
                }
            }
        });
    }).Detach();

    return detail_view;
}

RC<UIObject> HyperionEditorImpl::CreateBottomPanel()
{
    RC<UIPanel> bottom_panel = GetUIStage()->CreateUIObject<UIPanel>(NAME("Bottom_Panel"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 200, UIObjectSize::PIXEL }));

    RC<UITabView> tab_view = GetUIStage()->CreateUIObject<UITabView>(NAME("Bottom_Panel_Tab_View"), Vec2i { 0, 30 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));
    tab_view->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
    tab_view->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);

    RC<UITab> asset_browser_tab = tab_view->AddTab(NAME("Asset_Browser_Tab"), "Assets");

    RC<UIListView> asset_browser_list_view = GetUIStage()->CreateUIObject<UIListView>(NAME("Asset_Browser_ListView"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));
    asset_browser_list_view->SetInnerSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

    asset_browser_tab->GetContents()->AddChildUIObject(asset_browser_list_view);

    bottom_panel->AddChildUIObject(tab_view);
    
    return bottom_panel;
}

void HyperionEditorImpl::CreateInitialState()
{
    // Add Skybox
    auto skybox_entity = m_scene->GetEntityManager()->AddEntity();

    m_scene->GetEntityManager()->AddComponent(skybox_entity, TransformComponent {
        Transform(
            Vec3f::Zero(),
            Vec3f(1000.0f),
            Quaternion::Identity()
        )
    });

    m_scene->GetEntityManager()->AddComponent(skybox_entity, SkyComponent { });
    m_scene->GetEntityManager()->AddComponent(skybox_entity, MeshComponent { });
    m_scene->GetEntityManager()->AddComponent(skybox_entity, VisibilityStateComponent {
        VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE
    });
    m_scene->GetEntityManager()->AddComponent(skybox_entity, BoundingBoxComponent {
        BoundingBox(Vec3f(-1000.0f), Vec3f(1000.0f))
    });
}

void HyperionEditorImpl::SetFocusedNode(const NodeProxy &node)
{
    const NodeProxy previous_focused_node = m_focused_node;

    m_focused_node = node;

    // m_highlight_node.Remove();

    if (m_focused_node.IsValid()) {
        // @TODO watch for transform changes and update the highlight node

        // m_focused_node->AddChild(m_highlight_node);
        m_highlight_node->SetWorldScale(m_focused_node->GetWorldAABB().GetExtent() * 0.5f);
        m_highlight_node->SetWorldTranslation(m_focused_node->GetWorldTranslation());
    }

    OnFocusedNodeChanged(m_focused_node, previous_focused_node);
}

HYP_DISABLE_OPTIMIZATION;
void HyperionEditorImpl::Initialize()
{
    CreateHighlightNode();

    CreateMainPanel();
    CreateInitialState();
}
HYP_ENABLE_OPTIMIZATION;

void HyperionEditorImpl::UpdateEditorCamera(GameCounter::TickUnit delta)
{
    // temp
    /*if (m_focused_node.IsValid()) {


        auto debug_drawer_command_list = g_engine->GetDebugDrawer()->CreateCommandList();
        debug_drawer_command_list->Box(
            m_focused_node->GetWorldTranslation(),
            m_focused_node->GetWorldAABB().GetExtent(),
            Color(Vec4f(1.0f, 0.0f, 0.0f, 1.0f))
        );

        debug_drawer_command_list->Commit();
    }*/

    static constexpr float speed = 15.0f;

    if (!m_editor_camera_enabled) {
        return;
    }

    Vec3f translation = m_camera->GetTranslation();

    const Vec3f direction = m_camera->GetDirection();
    const Vec3f dir_cross_y = direction.Cross(m_camera->GetUpVector());

    if (m_input_manager->IsKeyDown(KeyCode::KEY_W)) {
        translation += direction * delta * speed;
    }
    if (m_input_manager->IsKeyDown(KeyCode::KEY_S)) {
        translation -= direction * delta * speed;
    }
    if (m_input_manager->IsKeyDown(KeyCode::KEY_A)) {
        translation -= dir_cross_y * delta * speed;
    }
    if (m_input_manager->IsKeyDown(KeyCode::KEY_D)) {
        translation += dir_cross_y * delta * speed;
    }

    m_camera->SetNextTranslation(translation);
}

#pragma endregion HyperionEditorImpl

#pragma region HyperionEditor

HyperionEditor::HyperionEditor()
    : Game(ManagedGameInfo { "GameName.dll", "TestGame1" })
{
}

HyperionEditor::~HyperionEditor()
{
}

void HyperionEditor::Init()
{
    Game::Init();

    const HypClass *bounding_box_class = GetClass(WeakName("BoundingBox"));
    AssertThrow(bounding_box_class->GetManagedClass() != nullptr);

    const HypClass *mesh_class = Mesh::GetClass();
    const HypMethod *test_method = mesh_class->GetMethod(NAME("TestMethod"));

    HYP_BREAKPOINT;

#if 0
    // const HypClass *cls = GetClass<Mesh>();
    // HYP_LOG(Editor, LogLevel::INFO, "my class: {}", cls->GetName());

    // Handle<Mesh> mesh = CreateObject<Mesh>();
    
    // if (HypProperty *property = cls->GetProperty("VertexAttributes")) {
    //     auto vertex_attributes_value = property->InvokeGetter(*mesh);
    //     HYP_LOG(Editor, LogLevel::INFO, "VertexAttributes: {}", vertex_attributes_value.ToString());

    //     decltype(auto) vertex_attributes_value1 = property->InvokeGetter<VertexAttributeSet>(*mesh);
    //     HYP_LOG(Editor, LogLevel::INFO, "VertexAttributes: {}", vertex_attributes_value.ToString());
    // }

    // HYP_LOG(Core, LogLevel::INFO, "cls properties: {}", cls->GetProperty("AABB")->name);

#if 1
    const HypClass *cls = GetClass<LightComponent>();
    HYP_LOG(Editor, LogLevel::INFO, "my class: {}", cls->GetName());

    LightComponent light_component;
    light_component.light = CreateObject<Light>(LightType::POINT, Vec3f { 0.0f, 1.0f, 0.0f }, Color{}, 1.0f, 100.0f);

    // for (HypProperty *property : cls->GetProperties()) {
    //     fbom::FBOMObject data_object;
    //     property->getter(light_component).ReadObject(data_object);
    //     HYP_LOG(Core, LogLevel::INFO, "Property: {}\t{}", property->name, data_object.ToString());
    // }

    if (HypProperty *property = cls->GetProperty("Light")) {
        // property->InvokeSetter(light_component, CreateObject<Light>(LightType::POINT, Vec3f { 0.0f, 1.0f, 0.0f }, Color{}, 1.0f, 100.0f));

        HYP_LOG(Editor, LogLevel::INFO, "LightComponent Light: {}", property->InvokeGetter(light_component).ToString());

        if (const HypClass *light_class = property->GetHypClass()) {
            AssertThrow(property->GetTypeID() == TypeID::ForType<Light>());
            HYP_LOG(Editor, LogLevel::INFO, "light_class: {}", light_class->GetName());
            HypProperty *light_radius_property = light_class->GetProperty("radius");
            AssertThrow(light_radius_property != nullptr);

            light_radius_property->InvokeSetter(property->InvokeGetter(light_component), 123.4f);
        }

        HYP_LOG(Editor, LogLevel::INFO, "LightComponent Light: {}", property->InvokeGetter<Handle<Light>>(light_component)->GetRadius());
    }
#endif

    // if (HypProperty *property = cls->GetProperty(NAME("VertexAttributes"))) {
    //     HYP_LOG(Core, LogLevel::INFO, "Mesh Vertex Attributes: {}", property->getter.Invoke(m).Get<VertexAttributeSet>().flag_mask);
    // }

    // if (HypProperty *property = cls->GetProperty(NAME("VertexAttributes"))) {
    //     HYP_LOG(Core, LogLevel::INFO, "Mesh Vertex Attributes: {}", property->getter.Invoke(m).Get<VertexAttributeSet>().flag_mask);
    // }

    HYP_BREAKPOINT;
#endif


    GetScene()->GetCamera()->SetCameraController(RC<CameraController>(new EditorCameraController()));

    GetScene()->GetEnvironment()->AddRenderComponent<UIRenderer>(NAME("EditorUIRenderer"), GetUIStage());
    
    Extent2D window_size;

    if (ApplicationWindow *current_window = GetAppContext()->GetMainWindow()) {
        window_size = current_window->GetDimensions();
    } else {
        window_size = Extent2D { 1280, 720 };
    }

    RC<ScreenCaptureRenderComponent> screen_capture_component = GetScene()->GetEnvironment()->AddRenderComponent<ScreenCaptureRenderComponent>(NAME("EditorSceneCapture"), window_size);

    m_impl = new HyperionEditorImpl(GetScene(), GetScene()->GetCamera(), m_input_manager.Get(), GetUIStage());
    m_impl->SetSceneTexture(screen_capture_component->GetTexture());
    m_impl->Initialize();


    // fbom::FBOMDeserializedObject obj;
    // fbom::FBOMReader reader({});
    // if (auto err = reader.LoadFromFile("Scene.hypscene", obj)) {
    //     HYP_FAIL("failed to load: %s", *err.message);
    // }

    // Handle<Scene> loaded_scene = obj.Get<Scene>();
    // m_scene->SetRoot(loaded_scene->GetRoot());

    // return;



    // add sun
    auto sun = CreateObject<Light>(DirectionalLight(
        Vec3f(-0.4f, 0.65f, 0.1f).Normalize(),
        Color(Vec4f(1.0f)),
        4.0f
    ));

    InitObject(sun);

    NodeProxy sun_node = m_scene->GetRoot()->AddChild();
    sun_node.SetName("Sun");

    auto sun_entity = m_scene->GetEntityManager()->AddEntity();
    sun_node.SetEntity(sun_entity);
    sun_node.SetWorldTranslation(Vec3f { -0.1f, 0.65f, 0.1f });

    m_scene->GetEntityManager()->AddComponent(sun_entity, LightComponent {
        sun
    });

    m_scene->GetEntityManager()->AddComponent(sun_entity, ShadowMapComponent {
        .mode       = ShadowMode::PCF,
        .radius     = 35.0f,
        .resolution = { 2048, 2048 }
    });

    // if (false) {
        

    //     Array<Handle<Light>> point_lights;

    //     point_lights.PushBack(CreateObject<Light>(PointLight(
    //         Vector3(-5.0f, 0.5f, 0.0f),
    //         Color(1.0f, 0.0f, 0.0f),
    //         1.0f,
    //         5.0f
    //     )));
    //     point_lights.PushBack(CreateObject<Light>(PointLight(
    //         Vector3(5.0f, 2.0f, 0.0f),
    //         Color(0.0f, 1.0f, 0.0f),
    //         1.0f,
    //         15.0f
    //     )));

    //     for (auto &light : point_lights) {
    //         auto point_light_entity = m_scene->GetEntityManager()->AddEntity();

    //         m_scene->GetEntityManager()->AddComponent(point_light_entity, ShadowMapComponent { });

    //         m_scene->GetEntityManager()->AddComponent(point_light_entity, TransformComponent {
    //             Transform(
    //                 light->GetPosition(),
    //                 Vec3f(1.0f),
    //                 Quaternion::Identity()
    //             )
    //         });

    //         m_scene->GetEntityManager()->AddComponent(point_light_entity, LightComponent {
    //             light
    //         });
    //     }
    // }


    // {
    //     Array<Handle<Light>> point_lights;

    //     point_lights.PushBack(CreateObject<Light>(PointLight(
    //        Vector3(0.0f, 1.5f, 2.0f),
    //        Color(0.0f, 1.0f, 0.0f),
    //        10.0f,
    //        15.0f
    //     )));

    //     for (auto &light : point_lights) {
    //         auto point_light_entity = m_scene->GetEntityManager()->AddEntity();

    //         m_scene->GetEntityManager()->AddComponent(point_light_entity, ShadowMapComponent { });

    //         m_scene->GetEntityManager()->AddComponent(point_light_entity, TransformComponent {
    //             Transform(
    //                 light->GetPosition(),
    //                 Vec3f(1.0f),
    //                 Quaternion::Identity()
    //             )
    //         });

    //         m_scene->GetEntityManager()->AddComponent(point_light_entity, LightComponent {
    //             light
    //         });
    //     }
    // }

    // { // add test area light
    //     auto light = CreateObject<Light>(RectangleLight(
    //         Vec3f(0.0f, 1.25f, 0.0f),
    //         Vec3f(0.0f, 0.0f, -1.0f).Normalize(),
    //         Vec2f(2.0f, 2.0f),
    //         Color(1.0f, 0.0f, 0.0f),
    //         1.0f
    //     ));

    //     light->SetMaterial(MaterialCache::GetInstance()->GetOrCreate(
    //         {
    //            .shader_definition = ShaderDefinition {
    //                 NAME("Forward"),
    //                 ShaderProperties(static_mesh_vertex_attributes)
    //             },
    //            .bucket = Bucket::BUCKET_OPAQUE
    //         },
    //         {
    //         }
    //     ));
    //     AssertThrow(light->GetMaterial().IsValid());

    //     InitObject(light);

    //     auto area_light_entity = m_scene->GetEntityManager()->AddEntity();

    //     m_scene->GetEntityManager()->AddComponent(area_light_entity, TransformComponent {
    //         Transform(
    //             light->GetPosition(),
    //             Vec3f(1.0f),
    //             Quaternion::Identity()
    //         )
    //     });

    //     m_scene->GetEntityManager()->AddComponent(area_light_entity, LightComponent {
    //         light
    //     });
    // }

    // if (false) { // test terrain
    //     auto terrain_node = m_scene->GetRoot()->AddChild();
    //     auto terrain_entity = m_scene->GetEntityManager()->AddEntity();

    //     // MeshComponent
    //     m_scene->GetEntityManager()->AddComponent(terrain_entity, MeshComponent {
    //         Handle<Mesh> { },
    //         MaterialCache::GetInstance()->GetOrCreate({
    //             .shader_definition = ShaderDefinition {
    //                 HYP_NAME(Terrain),
    //                 ShaderProperties(static_mesh_vertex_attributes)
    //             },
    //             .bucket = Bucket::BUCKET_OPAQUE
    //         })
    //     });

    //     // TerrainComponent
    //     m_scene->GetEntityManager()->AddComponent(terrain_entity, TerrainComponent {
    //     });

    //     terrain_node.SetEntity(terrain_entity);
    //     terrain_node.SetName("TerrainNode");
    // }

    // { // test terrain 2
    //     if (WorldGrid *world_grid = m_scene->GetWorldGrid()) {
    //         world_grid->AddPlugin(0, RC<TerrainWorldGridPlugin>(new TerrainWorldGridPlugin()));
    //     } else {
    //         HYP_FAIL("Failed to get world grid");
    //     }
    // }

    // temp
    RC<AssetBatch> batch = AssetManager::GetInstance()->CreateBatch();
    batch->Add("test_model", "models/pica_pica/pica_pica.obj");//sponza/sponza.obj");
    // batch->Add("zombie", "models/ogrexml/dragger_Body.mesh.xml");
    // batch->Add("house", "models/house.obj");

    HYP_LOG(Editor, LogLevel::DEBUG, "Loading assets, scene ID = {}", GetScene()->GetID().Value());

    ID<Entity> root_entity = GetScene()->GetEntityManager()->AddEntity();
    GetScene()->GetRoot()->SetEntity(root_entity);

    GetScene()->GetEntityManager()->AddComponent(root_entity, ScriptComponent {
        {
            .assembly_path  = "GameName.dll",
            .class_name     = "FizzBuzzTest"
        }
    });

    batch->OnComplete.Bind([this](AssetMap &results)
    {
#if 1
        NodeProxy node = results["test_model"].ExtractAs<Node>();

        // node.Scale(0.02f);
        node.SetName("test_model");
        node.LockTransform();

        if (true) {
            ID<Entity> env_grid_entity = m_scene->GetEntityManager()->AddEntity();

            m_scene->GetEntityManager()->AddComponent(env_grid_entity, TransformComponent {
                node.GetWorldTransform()
            });

            m_scene->GetEntityManager()->AddComponent(env_grid_entity, BoundingBoxComponent {
                node.GetLocalAABB() * 1.05f,
                node.GetWorldAABB() * 1.05f
            });

            // Add env grid component
            m_scene->GetEntityManager()->AddComponent(env_grid_entity, EnvGridComponent {
                EnvGridType::ENV_GRID_TYPE_SH
            });

            NodeProxy env_grid_node = m_scene->GetRoot()->AddChild();
            env_grid_node.SetEntity(env_grid_entity);
            env_grid_node.SetName("EnvGrid");
        }

        GetScene()->GetRoot()->AddChild(node);
        
        for (auto &node : node.GetChildren()) {
            if (auto child_entity = node.GetEntity()) {
                // Add BLASComponent

                m_scene->GetEntityManager()->AddComponent(child_entity, BLASComponent { });
            }
        }
#endif

        if (auto &zombie_asset = results["zombie"]; zombie_asset.IsOK()) {
            auto zombie = zombie_asset.ExtractAs<Node>();
            zombie.Scale(0.25f);
            zombie.Translate(Vec3f(0, 2.0f, -1.0f));
            auto zombie_entity = zombie[0].GetEntity();

            m_scene->GetRoot()->AddChild(zombie);

            if (zombie_entity.IsValid()) {
                if (auto *mesh_component = m_scene->GetEntityManager()->TryGetComponent<MeshComponent>(zombie_entity)) {
                    mesh_component->material = mesh_component->material->Clone();
                    mesh_component->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_ALBEDO, Vector4(1.0f, 0.0f, 0.0f, 1.0f));
                    mesh_component->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_ROUGHNESS, 0.05f);
                    mesh_component->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_METALNESS, 1.0f);
                    InitObject(mesh_component->material);
                }
            }

            zombie.SetName("zombie");
        }

        // FileByteWriter byte_writer("Scene.hypscene");
        // fbom::FBOMWriter writer;
        // writer.Append(*GetScene());
        // auto err = writer.Emit(&byte_writer);
        // byte_writer.Close();

        // if (err != fbom::FBOMResult::FBOM_OK) {
        //     HYP_FAIL("Failed to save scene");
        // }

        // fbom::FBOMDeserializedObject obj;
        // fbom::FBOMReader reader({});
        // if (auto err = reader.LoadFromFile("Scene.hypscene", obj)) {
        //     HYP_FAIL("failed to load: %s", *err.message);
        // }

        // Handle<Scene> loaded_scene = obj.Get<Scene>();
        
        // DebugLog(LogType::Debug, "Loaded scene root node : %s\n", *loaded_scene->GetRoot().GetName());

        // HYP_BREAKPOINT;
    }).Detach();

    batch->LoadAsync();
}

void HyperionEditor::Teardown()
{
    delete m_impl;
    m_impl = nullptr;
}

    
void HyperionEditor::Logic(GameCounter::TickUnit delta)
{
    m_impl->UpdateEditorCamera(delta);
}

void HyperionEditor::OnInputEvent(const SystemEvent &event)
{
    Game::OnInputEvent(event);

    if (event.GetType() == SystemEventType::EVENT_KEYDOWN && event.GetNormalizedKeyCode() == KeyCode::KEY_M) {
        NodeProxy test_model = m_scene->FindNodeByName("test_model");

        if (test_model) {
            test_model->UnlockTransform();
            test_model->Translate(Vec3f { 0.01f });
            test_model->LockTransform();
        }
    }
}

void HyperionEditor::OnFrameEnd(Frame *frame)
{
    Game::OnFrameEnd(frame);
}

#pragma endregion HyperionEditor

} // namespace editor
} // namespace hyperion