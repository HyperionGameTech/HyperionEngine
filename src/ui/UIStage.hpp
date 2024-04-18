/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_STAGE_HPP
#define HYPERION_UI_STAGE_HPP

#include <ui/UIObject.hpp>

#include <core/Base.hpp>
#include <core/Containers.hpp>
#include <core/functional/Delegate.hpp>

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

namespace hyperion {

class UIButton;
class FontAtlas;

class UIStage : public BasicObject<STUB_CLASS(UIStage)>
{
public:
    // The minimum and maximum depth values for the UI scene for layering
    static const int min_depth = -10000;
    static const int max_depth = 10000;

    HYP_API UIStage();
    UIStage(const UIStage &other)               = delete;
    UIStage &operator=(const UIStage &other)    = delete;
    HYP_API ~UIStage();

    /*! \brief Get the size of the surface that the UI objects are rendered on.
     * 
     * \return The size of the surface. */
    [[nodiscard]]
    HYP_FORCE_INLINE
    Vec2i GetSurfaceSize() const
        { return m_surface_size; }

    /*! \brief Get the scene that contains the UI objects.
     * 
     * \return Handle to the scene. */
    [[nodiscard]]
    HYP_FORCE_INLINE
    const Handle<Scene> &GetScene() const
        { return m_scene; }

    /*! \brief Get the default font atlas to use for text rendering.
     * UIText objects will use this font atlas if they don't have a font atlas set.
     * 
     * \return The default font atlas. */
    [[nodiscard]]
    HYP_FORCE_INLINE
    const RC<FontAtlas> &GetDefaultFontAtlas() const
        { return m_default_font_atlas; }

    /*! \brief Set the default font atlas to use for text rendering.
     * UIText objects will use this font atlas if they don't have a font atlas set.
     * 
     * \param font_atlas The font atlas to set. */
    HYP_FORCE_INLINE
    void SetDefaultFontAtlas(RC<FontAtlas> font_atlas)
        { m_default_font_atlas = std::move(font_atlas); }

    /*! \brief Create a UI object of type T and optionally attach it to the root.
     * 
     * \tparam T The type of UI object to create. Must be a derived class of UIObject.
     * \param name The name of the UI object.
     * \param position The position of the UI object.
     * \param size The size of the UI object.
     * \param attach_to_root Whether to attach the UI object to the root of the UI scene immediately.
     * \return A handle to the created UI object. */
    template <class T>
    RC<T> CreateUIObject(
        Name name,
        Vec2i position,
        UIObjectSize size,
        bool attach_to_root = false
    )
    {
        Threads::AssertOnThread(ThreadName::THREAD_GAME);
        AssertReady();

        NodeProxy node_proxy(new Node(name.LookupString()));
        
        if (attach_to_root) {
            node_proxy = m_scene->GetRoot().AddChild(node_proxy);
        }
        
        // Set it to ignore parent scale so size of the UI object is not affected by the parent
        node_proxy->SetFlags(node_proxy->GetFlags() | NODE_FLAG_IGNORE_PARENT_SCALE);

        RC<UIObject> ui_object = CreateUIObjectInternal<T>(name, node_proxy, false /* init */);

        ui_object->SetPosition(position);
        ui_object->SetSize(size);
        ui_object->Init();

        return ui_object.Cast<T>();
    }

    HYP_API bool OnInputEvent(
        InputManager *input_manager,
        const SystemEvent &event
    );

    /*! \brief Ray test the UI scene using screen space mouse coordinates */
    HYP_API bool TestRay(const Vec2f &position, RayTestResults &out_ray_test_results);

    HYP_API void Init();
    HYP_API void Update(GameCounter::TickUnit delta);

private:
    template <class T>
    RC<UIObject> CreateUIObjectInternal(Name name, NodeProxy &node_proxy, bool init = false)
    {
        AssertThrow(node_proxy.IsValid());

        static_assert(std::is_base_of_v<UIObject, T>, "T must be a derived class of UIObject");

        AssertReady();

        const ID<Entity> entity = node_proxy->GetScene()->GetEntityManager()->AddEntity();
        node_proxy->SetEntity(entity);
        node_proxy->LockTransform(); // Lock the transform so it can't be modified by the user except through the UIObject

        RC<UIObject> ui_object(new T(entity, this, node_proxy));
        ui_object->SetName(name);

        node_proxy->GetScene()->GetEntityManager()->AddComponent(entity, UIComponent { ui_object });

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

} // namespace hyperion

#endif