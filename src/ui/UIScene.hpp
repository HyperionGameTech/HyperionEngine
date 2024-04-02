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

class UIScene : public BasicObject<STUB_CLASS(UIScene)>
{
public:
    UIScene();
    UIScene(const UIScene &other) = delete;
    UIScene &operator=(const UIScene &other) = delete;
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
        Vec2i position,
        Vec2i size
    )
    {
        Threads::AssertOnThread(THREAD_GAME);
        AssertReady();

        RC<UIObject> ui_object = CreateUIObjectInternal<T>();
        ui_object->SetPosition(position);
        ui_object->SetSize(size);

        NodeProxy node_proxy = m_scene->GetRoot().AddChild();
        node_proxy.SetEntity(ui_object->GetEntity());
        node_proxy.LockTransform(); // Lock the transform so it can't be modified by the user except through the UIObject

        // Set it to ignore parent scale so size of the UI object is not affected by the parent
        node_proxy.Get()->SetFlags(node_proxy.Get()->GetFlags() | NODE_FLAG_IGNORE_PARENT_SCALE);

        return UIObjectProxy<T>(std::move(node_proxy));
    }

    bool OnInputEvent(
        InputManager *input_manager,
        const SystemEvent &event
    );

    /*! \brief Ray test the UI scene using screen space mouse coordinates */
    bool TestRay(const Vector2 &position, RayHit &out_first_hit);

    void Init();
    void Update(GameCounter::TickUnit delta);

private:
    template <class T>
    RC<UIObject> CreateUIObjectInternal()
    {
        static_assert(std::is_base_of_v<UIObject, T>, "T must be a derived class of UIObject");

        AssertReady();

        const ID<Entity> entity = m_scene->GetEntityManager()->AddEntity();

        RC<UIObject> ui_object(new T(entity, this));

        m_scene->GetEntityManager()->AddComponent(entity, UIComponent { ui_object });

        ui_object->Init();

        return ui_object;
    }

    bool Remove(ID<Entity> entity);

    Vec2i                       m_surface_size;

    Handle<Scene>               m_scene;

    RC<FontAtlas>               m_default_font_atlas;

    FlatMap<ID<Entity>, float>  m_mouse_held_times;
};

} // namespace hyperion::v2

#endif