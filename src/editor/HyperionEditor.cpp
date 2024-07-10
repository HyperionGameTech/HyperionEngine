#include <editor/HyperionEditor.hpp>
#include <editor/EditorCamera.hpp>
#include <editor/EditorObjectProperties.hpp>
#include <editor/EditorDelegates.hpp>

#include <rendering/Texture.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/UIRenderer.hpp>
#include <rendering/render_components/ScreenCapture.hpp>

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

#include <scripting/Script.hpp>
#include <scripting/ScriptingService.hpp>

// temp
#include <util/profiling/Profile.hpp>
#include <rendering/lightmapper/LightmapRenderer.hpp>
#include <rendering/lightmapper/LightmapUVBuilder.hpp>

#include <core/system/SystemEvent.hpp>

// temp
#include <core/HypClass.hpp>
#include <core/HypClassProperty.hpp>

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

        // for (const HypClassProperty *property : hyp_class->GetProperties()) {
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

        RC<UIText> text = stage->CreateUIObject<UIText>(NAME("NodeNameText"), Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::AUTO }, { 14, UIObjectSize::PIXEL }));
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
    const HypClassProperty  *property = nullptr;
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

        fbom::FBOMData property_value = value.property->InvokeGetter(*node_rc);

        if (Optional<fbom::FBOMDeserializedObject &> deserialized_object = property_value.GetDeserializedObject()) {
            const TypeID property_value_type_id = deserialized_object->any_value.GetTypeID();

            IUIDataSourceElementFactory *element_factory = UIDataSourceElementFactoryRegistry::GetInstance().GetFactory(property_value_type_id);

            if (element_factory) {
                // RC<UIPanel> sub_panel = stage->CreateUIObject<UIPanel>(NAME("PropertySubPanel"), Vec2i { 0, 25 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

                RC<UIObject> element = element_factory->CreateUIObject(stage, deserialized_object->any_value.ToRef());
                panel->AddChildUIObject(element);

                // panel->AddChildUIObject(sub_panel);
            } else {
                HYP_LOG(Editor, LogLevel::ERR, "No UI element factory found for type ID: {}; cannot render element", property_value_type_id.Value());
            }
         } else {
            HYP_LOG(Editor, LogLevel::ERR, "Property value is not a deserialized object; cannot render element");
        }

        return panel;
    }

    virtual void UpdateUIObject_Internal(UIObject *ui_object, const EditorNodePropertyRef &value) const override
    {
        RC<Node> node_rc = value.node.Lock();
        if (!node_rc) {
            return;
        }

        fbom::FBOMData property_value = value.property->InvokeGetter(*node_rc);

        if (Optional<fbom::FBOMDeserializedObject &> deserialized_object = property_value.GetDeserializedObject()) {
            const TypeID property_value_type_id = deserialized_object->any_value.GetTypeID();

            IUIDataSourceElementFactory *element_factory = UIDataSourceElementFactoryRegistry::GetInstance().GetFactory(property_value_type_id);

            if (element_factory) {
                element_factory->UpdateUIObject(ui_object, deserialized_object->any_value.ToRef());
            } else {
                HYP_LOG(Editor, LogLevel::ERR, "No UI element factory found for type ID: {}; cannot render element", property_value_type_id.Value());
            }
        } else {
            HYP_LOG(Editor, LogLevel::ERR, "Property value is not a deserialized object; cannot render element");
        }
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
          m_editor_camera_enabled(false)
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
    void CreateFontAtlas();
    void CreateMainPanel();
    RC<UIObject> CreateSceneOutline();
    RC<UIObject> CreateDetailView();
    void CreateInitialState();

    void SetFocusedNode(const NodeProxy &node);

    Handle<Scene>                                           m_scene;
    Handle<Camera>                                          m_camera;
    InputManager                                            *m_input_manager;
    RC<UIStage>                                             m_ui_stage;
    Handle<Texture>                                         m_scene_texture;
    RC<UIObject>                                            m_main_panel;

    NodeProxy                                               m_focused_node;

    bool                                                    m_editor_camera_enabled;

    Delegate<void, const NodeProxy &, const NodeProxy &>    OnFocusedNodeChanged;
};

