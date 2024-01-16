#include <ui/UIScene.hpp>
#include <util/MeshBuilder.hpp>
#include <scene/camera/OrthoCamera.hpp>
#include <scene/camera/PerspectiveCamera.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <system/Application.hpp>
#include <input/InputManager.hpp>
#include <Threads.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

// UIObject

UIObject::UIObject(UIComponentType component_type, UIScene *ui_scene)
    : BasicObject(),
      m_component_type(component_type),
      m_ui_scene(ui_scene)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertThrow(m_ui_scene != nullptr);
    AssertThrow(m_ui_scene->IsReady());

    m_entity = m_ui_scene->GetScene()->GetEntityManager()->AddEntity();

    m_ui_scene->GetScene()->GetEntityManager()->AddComponent(m_entity, UIComponent {
        m_component_type
    });

    // auto font_texture = g_asset_manager->Load<Texture>("textures/fontmap.png");
            
    // FontMap font_map(
    //     font_texture,
    //     Extent2D { 32, 32 }
    // );

    // auto mesh = UIText::BuildTextMesh(font_map, "HyperionEngine v0.2");

    Handle<Mesh> mesh = MeshBuilder::Quad();
    InitObject(mesh);

    m_ui_scene->GetScene()->GetEntityManager()->AddComponent(m_entity, MeshComponent {
        mesh,
        g_material_system->GetOrCreate(
            MaterialAttributes {
                .shader_definition = ShaderDefinition {
                    HYP_NAME(UIObject),
                    ShaderProperties(mesh->GetVertexAttributes())
                },
                .bucket = Bucket::BUCKET_UI,
                .blend_mode = BlendMode::NORMAL,
                .cull_faces = FaceCullMode::NONE,
            },
            { }
        )
    });

    m_ui_scene->GetScene()->GetEntityManager()->AddComponent(m_entity, VisibilityStateComponent {
        VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE
    });

    m_ui_scene->GetScene()->GetEntityManager()->AddComponent(m_entity, TransformComponent {
        Transform(
            Vec3f::zero,
            Vec3f(0.25f), // test
            Quaternion::identity
        )
    });

    m_ui_scene->GetScene()->GetEntityManager()->AddComponent(m_entity, BoundingBoxComponent {
        mesh->GetAABB()
    });
}

UIObject::~UIObject()
{
    if (m_ui_scene) {
        m_ui_scene->GetScene()->GetEntityManager()->RemoveEntity(m_entity);
    }
}

void UIObject::Init()
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertThrow(m_ui_scene != nullptr);
    AssertThrow(m_ui_scene->IsReady());

    BasicObject::Init();
}

Name UIObject::GetName() const
{
    Threads::AssertOnThread(THREAD_GAME);

    return m_ui_scene->GetScene()->GetEntityManager()->GetComponent<UIComponent>(m_entity).name;
}

void UIObject::SetName(Name name)
{
    Threads::AssertOnThread(THREAD_GAME);

    UIComponent &ui_component = m_ui_scene->GetScene()->GetEntityManager()->GetComponent<UIComponent>(m_entity);

    ui_component.name = name;
}

const Vector2 &UIObject::GetPosition() const
{
    Threads::AssertOnThread(THREAD_GAME);

    return m_ui_scene->GetScene()->GetEntityManager()->GetComponent<UIComponent>(m_entity).bounds.position;
}

void UIObject::SetPosition(const Vector2 &position)
{
    Threads::AssertOnThread(THREAD_GAME);

    UIComponent &ui_component = m_ui_scene->GetScene()->GetEntityManager()->GetComponent<UIComponent>(m_entity);

    ui_component.bounds.position = position;
}

const Vector2 &UIObject::GetSize() const
{
    Threads::AssertOnThread(THREAD_GAME);

    return m_ui_scene->GetScene()->GetEntityManager()->GetComponent<UIComponent>(m_entity).bounds.size;
}

void UIObject::SetSize(const Vector2 &size)
{
    Threads::AssertOnThread(THREAD_GAME);

    UIComponent &ui_component = m_ui_scene->GetScene()->GetEntityManager()->GetComponent<UIComponent>(m_entity);

    ui_component.bounds.size = size;
}

const String &UIObject::GetText() const
{
    Threads::AssertOnThread(THREAD_GAME);

    UIComponent &ui_component = m_ui_scene->GetScene()->GetEntityManager()->GetComponent<UIComponent>(m_entity);

    if (ui_component.text.HasValue()) {
        return ui_component.text.Get();
    }

    return String::empty;
}

void UIObject::SetText(const String &text)
{
    Threads::AssertOnThread(THREAD_GAME);

    UIComponent &ui_component = m_ui_scene->GetScene()->GetEntityManager()->GetComponent<UIComponent>(m_entity);

    ui_component.text = text;
}

// UIScene

UIScene::UIScene()
    : BasicObject()
{
}

UIScene::~UIScene()
{
    Teardown();
}

