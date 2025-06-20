/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_STAGE_HPP
#define HYPERION_UI_STAGE_HPP

#include <ui/UIObject.hpp>

#include <core/Base.hpp>

#include <core/object/HypObject.hpp>

#include <core/functional/Delegate.hpp>

#include <scene/Node.hpp>
#include <scene/Scene.hpp>

#include <scene/ecs/components/UIComponent.hpp>

#include <rendering/backend/RendererStructs.hpp>

#include <input/Mouse.hpp>
#include <input/Keyboard.hpp>

#include <core/math/Transform.hpp>

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
    float held_time = 0.0f;
};

enum class UIRayTestFlags : uint32
{
    NONE = 0x0,
    ONLY_VISIBLE = 0x1,

    DEFAULT = ONLY_VISIBLE
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
    static const int g_min_depth = -10000;
    static const int g_max_depth = 10000;

    UIStage();
    UIStage(ThreadID owner_thread_id);
    UIStage(const UIStage& other) = delete;
    UIStage& operator=(const UIStage& other) = delete;
    virtual ~UIStage() override;

    /*! \brief Get the size of the surface that the UI objects are rendered on.
     *
     *  \return The size of the surface. */
    HYP_METHOD()
    HYP_FORCE_INLINE Vec2i GetSurfaceSize() const
    {
        return m_surface_size;
    }

    HYP_METHOD()
    void SetSurfaceSize(Vec2i surface_size);

    /*! \brief Get the scene that contains the UI objects.
     *
     *  \return Handle to the scene. */
    HYP_METHOD()
    virtual Scene* GetScene() const override;

    /*! \brief Set the scene for this UIStage.
     *  \internal Used internally, for serialization.
     *
     *  \param scene The scene to set. */
    HYP_METHOD()
    void SetScene(const Handle<Scene>& scene);

    HYP_METHOD()
    const Handle<Camera>& GetCamera() const
    {
        return m_camera;
    }

    /*! \brief Get the default font atlas to use for text rendering.
     *  UIText objects will use this font atlas if they don't have a font atlas set.
     *
     *  \return The default font atlas. */
    const RC<FontAtlas>& GetDefaultFontAtlas() const;

    /*! \brief Set the default font atlas to use for text rendering.
     *  UIText objects will use this font atlas if they don't have a font atlas set.
     *
     *  \param font_atlas The font atlas to set. */
    void SetDefaultFontAtlas(RC<FontAtlas> font_atlas);

    /*! \brief Get the UI object that is currently focused. If no object is focused, returns nullptr.
     *  \return The focused UI object. */
    HYP_FORCE_INLINE const WeakHandle<UIObject>& GetFocusedObject() const
    {
        return m_focused_object;
    }

    UIEventHandlerResult OnInputEvent(
        InputManager* input_manager,
        const SystemEvent& event);

    /*! \brief Ray test the UI scene using screen space mouse coordinates */
    bool TestRay(const Vec2f& position, Array<Handle<UIObject>>& out_objects, EnumFlags<UIRayTestFlags> flags = UIRayTestFlags::DEFAULT);

    virtual void AddChildUIObject(const Handle<UIObject>& ui_object) override;

protected:
    virtual void Init() override;

    virtual void Update_Internal(float delta) override;

    virtual void OnAttached_Internal(UIObject* parent) override;

    // Override OnRemoved_Internal to update subobjects to have this as a stage
    virtual void OnRemoved_Internal() override;

    virtual void SetStage_Internal(UIStage* stage) override;

private:
    virtual void ComputeActualSize(const UIObjectSize& in_size, Vec2i& out_actual_size, UpdateSizePhase phase, bool is_inner) override;

    /*! \brief To be called internally from UIObject only */
    void SetFocusedObject(const Handle<UIObject>& ui_object);

    Handle<UIObject> GetUIObjectForEntity(const Entity* entity) const;

    bool Remove(const Entity* entity);

    Vec2i m_surface_size;

    Handle<Scene> m_scene;
    Handle<Camera> m_camera;

    RC<FontAtlas> m_default_font_atlas;

    HashMap<WeakHandle<UIObject>, UIObjectPressedState> m_mouse_button_pressed_states;
    FlatSet<WeakHandle<UIObject>> m_hovered_ui_objects;
    HashMap<KeyCode, Array<WeakHandle<UIObject>>> m_keyed_down_objects;

    WeakHandle<UIObject> m_focused_object;

    DelegateHandler m_on_current_window_changed_handler;
};

} // namespace hyperion

#endif