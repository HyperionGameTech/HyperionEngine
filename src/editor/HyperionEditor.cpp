#include <editor/HyperionEditor.hpp>
#include <editor/EditorCamera.hpp>

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

#include <ui/UIObject.hpp>
#include <ui/UIText.hpp>
#include <ui/UIButton.hpp>
#include <ui/UIPanel.hpp>
#include <ui/UITabView.hpp>
#include <ui/UIMenuBar.hpp>
#include <ui/UIGrid.hpp>
#include <ui/UIImage.hpp>
#include <ui/UIDockableContainer.hpp>

// temp
#include <util/Profile.hpp>
#include <core/system/SystemEvent.hpp>

namespace hyperion {
namespace editor {

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
    void CreateInitialState();

    Handle<Scene>       m_scene;
    Handle<Camera>      m_camera;
    InputManager        *m_input_manager;
    RC<UIStage>         m_ui_stage;
    Handle<Texture>     m_scene_texture;
    RC<UIObject>        m_main_panel;

    bool                m_editor_camera_enabled;
};

void HyperionEditorImpl::CreateFontAtlas()
{
    RC<FontFace> font_face = AssetManager::GetInstance()->Load<FontFace>("fonts/Roboto/Roboto-Regular.ttf");

    if (!font_face) {
        DebugLog(LogType::Error, "Failed to load font face!\n");

        return;
    }

    RC<FontAtlas> atlas(new FontAtlas(std::move(font_face)));
    atlas->Render();

    GetUIStage()->SetDefaultFontAtlas(std::move(atlas));
}

void HyperionEditorImpl::CreateMainPanel()
{
    // btn->SetPadding(Vec2i { 5, 5 });
    
    if (RC<UIObject> loaded_ui = AssetManager::GetInstance()->Load<UIObject>("ui/Editor.Main.ui.xml")) {
        if (loaded_ui.Is<UIStage>()) {
            loaded_ui.Cast<UIStage>()->SetOwnerThreadID(ThreadID::Current());
        }

        // auto main_menu = loaded_ui->FindChildUIObject(HYP_NAME(Main_MenuBar));

        // if (main_menu != nullptr) {
        //     DebugLog(LogType::Debug, "Main menu: %u\n", uint(main_menu->GetType()));
        // }

        // DebugLog(LogType::Debug, "Loaded position: %d, %d\n", loaded_ui->GetPosition().x, loaded_ui->GetPosition().y);
        // DebugLog(LogType::Debug, "Loaded UI: %s\n", *loaded_ui->GetName());

        GetUIStage()->AddChildUIObject(loaded_ui);
    }

#if 0
    m_main_panel = GetUIStage()->CreateUIObject<UIPanel>(HYP_NAME(Main_Panel), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }), true);

#if 0
    RC<UIMenuBar> menu_bar = GetUIStage()->CreateUIObject<UIMenuBar>(HYP_NAME(Sample_MenuBar), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 30, UIObjectSize::PIXEL }));
    menu_bar->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
    menu_bar->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);

    RC<UIMenuItem> file_menu_item = menu_bar->AddMenuItem(HYP_NAME(File_Menu_Item), "File");

    file_menu_item->AddDropDownMenuItem({
        HYP_NAME(New),
        "New",
        []()
        {
            DebugLog(LogType::Debug, "New clicked!\n");
        }
    });

    file_menu_item->AddDropDownMenuItem({
        HYP_NAME(Open),
        "Open"
    });

    file_menu_item->AddDropDownMenuItem({
        HYP_NAME(Save),
        "Save"
    });

    file_menu_item->AddDropDownMenuItem({
        HYP_NAME(Save_As),
        "Save As..."
    });

    file_menu_item->AddDropDownMenuItem({
        HYP_NAME(Exit),
        "Exit"
    });

    RC<UIMenuItem> edit_menu_item = menu_bar->AddMenuItem(HYP_NAME(Edit_Menu_Item), "Edit");
    edit_menu_item->AddDropDownMenuItem({
        HYP_NAME(Undo),
        "Undo"
    });

    edit_menu_item->AddDropDownMenuItem({
        HYP_NAME(Redo),
        "Redo"
    });

    edit_menu_item->AddDropDownMenuItem({
        HYP_NAME(Cut),
        "Cut"
    });

    edit_menu_item->AddDropDownMenuItem({
        HYP_NAME(Copy),
        "Copy"
    });

    edit_menu_item->AddDropDownMenuItem({
        HYP_NAME(Paste),
        "Paste"
    });

    RC<UIMenuItem> tools_menu_item = menu_bar->AddMenuItem(HYP_NAME(Tools_Menu_Item), "Tools");
    tools_menu_item->AddDropDownMenuItem({ HYP_NAME(Build_Lightmap), "Build Lightmaps" });

    RC<UIMenuItem> view_menu_item = menu_bar->AddMenuItem(HYP_NAME(View_Menu_Item), "View");
    view_menu_item->AddDropDownMenuItem({ HYP_NAME(Content_Browser), "Content Browser" });
    
    RC<UIMenuItem> window_menu_item = menu_bar->AddMenuItem(HYP_NAME(Window_Menu_Item), "Window");
    window_menu_item->AddDropDownMenuItem({ HYP_NAME(Reset_Layout), "Reset Layout" });

    m_main_panel->AddChildUIObject(menu_bar);