void UIScene::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();

    m_scene = CreateObject<Scene>(CreateObject<Camera>());

    m_scene->GetCamera()->SetCameraController(RC<OrthoCameraController>::Construct(
        0.0f, 1.0f,
        0.0f, 1.0f,
        -1.0f, 1.0f
    ));

    InitObject(m_scene);

    // m_scene->GetCamera()->SetDirection(Vector3(0.0f, 0.0f, -1.0f));

    SetReady(true);

    OnTeardown([this](...) {
        SetReady(false);

        // m_ui_objects.Clear();
        m_scene.Reset();
    });
}

NodeProxy UIScene::CreateButton(const Vector2 &position, const Vector2 &size, const String &text)
{
    Threads::AssertOnThread(THREAD_GAME);
    AssertReady();

    auto entity = m_scene->GetEntityManager()->AddEntity();

    m_scene->GetEntityManager()->AddComponent(entity, UIComponent {
        UI_COMPONENT_TYPE_BUTTON
    });

    Handle<Mesh> mesh = MeshBuilder::Quad();
    InitObject(mesh);

    m_scene->GetEntityManager()->AddComponent(entity, MeshComponent {
        mesh,
        g_material_system->GetOrCreate(
            MaterialAttributes {
                .shader_definition = ShaderDefinition {
                    HYP_NAME(UIObject),
                    ShaderProperties(mesh->GetVertexAttributes())
                },
                .bucket = Bucket::BUCKET_UI,
                .blend_mode = BlendMode::NORMAL,
                .cull_faces = FaceCullMode::NONE,
            },
            { }
        )
    });

    m_scene->GetEntityManager()->AddComponent(entity, VisibilityStateComponent {
        VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE
    });

    m_scene->GetEntityManager()->AddComponent(entity, TransformComponent {
        Transform(
            Vec3f::zero,
            Vec3f(0.25f), // test
            Quaternion::identity
        )
    });
    m_scene->GetEntityManager()->AddComponent(entity, BoundingBoxComponent {
        mesh->GetAABB()
    });

    auto node_proxy = m_scene->GetRoot().AddChild();
    node_proxy.SetLocalTranslation(Vec3f(position.x, position.y, 0.0f));
    node_proxy.SetLocalScale(Vec3f(size.x, size.y, 1.0f));
    node_proxy.SetEntity(entity);

    // Handle<UIObject> button = CreateObject<UIObject>(UI_COMPONENT_TYPE_BUTTON, this);

    // button->SetPosition(position);
    // button->SetSize(size);
    // button->SetText(text);

    // InitObject(button);

    // m_ui_objects.PushBack(button);

    return node_proxy;
}

Bool UIScene::Remove(ID<UIObject> id)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertThrow(IsInitCalled());
    
    for (auto &it : m_ui_objects) {
        if (it.GetID() == id) {
            m_ui_objects.Erase(it);

            return true;
        }
    }

    return false;
}

void UIScene::Update(GameCounter::TickUnit delta)
{
    m_scene->Update(delta);

    for (auto &it : m_mouse_held_times) {
        it.second += delta;
    }
}

bool UIScene::TestRay(const Vector2 &position, RayHit &out_first_hit)
{
    Threads::AssertOnThread(THREAD_GAME);

    Vector4 world_position = m_scene->GetCamera()->TransformScreenToWorld(position);
    world_position.x = -world_position.x;

    const Vector3 direction = Vector3(world_position.x / world_position.w, world_position.y / world_position.w, 0.0f);

    Ray ray;
    ray.position = world_position.GetXYZ() / world_position.w;
    ray.direction = direction;
    
    RayTestResults results;

    for (auto [entity_id, ui_component, transform_component, bounding_box_component] : m_scene->GetEntityManager()->GetEntitySet<UIComponent, TransformComponent, BoundingBoxComponent>()) {
        if (bounding_box_component.world_aabb.ContainsPoint(direction)) {
            RayHit hit { };
            hit.hitpoint = Vector3(position.x, position.y, 0.0f);
            hit.distance = m_scene->GetCamera()->TransformWorldToNDC(transform_component.transform.GetTranslation()).z;
            hit.id = entity_id.value;

            results.AddHit(hit);
        }
    }

    if (results.Any()) {
        out_first_hit = results.Front();
        
        return true;
    }

    return false;
}