void HyperionEditorImpl::CreateFontAtlas()
{
    auto font_face_asset = AssetManager::GetInstance()->Load<RC<FontFace>>("fonts/Roboto/Roboto-Regular.ttf");

    if (!font_face_asset.IsOK()) {
        HYP_LOG(Editor, LogLevel::ERR, "Failed to load font face!");

        return;
    }

    RC<FontAtlas> atlas(new FontAtlas(std::move(font_face_asset.Result())));
    atlas->Render();

    GetUIStage()->SetDefaultFontAtlas(std::move(atlas));
}

void HyperionEditorImpl::CreateMainPanel()
{
#if 0
    if (RC<UIObject> loaded_ui = AssetManager::GetInstance()->Load<RC<UIObject>>("ui/Editor.Main.ui.xml")) {
        if (loaded_ui.Is<UIStage>()) {
            loaded_ui.Cast<UIStage>()->SetOwnerThreadID(ThreadID::Current());
        }

        // auto main_menu = loaded_ui->FindChildUIObject(NAME("Main_MenuBar"));

        // if (main_menu != nullptr) {
        //     DebugLog(LogType::Debug, "Main menu: %u\n", uint(main_menu->GetType()));
        // }

        // DebugLog(LogType::Debug, "Loaded position: %d, %d\n", loaded_ui->GetPosition().x, loaded_ui->GetPosition().y);
        // DebugLog(LogType::Debug, "Loaded UI: %s\n", *loaded_ui->GetName());

        GetUIStage()->AddChildUIObject(loaded_ui);

        loaded_ui.Cast<UIStage>()->SetDefaultFontAtlas(GetUIStage()->GetDefaultFontAtlas());
    }
#else

    m_main_panel = GetUIStage()->CreateUIObject<UIPanel>(NAME("Main_Panel"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }), true);

    RC<UIMenuBar> menu_bar = GetUIStage()->CreateUIObject<UIMenuBar>(NAME("Sample_MenuBar"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 30, UIObjectSize::PIXEL }));
    menu_bar->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
    menu_bar->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);

    RC<UIMenuItem> file_menu_item = menu_bar->AddMenuItem(NAME("File_Menu_Item"), "File");

    file_menu_item->AddDropDownMenuItem({
        NAME("New"),
        "New",
        []()
        {
            DebugLog(LogType::Debug, "New clicked!\n");
        }
    });

    file_menu_item->AddDropDownMenuItem({
        NAME("Open"),
        "Open"
    });

    file_menu_item->AddDropDownMenuItem({
        NAME("Save"),
        "Save"
    });

    file_menu_item->AddDropDownMenuItem({
        NAME("Save_As"),
        "Save As..."
    });

    file_menu_item->AddDropDownMenuItem({
        NAME("Exit"),
        "Exit"
    });

    RC<UIMenuItem> edit_menu_item = menu_bar->AddMenuItem(NAME("Edit_Menu_Item"), "Edit");
    edit_menu_item->AddDropDownMenuItem({
        NAME("Undo"),
        "Undo"
    });

    edit_menu_item->AddDropDownMenuItem({
        NAME("Redo"),
        "Redo"
    });

    edit_menu_item->AddDropDownMenuItem({
        NAME("Cut"),
        "Cut"
    });

    edit_menu_item->AddDropDownMenuItem({
        NAME("Copy"),
        "Copy"
    });

    edit_menu_item->AddDropDownMenuItem({
        NAME("Paste"),
        "Paste"
    });

    RC<UIMenuItem> tools_menu_item = menu_bar->AddMenuItem(NAME("Tools_Menu_Item"), "Tools");
    tools_menu_item->AddDropDownMenuItem({ NAME("Build_Lightmap"), "Build Lightmaps" });

    RC<UIMenuItem> view_menu_item = menu_bar->AddMenuItem(NAME("View_Menu_Item"), "View");
    view_menu_item->AddDropDownMenuItem({ NAME("Content_Browser"), "Content Browser" });
    
    RC<UIMenuItem> window_menu_item = menu_bar->AddMenuItem(NAME("Window_Menu_Item"), "Window");
    window_menu_item->AddDropDownMenuItem({ NAME("Reset_Layout"), "Reset Layout" });

    m_main_panel->AddChildUIObject(menu_bar);