#endif

    RC<UIDockableContainer> dockable_container = GetUIStage()->CreateUIObject<UIDockableContainer>(HYP_NAME(Dockable_Container), Vec2i { 0, 30 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 768-30, UIObjectSize::PIXEL }));

    RC<UITabView> tab_view = GetUIStage()->CreateUIObject<UITabView>(HYP_NAME(Sample_TabView), Vec2i { 0, 30 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));
    tab_view->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
    tab_view->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);

    RC<UITab> scene_tab = tab_view->AddTab(HYP_NAME(Scene_Tab), "Scene");

    RC<UIImage> ui_image = GetUIStage()->CreateUIObject<UIImage>(HYP_NAME(Sample_Image), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));

    ui_image->OnMouseDrag.Bind([this, ui_image = ui_image.Get()](const UIMouseEventData &event)
    {
        CameraController *camera_controller = m_camera->GetCameraController();
        
        const Vec2f delta = (event.position - event.previous_position) * 150.0f;

        const Vec3f dir_cross_y = m_camera->GetDirection().Cross(m_camera->GetUpVector());

        if (event.mouse_buttons & MouseButtonState::LEFT) {
            const Vec2i delta_sign = Vec2i(MathUtil::Sign(delta));

            if (event.mouse_buttons & MouseButtonState::RIGHT) {
                m_camera->SetNextTranslation(m_camera->GetTranslation() + dir_cross_y * 0.1f * float(-delta_sign.y));
            } else {
                m_camera->Rotate(m_camera->GetUpVector(), MathUtil::DegToRad(delta.x));

                m_camera->SetNextTranslation(m_camera->GetTranslation() + m_camera->GetDirection() * 0.1f * float(-delta_sign.y));
            }
        } else if (event.mouse_buttons & MouseButtonState::RIGHT) {
            m_camera->Rotate(m_camera->GetUpVector(), MathUtil::DegToRad(delta.x));
            m_camera->Rotate(dir_cross_y, MathUtil::DegToRad(delta.y));

            if (m_camera->GetDirection().y > 0.98f || m_camera->GetDirection().y < -0.98f) {
                m_camera->Rotate(dir_cross_y, MathUtil::DegToRad(-delta.y));
            }
        }

        if (m_camera->GetCameraController()->IsMouseLocked()) {
            const Vec2f position = ui_image->GetAbsolutePosition();
            const Vec2i size = ui_image->GetActualSize();
            const Vec2i window_size = { m_camera->GetWidth(), m_camera->GetHeight() };

            // Set mouse position to previous position to keep it stationary while rotating
            event.input_manager->SetMousePosition(Vec2i(position + event.previous_position * Vec2f(size)));
        }

        return UIEventHandlerResult::OK;
    }).Detach();

    ui_image->OnMouseDown.Bind([this](const UIMouseEventData &event)
    {
        CameraController *camera_controller = m_camera->GetCameraController();

        if (EditorCameraController *editor_camera_controller = dynamic_cast<EditorCameraController *>(camera_controller)) {
            editor_camera_controller->SetMode(EditorCameraControllerMode::MOUSE_LOCKED);
        }

        return UIEventHandlerResult::OK;
    }).Detach();

    ui_image->OnMouseUp.Bind([this](const UIMouseEventData &event)
    {
        CameraController *camera_controller = m_camera->GetCameraController();

        if (EditorCameraController *editor_camera_controller = dynamic_cast<EditorCameraController *>(camera_controller)) {
            editor_camera_controller->SetMode(EditorCameraControllerMode::INACTIVE);
        }

        return UIEventHandlerResult::OK;
    }).Detach();

    /*ui_image->OnKeyDown.Bind([this](const UIKeyEventData &event)
    {
        DebugLog(LogType::Debug, "Got keydown event, keycode = %u\n", event.key_code);
        if (event.key_code == KeyCode::KEY_W || event.key_code == KeyCode::KEY_S || event.key_code == KeyCode::KEY_A || event.key_code == KeyCode::KEY_D) {
            CameraController *camera_controller = m_camera->GetCameraController();

            Vec3f translation = m_camera->GetTranslation();

            const Vec3f direction = m_camera->GetDirection();
            const Vec3f dir_cross_y = direction.Cross(m_camera->GetUpVector());

            if (event.key_code == KeyCode::KEY_W) {
                translation += direction * 0.01f;
            }
            if (event.key_code == KeyCode::KEY_S) {
                translation -= direction * 0.01f;
            }
            if (event.key_code == KeyCode::KEY_A) {
                translation += dir_cross_y * 0.01f;
            }
            if (event.key_code == KeyCode::KEY_D) {
                translation -= dir_cross_y * 0.01f;
            }

            camera_controller->SetNextTranslation(translation);

            return UIEventHandlerResult::STOP_BUBBLING;
        }

        return UIEventHandlerResult::OK;
    }).Detach();*/

    ui_image->OnGainFocus.Bind([this](const UIMouseEventData &event)
    {
        m_editor_camera_enabled = true;

        return UIEventHandlerResult::OK;
    });

    ui_image->OnLoseFocus.Bind([this](const UIMouseEventData &event)
    {
        m_editor_camera_enabled = false;

        return UIEventHandlerResult::OK;
    });

    ui_image->SetTexture(m_scene_texture);

    scene_tab->GetContents()->AddChildUIObject(ui_image);

    dockable_container->AddChildUIObject(tab_view, UIDockableItemPosition::CENTER);

    RC<UIPanel> scene_graph = GetUIStage()->CreateUIObject<UIPanel>(HYP_NAME(Scene_Graph_Panel), Vec2i { 0, 0 }, UIObjectSize({ 200, UIObjectSize::PIXEL }, { 100, UIObjectSize::PERCENT }));
    dockable_container->AddChildUIObject(scene_graph, UIDockableItemPosition::LEFT);

    RC<UIPanel> properties_panel = GetUIStage()->CreateUIObject<UIPanel>(HYP_NAME(Properties_panel), Vec2i { 0, 0 }, UIObjectSize({ 200, UIObjectSize::PIXEL }, { 100, UIObjectSize::PERCENT }));
    dockable_container->AddChildUIObject(properties_panel, UIDockableItemPosition::RIGHT);

    RC<UIPanel> bottom_panel = GetUIStage()->CreateUIObject<UIPanel>(HYP_NAME(Bottom_panel), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PIXEL }));
    dockable_container->AddChildUIObject(bottom_panel, UIDockableItemPosition::BOTTOM);

    // {
    //     auto btn = GetUIStage()->CreateUIObject<UIButton>(HYP_NAME(TestButton1), Vec2i { 0, 0 }, UIObjectSize({ 50, UIObjectSize::PIXEL }, { 25, UIObjectSize::PIXEL }));
    //     // game_tab_content_button->SetParentAlignment(UIObjectAlignment::CENTER);
    //     // game_tab_content_button->SetOriginAlignment(UIObjectAlignment::CENTER);
    //     btn->SetText("hello hello world");
    //     // btn->GetContents()->SetInnerSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    //     dockable_container->AddChildUIObject(bottom_panel, UIDockableItemPosition::BOTTOM);
    // }

    RC<UITab> game_tab = tab_view->AddTab(HYP_NAME(Game_Tab), "Game");

    for (int i = 0; i < 50; i++) {
        auto game_tab_content_button = GetUIStage()->CreateUIObject<UIButton>(CreateNameFromDynamicString(ANSIString("Hello_world_button_") + ANSIString::ToString(i)), Vec2i { 20, i * 25 }, UIObjectSize({ 50, UIObjectSize::PIXEL }, { 25, UIObjectSize::PIXEL }));
        // game_tab_content_button->SetParentAlignment(UIObjectAlignment::CENTER);
        // game_tab_content_button->SetOriginAlignment(UIObjectAlignment::CENTER);
        game_tab_content_button->SetText(String("Hello ") + String::ToString(i));
        game_tab->GetContents()->SetInnerSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
        game_tab->GetContents()->AddChildUIObject(game_tab_content_button);
    }

    m_main_panel->AddChildUIObject(dockable_container);

