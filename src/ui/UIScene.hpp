#ifndef HYPERION_V2_UI_SCENE_HPP
#define HYPERION_V2_UI_SCENE_HPP

#include <ui/UIObject.hpp>

#include <core/Base.hpp>
#include <core/Containers.hpp>
#include <core/lib/Delegate.hpp>

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

class UIButton;
class FontAtlas;

class HYP_API UIScene : public BasicObject<STUB_CLASS(UIScene)>
{
public:
    // The minimum and maximum depth values for the UI scene for layering
    static const int min_depth = -10000;
    static const int max_depth = 10000;

    UIScene();
    UIScene(const UIScene &other)               = delete;
    UIScene &operator=(const UIScene &other)    = delete;
    ~UIScene();

    Vec2i GetSurfaceSize() const
        { return m_surface_size; }

    Handle<Scene> &GetScene()
        { return m_scene; }

    const Handle<Scene> &GetScene() const
        { return m_scene; }

    const RC<FontAtlas> &GetDefaultFontAtlas() const
        { return m_default_font_atlas; }

    void SetDefaultFontAtlas(RC<FontAtlas> font_atlas)
        { m_default_font_atlas = std::move(font_atlas); }

    template <class T>
    UIObjectProxy<T> CreateUIObject(
        Name name,
        Vec2i position,
        UIObjectSize size
    )
    {
        Threads::AssertOnThread(THREAD_GAME);
        AssertReady();

        NodeProxy node_proxy = m_scene->GetRoot().AddChild();
        
        // Set it to ignore parent scale so size of the UI object is not affected by the parent
        node_proxy->SetFlags(node_proxy->GetFlags() | NODE_FLAG_IGNORE_PARENT_SCALE);

        RC<UIObject> ui_object = CreateUIObjectInternal<T>(name, node_proxy, false /* init */);

        node_proxy->LockTransform(); // Lock the transform so it can't be modified by the user except through the UIObject
        ui_object->SetPosition(position);
        ui_object->SetSize(size);
        ui_object->Init();

        return UIObjectProxy<T>(std::move(node_proxy));
    }

    bool OnInputEvent(
        InputManager *input_manager,
        const SystemEvent &event
    );

    /*! \brief Ray test the UI scene using screen space mouse coordinates */
    bool TestRay(const Vec2f &position, RayTestResults &out_ray_test_results);

    void Init();
    void Update(GameCounter::TickUnit delta);

private:
    template <class T>
    RC<UIObject> CreateUIObjectInternal(Name name, NodeProxy &node_proxy, bool init = false)
    {
        AssertThrow(node_proxy.IsValid());

        static_assert(std::is_base_of_v<UIObject, T>, "T must be a derived class of UIObject");

        AssertReady();

        const ID<Entity> entity = m_scene->GetEntityManager()->AddEntity();
        node_proxy.SetEntity(entity);

        RC<UIObject> ui_object(new T(entity, this));
        ui_object->SetName(name);

        m_scene->GetEntityManager()->AddComponent(entity, UIComponent { ui_object });

        if (init) {
            ui_object->Init();
        }

        return ui_object;
    }

    bool Remove(ID<Entity> entity);

    Vec2i                       m_surface_size;

    Handle<Scene>               m_scene;

    RC<FontAtlas>               m_default_font_atlas;

    HashMap<ID<Entity>, float>  m_mouse_held_times;
    FlatSet<ID<Entity>>         m_hovered_entities;

    DelegateHandler             m_on_current_window_changed_handler;
};

} // namespace hyperion::v2

#endif