#endif

#if 1
    RC<UIDockableContainer> dockable_container = GetUIStage()->CreateUIObject<UIDockableContainer>(NAME("Dockable_Container"), Vec2i { 0, 30 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 768-30, UIObjectSize::PIXEL }));

    RC<UITabView> tab_view = GetUIStage()->CreateUIObject<UITabView>(NAME("Sample_TabView"), Vec2i { 0, 30 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));
    tab_view->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
    tab_view->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);

    RC<UITab> scene_tab = tab_view->AddTab(NAME("Scene_Tab"), "Scene");

    RC<UIImage> ui_image = GetUIStage()->CreateUIObject<UIImage>(NAME("Sample_Image"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));

    ui_image->OnMouseDrag.Bind([this, ui_image = ui_image.Get()](const MouseEvent &event)
    {
        m_camera->GetCameraController()->GetInputHandler()->OnMouseDrag(event);

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

        return UIEventHandlerResult::OK;
    }).Detach();

    ui_image->OnMouseUp.Bind([this](const MouseEvent &event)
    {
        m_camera->GetCameraController()->GetInputHandler()->OnMouseUp(event);

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
    });

    ui_image->OnLoseFocus.Bind([this](const MouseEvent &event)
    {
        m_editor_camera_enabled = false;

        return UIEventHandlerResult::OK;
    });

    ui_image->SetTexture(m_scene_texture);

    scene_tab->GetContents()->AddChildUIObject(ui_image);

    dockable_container->AddChildUIObject(tab_view, UIDockableItemPosition::CENTER);

    dockable_container->AddChildUIObject(CreateSceneOutline(), UIDockableItemPosition::LEFT);
    dockable_container->AddChildUIObject(CreateDetailView(), UIDockableItemPosition::RIGHT);

    RC<UIPanel> bottom_panel = GetUIStage()->CreateUIObject<UIPanel>(NAME("Bottom_panel"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PIXEL }));
    dockable_container->AddChildUIObject(bottom_panel, UIDockableItemPosition::BOTTOM);

    // {
    //     auto btn = GetUIStage()->CreateUIObject<UIButton>(NAME("TestButton1"), Vec2i { 0, 0 }, UIObjectSize({ 50, UIObjectSize::PIXEL }, { 25, UIObjectSize::PIXEL }));
    //     // game_tab_content_button->SetParentAlignment(UIObjectAlignment::CENTER);
    //     // game_tab_content_button->SetOriginAlignment(UIObjectAlignment::CENTER);
    //     btn->SetText("hello hello world");
    //     // btn->GetContents()->SetInnerSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    //     dockable_container->AddChildUIObject(bottom_panel, UIDockableItemPosition::BOTTOM);
    // }

    RC<UITab> game_tab = tab_view->AddTab(NAME("Game_Tab"), "Game");
    game_tab->GetContents()->SetInnerSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    
    auto generate_lightmaps_button = GetUIStage()->CreateUIObject<UIButton>(HYP_NAME(Generate_Lightmaps), Vec2i{60, 50}, UIObjectSize({50, UIObjectSize::PIXEL}, {25, UIObjectSize::PIXEL}));
    generate_lightmaps_button->SetText("GenerateLightmap");

    generate_lightmaps_button->OnClick.Bind([this](...)
    {
        struct RENDER_COMMAND(SubmitLightmapJob) : renderer::RenderCommand
        {
            Handle<Scene> scene;

            RENDER_COMMAND(SubmitLightmapJob)(Handle<Scene> scene)
                : scene(std::move(scene))
            {
            }

            virtual ~RENDER_COMMAND(SubmitLightmapJob)() override = default;

            virtual Result operator()() override
            {
                if (scene->GetEnvironment()->HasRenderComponent<LightmapRenderer>()) {
                    scene->GetEnvironment()->RemoveRenderComponent<LightmapRenderer>();
                } else {
                    scene->GetEnvironment()->AddRenderComponent<LightmapRenderer>(Name::Unique("LightmapRenderer"));
                }

                HYPERION_RETURN_OK;
            }
        };
        
        PUSH_RENDER_COMMAND(SubmitLightmapJob, m_scene);

        return UIEventHandlerResult::OK;
    }).Detach();

    game_tab->GetContents()->AddChildUIObject(generate_lightmaps_button);

    auto game_tab_content_button = GetUIStage()->CreateUIObject<UIButton>(CreateNameFromDynamicString("Hello_world_button"), Vec2i { 20, 0 }, UIObjectSize({ 50, UIObjectSize::PIXEL }, { 25, UIObjectSize::PIXEL }));
    // game_tab_content_button->SetParentAlignment(UIObjectAlignment::CENTER);
    // game_tab_content_button->SetOriginAlignment(UIObjectAlignment::CENTER);
    game_tab_content_button->SetText("Hello");

    game_tab_content_button->GetScene()->GetEntityManager()->AddComponent(game_tab_content_button->GetEntity(), ScriptComponent {
        {
            .assembly_path  = "GameName.dll",
            .class_name     = "FizzBuzzTest"
        }
    });

    AssertThrow(game_tab_content_button->GetScene()->GetEntityManager()->HasEntity(game_tab_content_button->GetEntity()));

    game_tab->GetContents()->AddChildUIObject(game_tab_content_button);

    m_main_panel->AddChildUIObject(dockable_container);