#endif

    // AssertThrow(game_tab_content_button->GetScene() != nullptr);

    // game_tab_content_button->GetScene()->GetEntityManager()->AddComponent(game_tab_content_button->GetEntity(), ScriptComponent {
    //     {
    //         .assembly_name  = "csharp.dll",
    //         .class_name     = "TestUIScript"
    //     }
    // });

    // AssertThrow(game_tab_content_button->GetScene()->GetEntityManager()->HasEntity(game_tab_content_button->GetEntity()));

    // ui_image->SetTexture(AssetManager::GetInstance()->Load<Texture>("textures/dummy.jpg"));
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

void HyperionEditorImpl::Initialize()
{
    CreateFontAtlas();
    CreateMainPanel();
    CreateInitialState();

    // auto ui_text = GetUIStage()->CreateUIObject<UIText>(HYP_NAME(Sample_Text), Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::AUTO }, { 18, UIObjectSize::PIXEL }));
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

    // auto new_btn = GetUIStage()->CreateUIObject<UIButton>(HYP_NAME(Nested_Button), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT | UIObjectSize::RELATIVE }, { 100, UIObjectSize::PERCENT | UIObjectSize::RELATIVE }));
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

    GetScene()->GetCamera()->SetCameraController(RC<CameraController>(new EditorCameraController()));

    GetScene()->GetEnvironment()->AddRenderComponent<UIRenderer>(HYP_NAME(EditorUIRenderer), GetUIStage());
    
    Extent2D window_size;

    if (ApplicationWindow *current_window = GetAppContext()->GetCurrentWindow()) {
        window_size = current_window->GetDimensions();
    } else {
        window_size = Extent2D { 1280, 720 };
    }

    auto screen_capture_component = GetScene()->GetEnvironment()->AddRenderComponent<ScreenCaptureRenderComponent>(HYP_NAME(EditorSceneCapture), window_size);

    m_impl = new HyperionEditorImpl(GetScene(), GetScene()->GetCamera(), m_input_manager.Get(), GetUIStage());
    m_impl->SetSceneTexture(screen_capture_component->GetTexture());
    m_impl->Initialize();

    // add sun
    auto sun = CreateObject<Light>(DirectionalLight(
        Vec3f(-0.1f, 0.65f, 0.1f).Normalize(),
        Color(1.0f),
        4.0f
    ));

    InitObject(sun);

    auto sun_node = m_scene->GetRoot().AddChild();
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
    //                 HYP_NAME(Forward),
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

    // temp
    auto batch = AssetManager::GetInstance()->CreateBatch();
    batch->Add("test_model", "models/sponza/sponza.obj");
    batch->Add("zombie", "models/ogrexml/dragger_Body.mesh.xml");
    batch->Add("house", "models/house.obj");
    batch->LoadAsync();
    auto results = batch->AwaitResults();

    auto node = results["test_model"].ExtractAs<Node>();
    node.Scale(0.0125f);
    node.SetName("test_model");
    node.LockTransform();

    // if (auto house = results["house"].Get<Node>()) {
    //     house.Scale(0.25f);
    //     m_scene->GetRoot().AddChild(house);
    // }

    if (auto zombie = results["zombie"].Get<Node>()) {
        zombie.Scale(0.25f);
        zombie.Translate(Vec3f(0, 2.0f, -1.0f));
        auto zombie_entity = zombie[0].GetEntity();

        m_scene->GetRoot().AddChild(zombie);

        if (zombie_entity.IsValid()) {
            if (auto *mesh_component = m_scene->GetEntityManager()->TryGetComponent<MeshComponent>(zombie_entity)) {
                mesh_component->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_ALBEDO, Vector4(1.0f, 0.0f, 0.0f, 1.0f));
                mesh_component->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_ROUGHNESS, 0.05f);
                mesh_component->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_METALNESS, 1.0f);
            }
        }

        zombie.SetName("zombie");
    }

    GetScene()->GetRoot().AddChild(node);
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
}

#pragma endregion HyperionEditor

} // namespace editor
} // namespace hyperion