/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_STAGE_HPP
#define HYPERION_UI_STAGE_HPP

#include <ui/UIObject.hpp>

#include <core/Base.hpp>

#include <core/object/HypObject.hpp>

#include <core/functional/Delegate.hpp>

#include <scene/Node.hpp>
#include <scene/NodeProxy.hpp>
#include <scene/Scene.hpp>

#include <scene/ecs/components/UIComponent.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/FullScreenPass.hpp>

#include <input/Mouse.hpp>
#include <input/Keyboard.hpp>

#include <math/Transform.hpp>

#include <GameCounter.hpp>
#include <HashCode.hpp>
#include <Types.hpp>

namespace hyperion {
namespace sys {
class SystemEvent;
} // namespace sys

using sys::SystemEvent;

class InputManager;

} // namespace hyperion

namespace hyperion {

class UIButton;
class FontAtlas;

struct UIObjectPressedState
{
    EnumFlags<MouseButtonState> mouse_buttons = MouseButtonState::NONE;
    float                       held_time = 0.0f;
};

enum class UIRayTestFlags : uint32
{
    NONE            = 0x0,
    ONLY_VISIBLE    = 0x1,

    DEFAULT         = ONLY_VISIBLE
};

HYP_MAKE_ENUM_FLAGS(UIRayTestFlags)

/*! \brief The UIStage is the root of the UI scene graph. */

HYP_CLASS()
class HYP_API UIStage : public UIObject
{
    HYP_OBJECT_BODY(UIStage);

public:
    friend class UIObject;

    // The minimum and maximum depth values for the UI scene for layering
    static const int min_depth = -10000;
    static const int max_depth = 10000;

    UIStage(ThreadID owner_thread_id);
    UIStage(const UIStage &other)               = delete;
    UIStage &operator=(const UIStage &other)    = delete;
    virtual ~UIStage() override;

    /*! \brief Get the size of the surface that the UI objects are rendered on.
     * 
     *  \return The size of the surface. */
    HYP_METHOD()
    HYP_FORCE_INLINE Vec2i GetSurfaceSize() const
        { return m_surface_size; }

    HYP_METHOD()
    void SetSurfaceSize(Vec2i surface_size);

    /*! \brief Get the scene that contains the UI objects.
     * 
     *  \return Handle to the scene. */
    HYP_METHOD()
    virtual Scene *GetScene() const override;

    /*! \brief Set the scene for this UIStage.
     *  \internal Used internally, for serialization.
     * 
     *  \param scene The scene to set. */
    HYP_METHOD()
    void SetScene(const Handle<Scene> &scene);

    /*! \brief Get the default font atlas to use for text rendering.
     *  UIText objects will use this font atlas if they don't have a font atlas set.
     * 
     *  \return The default font atlas. */
    const RC<FontAtlas> &GetDefaultFontAtlas() const;

    /*! \brief Set the default font atlas to use for text rendering.
     *  UIText objects will use this font atlas if they don't have a font atlas set.
     * 
     *  \param font_atlas The font atlas to set. */
    void SetDefaultFontAtlas(RC<FontAtlas> font_atlas);

    /*! \brief Get the UI object that is currently focused. If no object is focused, returns nullptr.
     *  \note Because the focused object is a weak reference, a lock is required to access the object.
     *  \return The focused UI object. */
    HYP_METHOD()
    HYP_FORCE_INLINE RC<UIObject> GetFocusedObject() const
        { return m_focused_object.Lock(); }

    /*! \brief Create a UI object of type T and optionally attach it to the root.
     *  The object will not be named. To name the object, use the other CreateUIObject overload.
     * 
     *  \tparam T The type of UI object to create. Must be a derived class of UIObject.
     *  \param position The position of the UI object.
     *  \param size The size of the UI object.
     *  \param attach_to_root Whether to attach the UI object to the root of the UI scene immediately.
     *  \return A handle to the created UI object. */
    template <class T>
    HYP_NODISCARD RC<T> CreateUIObject(
        Vec2i position,
        UIObjectSize size,
        bool attach_to_root = false
    )
    {
        return CreateUIObject<T>(Name::Invalid(), position, size, attach_to_root);
    }