#endif

    // AssertThrow(game_tab_content_button->GetScene() != nullptr);

    // ui_image->SetTexture(AssetManager::GetInstance()->Load<Texture>("textures/dummy.jpg"));

    g_engine->GetScriptingService()->OnScriptStateChanged.Bind([](const ManagedScript &script)
    {
        DebugLog(LogType::Debug, "Script state changed: now is %u\n", script.state);
    }).Detach();
}

RC<UIObject> HyperionEditorImpl::CreateSceneOutline()
{
    RC<UIPanel> scene_outline = GetUIStage()->CreateUIObject<UIPanel>(NAME("Scene_Outline"), Vec2i { 0, 0 }, UIObjectSize({ 200, UIObjectSize::PIXEL }, { 100, UIObjectSize::PERCENT }));

    // RC<UIPanel> scene_outline_header = GetUIStage()->CreateUIObject<UIPanel>(NAME("Scene_Outline_Header"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 25, UIObjectSize::PIXEL }));
    // RC<UIText> scene_outline_header_text = GetUIStage()->CreateUIObject<UIText>(NAME("Scene_Outline_Header_Text"), Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::AUTO }, { 10, UIObjectSize::PIXEL }));
    // scene_outline_header_text->SetOriginAlignment(UIObjectAlignment::CENTER);
    // scene_outline_header_text->SetParentAlignment(UIObjectAlignment::CENTER);
    // scene_outline_header_text->SetText("SCENE");
    // scene_outline_header_text->SetTextColor(Vec4f::One());
    // scene_outline_header->AddChildUIObject(scene_outline_header_text);
    // scene_outline->AddChildUIObject(scene_outline_header);


    UniquePtr<UIDataSource<Weak<Node>>> temp_data_source(new UIDataSource<Weak<Node>>());
    RC<UIListView> list_view = GetUIStage()->CreateUIObject<UIListView>(NAME("Scene_Outline_ListView"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));
    list_view->SetInnerSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

    // for (uint32 i = 0; i < 100; i++) {
    //     RC<UIText> text = GetUIStage()->CreateUIObject<UIText>(NAME("Text"), Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::AUTO }, { 14, UIObjectSize::PIXEL }));
    //     text->SetText(HYP_FORMAT("Node {}", i));
    //     list_view->AddChildUIObject(text);
    // }
    
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

    RC<UIListView> list_view = GetUIStage()->CreateUIObject<UIListView>(NAME("Detail_View_ListView"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));
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

        for (const HypClassProperty *property : hyp_class->GetProperties()) {
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

    OnFocusedNodeChanged.Broadcast(m_focused_node, previous_focused_node);
}

void HyperionEditorImpl::Initialize()
{
    CreateFontAtlas();
    CreateMainPanel();
    CreateInitialState();

    // auto ui_text = GetUIStage()->CreateUIObject<UIText>(NAME("Sample_Text"), Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::AUTO }, { 18, UIObjectSize::PIXEL }));
    // ui_text->SetText("Hi hello");
    // ui_text->SetParentAlignment(UIObjectAlignment::CENTER);
    // ui_text->SetOriginAlignment(UIObjectAlignment::CENTER);
    // ui_text->OnClick.Bind([ui_text](...) -> bool
    // {
    //     ui_text->SetText("Hi hello world\nMultiline test");

    //     return false;
    // }).Detach();

    // btn->AddChildUIObject(ui_text);
    
    // ui_text->SetTextColor(Vec4f { 1.0f, 1.0f, 1.0f, 1.0f });

    // auto new_btn = GetUIStage()->CreateUIObject<UIButton>(NAME("Nested_Button"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT | UIObjectSize::RELATIVE }, { 100, UIObjectSize::PERCENT | UIObjectSize::RELATIVE }));
    // new_btn->SetOriginAlignment(UIObjectAlignment::CENTER);
    // new_btn->SetParentAlignment(UIObjectAlignment::CENTER);
    // btn->AddChildUIObject(new_btn);
    // new_btn->UpdatePosition();
    // new_btn->UpdateSize();

    // DebugLog(LogType::Debug, "ui_text aabb [%f, %f, %f, %f]\n", ui_text->GetLocalAABB().min.x, ui_text->GetLocalAABB().min.y, ui_text->GetLocalAABB().max.x, ui_text->GetLocalAABB().max.y);
    // DebugLog(LogType::Debug, "new_btn aabb [%f, %f, %f, %f]\n", new_btn->GetLocalAABB().min.x, new_btn->GetLocalAABB().min.y, new_btn->GetLocalAABB().max.x, new_btn->GetLocalAABB().max.y);
}

void HyperionEditorImpl::UpdateEditorCamera(GameCounter::TickUnit delta)
{
    if (!m_editor_camera_enabled) {
        return;
    }

    CameraController *camera_controller = m_camera->GetCameraController();

    Vec3f translation = m_camera->GetTranslation();

    const Vec3f direction = m_camera->GetDirection();
    const Vec3f dir_cross_y = direction.Cross(m_camera->GetUpVector());

    if (m_input_manager->IsKeyDown(KeyCode::KEY_W)) {
        translation += direction * 0.01f;
    }
    if (m_input_manager->IsKeyDown(KeyCode::KEY_S)) {
        translation -= direction * 0.01f;
    }
    if (m_input_manager->IsKeyDown(KeyCode::KEY_A)) {
        translation += dir_cross_y * 0.01f;
    }
    if (m_input_manager->IsKeyDown(KeyCode::KEY_D)) {
        translation -= dir_cross_y * 0.01f;
    }

    camera_controller->SetNextTranslation(translation);
}

#pragma endregion HyperionEditorImpl

#pragma region HyperionEditor

HyperionEditor::HyperionEditor()
    : Game()
{
}

HyperionEditor::~HyperionEditor()
{
}

void HyperionEditor::Init()
{
    Game::Init();

#if 0
    // const HypClass *cls = GetClass<Mesh>();
    // HYP_LOG(Editor, LogLevel::INFO, "my class: {}", cls->GetName());

    // Handle<Mesh> mesh = CreateObject<Mesh>();
    
    // if (HypClassProperty *property = cls->GetProperty("VertexAttributes")) {
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

    // for (HypClassProperty *property : cls->GetProperties()) {
    //     fbom::FBOMObject data_object;
    //     property->getter(light_component).ReadObject(data_object);
    //     HYP_LOG(Core, LogLevel::INFO, "Property: {}\t{}", property->name, data_object.ToString());
    // }

    if (HypClassProperty *property = cls->GetProperty("Light")) {
        // property->InvokeSetter(light_component, CreateObject<Light>(LightType::POINT, Vec3f { 0.0f, 1.0f, 0.0f }, Color{}, 1.0f, 100.0f));

        HYP_LOG(Editor, LogLevel::INFO, "LightComponent Light: {}", property->InvokeGetter(light_component).ToString());

        if (const HypClass *light_class = property->GetHypClass()) {
            AssertThrow(property->GetTypeID() == TypeID::ForType<Light>());
            HYP_LOG(Editor, LogLevel::INFO, "light_class: {}", light_class->GetName());
            HypClassProperty *light_radius_property = light_class->GetProperty("radius");
            AssertThrow(light_radius_property != nullptr);

            light_radius_property->InvokeSetter(property->InvokeGetter(light_component), 123.4f);
        }

        HYP_LOG(Editor, LogLevel::INFO, "LightComponent Light: {}", property->InvokeGetter<Handle<Light>>(light_component)->GetRadius());
    }
#endif

    // if (HypClassProperty *property = cls->GetProperty(NAME("VertexAttributes"))) {
    //     HYP_LOG(Core, LogLevel::INFO, "Mesh Vertex Attributes: {}", property->getter.Invoke(m).Get<VertexAttributeSet>().flag_mask);
    // }

    // if (HypClassProperty *property = cls->GetProperty(NAME("VertexAttributes"))) {
    //     HYP_LOG(Core, LogLevel::INFO, "Mesh Vertex Attributes: {}", property->getter.Invoke(m).Get<VertexAttributeSet>().flag_mask);
    // }

    HYP_BREAKPOINT;
#endif


    GetScene()->GetCamera()->SetCameraController(RC<CameraController>(new EditorCameraController()));

    GetScene()->GetEnvironment()->AddRenderComponent<UIRenderer>(NAME("EditorUIRenderer"), GetUIStage());
    
    Extent2D window_size;

    if (ApplicationWindow *current_window = GetAppContext()->GetCurrentWindow()) {
        window_size = current_window->GetDimensions();
    } else {
        window_size = Extent2D { 1280, 720 };
    }

    auto screen_capture_component = GetScene()->GetEnvironment()->AddRenderComponent<ScreenCaptureRenderComponent>(NAME("EditorSceneCapture"), window_size);

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
        Vec3f(-0.1f, 0.65f, 0.1f).Normalize(),
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

    { // test terrain 2
        if (WorldGrid *world_grid = m_scene->GetWorldGrid()) {
            world_grid->AddPlugin(0, RC<TerrainWorldGridPlugin>(new TerrainWorldGridPlugin()));
        } else {
            HYP_FAIL("Failed to get world grid");
        }
    }

    // temp
    RC<AssetBatch> batch = AssetManager::GetInstance()->CreateBatch();
    batch->Add("test_model", "models/sponza/sponza.obj");
    batch->Add("zombie", "models/ogrexml/dragger_Body.mesh.xml");
    batch->Add("house", "models/house.obj");

    batch->OnComplete.Bind([this](AssetMap &results)
    {
#if 1
        NodeProxy node = results["test_model"].ExtractAs<Node>();
        GetScene()->GetRoot()->AddChild(node);

        node.Scale(0.0125f);
        node.SetName("test_model");
        node.LockTransform();

        if (true) {
            ID<Entity> env_grid_entity = m_scene->GetEntityManager()->AddEntity();

            m_scene->GetEntityManager()->AddComponent(env_grid_entity, TransformComponent {
                node.GetWorldTransform()
            });

            m_scene->GetEntityManager()->AddComponent(env_grid_entity, BoundingBoxComponent {
                node.GetLocalAABB() * 1.0f,
                node.GetWorldAABB() * 1.0f
            });

            // Add env grid component
            m_scene->GetEntityManager()->AddComponent(env_grid_entity, EnvGridComponent {
                EnvGridType::ENV_GRID_TYPE_SH
            });

            NodeProxy env_grid_node = m_scene->GetRoot()->AddChild();
            env_grid_node.SetEntity(env_grid_entity);
            env_grid_node.SetName("EnvGrid");
        }

        GetScene()->GetRoot().AddChild(node);
        
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
                    mesh_component->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_ALBEDO, Vector4(1.0f, 0.0f, 0.0f, 1.0f));
                    mesh_component->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_ROUGHNESS, 0.05f);
                    mesh_component->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_METALNESS, 1.0f);
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

    // testing
    if (auto zombie = GetScene()->GetRoot()->Select("zombie"); zombie.IsValid()) {
        static double timer = 0.0;
        timer += delta;

        zombie->Rotate(Quaternion::AxisAngles(Vec3f::UnitY(), 2.5f * delta));

        // testing remove
        if (timer > 15.0) {
            // HYP_LOG(Editor, LogLevel::DEBUG, "Removing zombie");
            // zombie->Remove();
            zombie->SetName("Blah blah");
        }
    }
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