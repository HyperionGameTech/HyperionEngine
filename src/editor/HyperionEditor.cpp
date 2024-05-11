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

// temp
#include <util/Profile.hpp>

namespace hyperion {
namespace editor {

#pragma region HyperionEditorImpl

class HyperionEditorImpl
{
public:
    HyperionEditorImpl(const Handle<Scene> &scene, const Handle<Camera> &camera, const RC<UIStage> &ui_stage)
        : m_scene(scene),
          m_camera(camera),
          m_ui_stage(ui_stage)
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

private:
    void CreateFontAtlas();
    void CreateMainPanel();
    void CreateInitialState();

    Handle<Scene>   m_scene;
    Handle<Camera>  m_camera;
    RC<UIStage>     m_ui_stage;
    Handle<Texture> m_scene_texture;
    RC<UIObject>    m_main_panel;
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
    m_main_panel = GetUIStage()->CreateUIObject<UIPanel>(HYP_NAME(Main_Panel), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }), true);
    // btn->SetPadding(Vec2i { 5, 5 });
    
    auto menu_bar = GetUIStage()->CreateUIObject<UIMenuBar>(HYP_NAME(Sample_MenuBar), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 30, UIObjectSize::PIXEL }));
    menu_bar->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
    menu_bar->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);

    auto file_menu_item = menu_bar->AddMenuItem(HYP_NAME(File_Menu_Item), "File");

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

    auto edit_menu_item = menu_bar->AddMenuItem(HYP_NAME(Edit_Menu_Item), "Edit");
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

    auto tools_menu_item = menu_bar->AddMenuItem(HYP_NAME(Tools_Menu_Item), "Tools");
    tools_menu_item->AddDropDownMenuItem({ HYP_NAME(Build_Lightmap), "Build Lightmaps" });

    auto view_menu_item = menu_bar->AddMenuItem(HYP_NAME(View_Menu_Item), "Window");
    view_menu_item->AddDropDownMenuItem({ HYP_NAME(Reset_Layout), "Reset Layout" });
    
    m_main_panel->AddChildUIObject(menu_bar);

    auto tab_view = GetUIStage()->CreateUIObject<UITabView>(HYP_NAME(Sample_TabView), Vec2i { 60, 80 }, Vec2i { 1150, 650 });
    tab_view->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
    tab_view->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);
    m_main_panel->AddChildUIObject(tab_view);

    auto scene_tab_content_grid = GetUIStage()->CreateUIObject<UIGrid>(HYP_NAME(Scene_Tab_Grid), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));
    scene_tab_content_grid->SetNumColumns(3);
    scene_tab_content_grid->SetNumRows(5);

    // auto scene_tab_content_text = GetUIStage()->CreateUIObject<UIText>(HYP_NAME(Scene_Tab_Text), Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::GROW }, { 30, UIObjectSize::PIXEL }));
    // scene_tab_content_text->SetText("grid test 1234567");
    // scene_tab_content_text->SetParentAlignment(UIObjectAlignment::CENTER);
    // scene_tab_content_text->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);
    // scene_tab_content_text->SetTextColor(Vec4f { 1.0f, 1.0f, 1.0f, 1.0f });


    RC<UITab> scene_tab = tab_view->AddTab(HYP_NAME(Scene_Tab), "Scene");
    RC<UIImage> ui_image = GetUIStage()->CreateUIObject<UIImage>(HYP_NAME(Sample_Image), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));

    ui_image->OnMouseDrag.Bind([this, ui_image = ui_image.Get()](const UIMouseEventData &event)
    {
        if (event.mouse_buttons & MouseButtonState::RIGHT) {
            CameraController *camera_controller = m_camera->GetCameraController();

            if (EditorCameraController *editor_camera_controller = dynamic_cast<EditorCameraController *>(camera_controller)) {
                const Vec2f mouse_position = event.position + 0.5f;
                const Vec2i mouse_position_i = Vec2i { int(mouse_position.x * float(m_camera->GetWidth())), int(mouse_position.y * float(m_camera->GetHeight())) };

                DebugLog(LogType::Debug, "mx: %d, my: %d\n", mouse_position_i.x, mouse_position_i.y);

                if (m_scene) {
                    if (auto *controller = m_scene->GetCamera()->GetCameraController()) {
                        controller->PushCommand(CameraCommand {
                            .command = CameraCommand::CAMERA_COMMAND_MAG,
                            .mag_data = {
                                .mouse_x    = mouse_position_i.x,
                                .mouse_y    = mouse_position_i.y
                            }
                        });
                    }
                }
            }
        }

        if (m_camera->GetCameraController()->IsMouseLocked()) {
            const Vec2i position = ui_image->GetPosition();
            const Vec2i size = ui_image->GetActualSize();

            g_engine->GetAppContext()->GetCurrentWindow()->SetMousePosition(position + size / 2);
        }

        return UIEventHandlerResult::OK;
    }).Detach();

    ui_image->OnMouseDown.Bind([this](const UIMouseEventData &event)
    {
        DebugLog(LogType::Debug, "Game element gain focus, buttons: %u\n", uint32(event.mouse_buttons));

        auto *camera_controller = m_camera->GetCameraController();

        if (EditorCameraController *editor_camera_controller = dynamic_cast<EditorCameraController *>(camera_controller)) {
            if (event.mouse_buttons & MouseButtonState::RIGHT) {
                editor_camera_controller->SetMode(EditorCameraControllerMode::MOUSE_LOCKED);
            } else {
                editor_camera_controller->SetMode(EditorCameraControllerMode::FOCUSED);
            }
        }

        return UIEventHandlerResult::OK;
    }).Detach();

    ui_image->OnMouseUp.Bind([this](const UIMouseEventData &event)
    {
        DebugLog(LogType::Debug, "Game element lose focus, buttons: %u\n", uint32(event.mouse_buttons));

        auto *camera_controller = m_camera->GetCameraController();

        if (EditorCameraController *editor_camera_controller = dynamic_cast<EditorCameraController *>(camera_controller)) {
            editor_camera_controller->SetMode(EditorCameraControllerMode::INACTIVE);
        }

        return UIEventHandlerResult::OK;
    }).Detach();

    ui_image->SetTexture(m_scene_texture);
    scene_tab->GetContents()->AddChildUIObject(ui_image);

    auto game_tab = tab_view->AddTab(HYP_NAME(Game_Tab), "Game");

    for (int i = 0; i < 50; i++) {
        auto game_tab_content_button = GetUIStage()->CreateUIObject<UIButton>(CreateNameFromDynamicString(ANSIString("Hello_world_button_") + ANSIString::ToString(i)), Vec2i { 20, i * 25 }, UIObjectSize({ 50, UIObjectSize::PIXEL }, { 25, UIObjectSize::PIXEL }));
        // game_tab_content_button->SetParentAlignment(UIObjectAlignment::CENTER);
        // game_tab_content_button->SetOriginAlignment(UIObjectAlignment::CENTER);
        game_tab_content_button->SetText(String("Hello ") + String::ToString(i));
        game_tab->GetContents()->SetInnerSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::GROW }));
        game_tab->GetContents()->AddChildUIObject(game_tab_content_button);
    }

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

    // auto ui_text = GetUIStage()->CreateUIObject<UIText>(HYP_NAME(Sample_Text), Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::GROW }, { 18, UIObjectSize::PIXEL }));
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

    // const auto profile_results = Profile::RunInterleved(
    //     {
    //         new Profile([]()
    //         {
    //             HashMap<Vec3f, int> hash_map;
    //             for (uint i = 0; i < 30; i++) {
    //                 hash_map.Insert({ Vec3f { float((300-i) * 3), float((300-i) * 3 + 1), float((300-i) * 3 + 2) }, i * 100 });
    //             }
    //             for (uint i = 0; i < 30; i++) {
    //                 auto key = Vec3f { float((300-i) * 3), float((300-i) * 3 + 1), float((300-i) * 3 + 2) };

    //                 auto &value = hash_map[key];
    //                 value *= 100;
    //             }
    //         }),
    //         new Profile([]()
    //         {
    //             FlatMap<Vec3f, int> flat_map;
    //             for (uint i = 0; i < 30; i++) {
    //                 flat_map.Insert({ Vec3f { float((300-i) * 3), float((300-i) * 3 + 1), float((300-i) * 3 + 2) }, i * 100 });
    //             }
    //             for (uint i = 0; i < 30; i++) {
    //                 auto key = Vec3f { float((300-i) * 3), float((300-i) * 3 + 1), float((300-i) * 3 + 2) };

    //                 auto &value = flat_map[key];
    //                 value *= 100;
    //             }
    //         })
    //     },
    //     8,
    //     2500,
    //     2500
    // );

    // DebugLog(LogType::Debug, "Profile results:\n");
    // DebugLog(LogType::Debug, "\tfirst: %f\n", profile_results[0]);
    // DebugLog(LogType::Debug, "\tsecond: %f\n", profile_results[1]);

    // HYP_BREAKPOINT;

    GetScene()->GetCamera()->SetCameraController(RC<CameraController>(new EditorCameraController()));

    GetScene()->GetEnvironment()->AddRenderComponent<UIRenderer>(HYP_NAME(EditorUIRenderer), GetUIStage());
    
    Extent2D window_size;

    if (ApplicationWindow *current_window = GetAppContext()->GetCurrentWindow()) {
        window_size = current_window->GetDimensions();
    } else {
        window_size = Extent2D { 1280, 720 };
    }

    auto screen_capture_component = GetScene()->GetEnvironment()->AddRenderComponent<ScreenCaptureRenderComponent>(HYP_NAME(EditorSceneCapture), window_size);

    m_impl = new HyperionEditorImpl(GetScene(), GetScene()->GetCamera(), GetUIStage());
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
        .radius     = 18.0f,
        .resolution = { 2048, 2048 }
    });


    // temp
    auto batch = AssetManager::GetInstance()->CreateBatch();
    batch->Add("test_model", "models/sponza/sponza.obj");
    batch->LoadAsync();
    auto results = batch->AwaitResults();

    auto node = results["test_model"].ExtractAs<Node>();
    node.Scale(0.0125f);
    node.SetName("test_model");
    node.LockTransform();

    GetScene()->GetRoot().AddChild(node);
}

void HyperionEditor::Teardown()
{
    delete m_impl;
    m_impl = nullptr;
}

    
void HyperionEditor::Logic(GameCounter::TickUnit delta)
{

}

void HyperionEditor::OnInputEvent(const SystemEvent &event)
{
    Game::OnInputEvent(event);
}

void HyperionEditor::OnFrameEnd(Frame *frame)
{
}

#pragma endregion HyperionEditor

} // namespace editor
} // namespace hyperion