bool UIScene::OnInputEvent(
    InputManager *input_manager,
    const SystemEvent &event
)
{
    switch (event.GetType()) {
    case SystemEventType::EVENT_MOUSEMOTION: {
        // check intersects with objects on mouse movement.
        // for any objects that had mouse held on them,
        // if the mouse is on them, signal mouse movement

        // project a ray into the scene and test if it hits any objects
        RayHit hit;

        const auto &mouse_position = input_manager->GetMousePosition();
        const auto mouse_x = mouse_position.x.load();
        const auto mouse_y = mouse_position.y.load();

        const auto extent = input_manager->GetWindow()->GetExtent();

        const Vector2 mouse_screen(
            Float(mouse_x) / Float(extent.width),
            Float(mouse_y) / Float(extent.height)
        );

        Bool event_handled = false;

        { // mouse drag event
            UIComponentEventData drag_event;
            drag_event.event = UI_COMPONENT_EVENT_MOUSE_DRAG;
            drag_event.mouse_position = mouse_screen;

            for (auto &it : m_mouse_held_times) {
                if (it.second >= 0.05f) {
                    // signal mouse drag
                    if (auto *mouse_drag_event_handler_component = m_scene->GetEntityManager()->TryGetComponent<UIEventHandlerComponent<UI_COMPONENT_EVENT_MOUSE_DRAG>>(it.first); mouse_drag_event_handler_component && mouse_drag_event_handler_component->handler) {
                        if (mouse_drag_event_handler_component->handler(drag_event)) {
                            event_handled = true;
                        }
                    }
                }
            }
        }

        if (TestRay(mouse_screen, hit)) {
            auto it = m_mouse_held_times.Find(hit.id);

            if (it == m_mouse_held_times.End()) {
                UIComponentEventData hover_event;
                hover_event.event = UI_COMPONENT_EVENT_MOUSE_HOVER;
                hover_event.mouse_position = mouse_screen;

                if (auto *mouse_hover_event_handler_component = m_scene->GetEntityManager()->TryGetComponent<UIEventHandlerComponent<UI_COMPONENT_EVENT_MOUSE_HOVER>>(hit.id); mouse_hover_event_handler_component && mouse_hover_event_handler_component->handler) {
                    if (mouse_hover_event_handler_component->handler(hover_event)) {
                        event_handled = true;
                    }
                }
            }
        }

        return event_handled;
    }
    case SystemEventType::EVENT_MOUSEBUTTON_DOWN: {
        // project a ray into the scene and test if it hits any objects
        RayHit hit;

        const auto &mouse_position = input_manager->GetMousePosition();
        const auto mouse_x = mouse_position.x.load();
        const auto mouse_y = mouse_position.y.load();

        const auto extent = input_manager->GetWindow()->GetExtent();

        const Vector2 mouse_screen(
            Float(mouse_x) / Float(extent.width),
            Float(mouse_y) / Float(extent.height)
        );

        if (TestRay(mouse_screen, hit)) {
            UIComponentEventData mouse_down_event;
            mouse_down_event.event = UI_COMPONENT_EVENT_MOUSE_DOWN;
            mouse_down_event.mouse_position = mouse_screen;

            m_mouse_held_times.Insert(hit.id, 0.0f);

            // trigger mouse down
            if (auto *mouse_down_event_handler_component = m_scene->GetEntityManager()->TryGetComponent<UIEventHandlerComponent<UI_COMPONENT_EVENT_MOUSE_DOWN>>(hit.id); mouse_down_event_handler_component && mouse_down_event_handler_component->handler) {
                if (mouse_down_event_handler_component->handler(mouse_down_event)) {
                    return true;
                }
            }
        }

        break;
    }
    case SystemEventType::EVENT_MOUSEBUTTON_UP: {
        const auto &mouse_position = input_manager->GetMousePosition();
        const auto mouse_x = mouse_position.x.load();
        const auto mouse_y = mouse_position.y.load();

        const auto extent = input_manager->GetWindow()->GetExtent();

        const Vector2 mouse_screen(
            Float(mouse_x) / Float(extent.width),
            Float(mouse_y) / Float(extent.height)
        );

        bool result = false;

        for (auto &it : m_mouse_held_times) {
            // trigger click
            RayHit hit;

            if (TestRay(mouse_screen, hit)) {
                UIComponentEventData click_event;
                click_event.event = UI_COMPONENT_EVENT_CLICK;
                click_event.mouse_position = mouse_screen;

                // Check if the entity has a click event handler
                // and trigger it if it does
                if (auto *click_event_handler_component = m_scene->GetEntityManager()->TryGetComponent<UIEventHandlerComponent<UI_COMPONENT_EVENT_CLICK>>(hit.id); click_event_handler_component && click_event_handler_component->handler) {
                    if (click_event_handler_component->handler(click_event)) {
                        result = true;
                    }
                }
            }

            UIComponentEventData mouse_up_event;
            mouse_up_event.event = UI_COMPONENT_EVENT_MOUSE_UP;
            mouse_up_event.mouse_position = mouse_screen;

            // trigger mouse up
            if (auto *mouse_up_event_handler_component = m_scene->GetEntityManager()->TryGetComponent<UIEventHandlerComponent<UI_COMPONENT_EVENT_MOUSE_UP>>(it.first); mouse_up_event_handler_component && mouse_up_event_handler_component->handler) {
                if (mouse_up_event_handler_component->handler(mouse_up_event)) {
                    result = true;
                }
            }

            if (result) {
                break;
            }
        }

        m_mouse_held_times.Clear();

        return result;
    }
    }

    // no handle for it
    return false;
}

} // namespace hyperion::v2