    /*! \brief Create a UI object of type T and optionally attach it to the root.
     * 
     *  \tparam T The type of UI object to create. Must be a derived class of UIObject.
     *  \param name The name of the UI object.
     *  \param position The position of the UI object.
     *  \param size The size of the UI object.
     *  \param attach_to_root Whether to attach the UI object to the root of the UI scene immediately.
     *  \return A handle to the created UI object. */
    template <class T>
    HYP_NODISCARD RC<T> CreateUIObject(
        Name name,
        Vec2i position,
        UIObjectSize size,
        bool attach_to_root = false
    )
    {
        Threads::AssertOnThread(m_owner_thread_id);

        AssertThrow(IsInit());
        AssertThrow(GetNode().IsValid());

        if (!name.IsValid()) {
            name = CreateNameFromDynamicString(ANSIString("Unnamed_") + TypeNameHelper<T, true>::value.Data());
        }

        NodeProxy node_proxy(MakeRefCountedPtr<Node>(name.LookupString()));
        
        if (attach_to_root) {
            node_proxy = GetNode()->AddChild(node_proxy);
        }
        
        // Set it to ignore parent scale so size of the UI object is not affected by the parent
        node_proxy->SetFlags(node_proxy->GetFlags() | NodeFlags::IGNORE_PARENT_SCALE);

        RC<UIObject> ui_object = CreateUIObjectInternal<T>(name, node_proxy, false /* init */);

        ui_object->SetPosition(position);
        ui_object->SetSize(size);
        ui_object->Init();

        RC<T> result = ui_object.Cast<T>();
        AssertThrow(result != nullptr);

        return result;
    }

    UIEventHandlerResult OnInputEvent(
        InputManager *input_manager,
        const SystemEvent &event
    );

    /*! \brief Ray test the UI scene using screen space mouse coordinates */
    bool TestRay(const Vec2f &position, Array<RC<UIObject>> &out_objects, EnumFlags<UIRayTestFlags> flags = UIRayTestFlags::DEFAULT);

    /*! \brief Set the owner thread ID for the UIStage, as well as its
     *  underlying UIObjects.
     *  \note Ensure that the UIStage will not be accessed from any other
     *  thread than the one specified. This method is not thread-safe. */
    void SetOwnerThreadID(ThreadID thread_id);

    virtual bool IsContainer() const override
        { return true; }

    virtual void Init() override;
    virtual void AddChildUIObject(UIObject *ui_object) override;

protected:
    virtual void Update_Internal(GameCounter::TickUnit delta) override;
    
    virtual void OnAttached_Internal(UIObject *parent) override;
    
    // Override OnRemoved_Internal to update subobjects to have this as a stage
    virtual void OnRemoved_Internal() override;

    virtual void SetStage_Internal(UIStage *stage) override;

private:
    virtual void ComputeActualSize(const UIObjectSize &in_size, Vec2i &out_actual_size, UpdateSizePhase phase, bool is_inner) override;

    /*! \brief To be called internally from UIObject only */
    void SetFocusedObject(const RC<UIObject> &ui_object);

    RC<UIObject> GetUIObjectForEntity(ID<Entity> entity) const;

    template <class T>
    RC<UIObject> CreateUIObjectInternal(Name name, NodeProxy &node_proxy, bool init = false)
    {
        AssertThrow(node_proxy.IsValid());

        static_assert(std::is_base_of_v<UIObject, T>, "T must be a derived class of UIObject");

        const ID<Entity> entity = node_proxy->GetScene()->GetEntityManager()->AddEntity();
        node_proxy->SetEntity(entity);
        // node_proxy->LockTransform(); // Lock the transform so it can't be modified by the user except through the UIObject

        RC<UIObject> ui_object = MakeRefCountedPtr<T>();
        AssertThrow(ui_object.GetTypeID() == TypeID::ForType<T>());

        ui_object->SetStage(this);
        ui_object->SetNodeProxy(node_proxy);

        ui_object->SetName(name);

        node_proxy->GetScene()->GetEntityManager()->AddComponent<UIComponent>(entity, UIComponent { ui_object.Get() });

        if (init) {
            ui_object->Init();
        }

        return ui_object;
    }

    bool Remove(ID<Entity> entity);

    ThreadID                                        m_owner_thread_id;

    Vec2i                                           m_surface_size;

    Handle<Scene>                                   m_scene;

    RC<FontAtlas>                                   m_default_font_atlas;

    FlatMap<Weak<UIObject>, UIObjectPressedState>   m_mouse_button_pressed_states;
    FlatSet<Weak<UIObject>>                         m_hovered_ui_objects;
    HashMap<KeyCode, Array<Weak<UIObject>>>         m_keyed_down_objects;

    Weak<UIObject>                                  m_focused_object;

    DelegateHandler                                 m_on_current_window_changed_handler;
};

} // namespace hyperion

#endif