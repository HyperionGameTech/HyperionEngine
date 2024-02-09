#ifndef HYPERION_V2_UI_SCENE_H
#define HYPERION_V2_UI_SCENE_H

#include <core/Base.hpp>
#include <core/Containers.hpp>

#include <scene/Node.hpp>
#include <scene/NodeProxy.hpp>
#include <scene/Scene.hpp>
#include <scene/ecs/components/UIComponent.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/FullScreenPass.hpp>

#include <math/Transform.hpp>

#include <GameCounter.hpp>
#include <Types.hpp>

namespace hyperion {

class SystemEvent;
class InputManager;

} // namespace hyperion

namespace hyperion::v2 {

class UIScene;

// Helper function to get the scene from a UI scene
template <class UISceneType>
static inline Scene *GetScene(UISceneType *ui_scene)
{
    AssertThrow(ui_scene != nullptr);

    return ui_scene->GetScene().Get();
}

class UIObject : public BasicObject<STUB_CLASS(UIObject)>
{
public:
    UIObject(UIComponentType ui_component_type, UIScene *ui_scene);
    UIObject(const UIObject &other)                 = delete;
    UIObject &operator=(const UIObject &other)      = delete;
    UIObject(UIObject &&other) noexcept             = delete;
    UIObject &operator=(UIObject &&other) noexcept  = delete;
    ~UIObject();

    void Init();

    UIComponentType GetComponentType() const
        { return m_component_type; }

    Name GetName() const;
    void SetName(Name name);

    const Vector2 &GetPosition() const;
    void SetPosition(const Vector2 &position);

    const Vector2 &GetSize() const;
    void SetSize(const Vector2 &size);

    const String &GetText() const;
    void SetText(const String &text);

    template <UIComponentEvent EventType>
    void AddEventListener(
        Proc<bool, const UIComponentEventData &> &&handler
    )
    {
        Threads::AssertOnThread(THREAD_GAME);

        GetScene(m_ui_scene)->GetEntityManager()->AddComponent(
            m_entity,
            UIEventHandlerComponent<EventType> {
                std::move(handler)
            }
        );
    }

protected:
    UIComponentType m_component_type;
    ID<Entity>      m_entity;
    UIScene         *m_ui_scene;
};

class UIScene : public BasicObject<STUB_CLASS(UIScene)>
{
public:
    UIScene();
    UIScene(const UIScene &other) = delete;
    UIScene &operator=(const UIScene &other) = delete;
    ~UIScene();

    Handle<Scene> &GetScene()
        { return m_scene; }

    const Handle<Scene> &GetScene() const
        { return m_scene; }

    NodeProxy CreateButton(
        const Vector2 &position,
        const Vector2 &size,
        const String &text
    );

    bool Remove(ID<UIObject> id);

    bool OnInputEvent(
        InputManager *input_manager,
        const SystemEvent &event
    );

    /*! \brief Ray test the UI scene using screen space mouse coordinates */
    bool TestRay(const Vector2 &position, RayHit &out_first_hit);

    void Init();
    void Update(GameCounter::TickUnit delta);

private:
    Handle<Scene>               m_scene;
    FlatMap<ID<Entity>, float>  m_mouse_held_times;
    Array<Handle<UIObject>>     m_ui_objects;
};

} // namespace hyperion::v2

#endif