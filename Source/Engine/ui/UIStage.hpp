/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/HashCode.hpp>
#include <Core/Types.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <Core/functional/Delegate.hpp>

#include <Core/utilities/Time.hpp>

#include <Core/containers/SparsePagedArray.hpp>

#include <scene/Node.hpp>
#include <scene/Scene.hpp>

#include <scene/components/UIComponent.hpp>

#include <rendering/Shared.hpp>

#include <ui/UIObject.hpp>
#include <ui/UIUpdateManager.hpp>

#include <input/Mouse.hpp>
#include <input/Keyboard.hpp>

namespace Hyperion {

class Event;
class InputManager;
class UIButton;
class FontAtlas;

struct UIObjectMouseState
{
    EnumFlags<MouseButtonState> mouseButtons = MouseButtonState::NONE;
    float heldTime = 0.0f;
    Vec2f originalMousePosition;
};

struct UIObjectKeyState
{
    uint32 count = 0;
    float heldTime = 0.0f;
};

enum class UIRayTestFlags : uint32
{
    NONE = 0x0,
    ONLY_VISIBLE = 0x1,

    DEFAULT = ONLY_VISIBLE
};

HYP_MAKE_ENUM_FLAGS(UIRayTestFlags)

/*! \brief Extension to UIStage that adds update manager integration */
class HYP_API UIStageUpdateManager final : public UIUpdateManager
{
public:
    explicit UIStageUpdateManager(UIStage* stage);
    virtual ~UIStageUpdateManager() override = default;

    /*! \brief Process all pending updates for this frame */
    virtual void ProcessUpdates(float delta) override;

    /*! \brief Register an object for selective updating */
    virtual void RegisterForUpdate(UIObject* object, EnumFlags<UIObjectUpdateType> updateTypes) override;

private:
    UIStage* m_stage;
};

/*! \brief The UIStage is the root of the UI scene graph. */

HYP_CLASS()
class HYP_API UIStage : public UIObject
{
    HYP_OBJECT_BODY(UIStage);

public:
    friend class UIObject;

    // The minimum and maximum depth values for the UI scene for layering
    static constexpr int MinDepth = -10000;
    static constexpr int MaxDepth = 10000;

    UIStage();
    UIStage(World* world, ThreadId ownerThreadId);
    UIStage(const UIStage& other) = delete;
    UIStage& operator=(const UIStage& other) = delete;
    virtual ~UIStage() override;

    /*! \brief Get the size of the surface that the UI objects are rendered on.
     *
     *  \return The size of the surface. */
    HYP_METHOD()
    HYP_FORCE_INLINE Vec2i GetSurfaceSize() const
    {
        return m_surfaceSize;
    }

    HYP_METHOD()
    void SetSurfaceSize(Vec2i surfaceSize);

    /*! \brief Get the UI scale factor used to scale UI elements for touch-friendly interfaces.
     *  This is separate from the content scale factor (DPI) and is used to make UI elements larger
     *  on mobile devices or when touch input is needed.
     *
     *  \return The UI scale factor (default is 1.0). */
    HYP_METHOD()
    HYP_FORCE_INLINE float GetUIScaleFactor() const
    {
        return m_uiScaleFactor;
    }

    /*! \brief Set the UI scale factor used to scale UI elements for touch-friendly interfaces.
     *  \param scaleFactor The scale factor to set. Must be greater than 0. */
    HYP_METHOD()
    void SetUIScaleFactor(float scaleFactor);

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

    HYP_FORCE_INLINE World* GetWorld() const
    {
        return m_world;
    }

    void SetWorld(World* world);

    HYP_METHOD()
    const Handle<Camera>& GetCamera() const
    {
        return m_camera;
    }

    /*! \brief Get the default font atlas to use for text rendering.
     *  UIText objects will use this font atlas if they don't have a font atlas set.
     *
     *  \return The default font atlas. */
    const Handle<FontAtlas>& GetDefaultFontAtlas() const;

    /*! \brief Set the default font atlas to use for text rendering.
     *  UIText objects will use this font atlas if they don't have a font atlas set.
     *
     *  \param fontAtlas The font atlas to set. */
    void SetDefaultFontAtlas(const Handle<FontAtlas>& fontAtlas);

    /*! \brief Get the UI object that is currently focused. If no object is focused, returns nullptr.
     *  \return The focused UI object. */
    HYP_FORCE_INLINE const WeakHandle<UIObject>& GetFocusedObject() const
    {
        return m_focusedObject;
    }

    HYP_FORCE_INLINE UIUpdateManager& GetUpdateManager()
    {
        return m_updateManager;
    }

    HYP_FORCE_INLINE const UIUpdateManager& GetUpdateManager() const
    {
        return m_updateManager;
    }

    UIEventHandlerResult OnInputEvent(const Event& event);

    /*! \brief Ray test the UI scene using screen space mouse coordinates */
    bool TestRay(const Vec2f& position, Array<Handle<UIObject>>& outObjects, EnumFlags<UIRayTestFlags> flags = UIRayTestFlags::DEFAULT);

    virtual void AddChildUIObject(const Handle<UIObject>& uiObject) override;

protected:
    virtual void Init() override;

    virtual void Update_Internal(float delta) override;

    virtual void OnAttached_Internal(UIObject* parent) override;

    // Override OnRemoved_Internal to update subobjects to have this as a stage
    virtual void OnRemoved_Internal() override;

    virtual void SetStage_Internal(UIStage* stage) override;

    virtual bool NeedsUpdate() const override
    {
        return m_objectMouseStates.Any() // to update mouse down timers
            || m_keyedDownObjects.Any()
            || UIObject::NeedsUpdate();
    }

private:
    virtual void ComputeActualSize(const UIObjectSize& inSize, Vec2i& outActualSize, UpdateSizePhase phase, bool isInner) override;

    /*! \brief To be called internally from UIObject only */
    void SetFocusedObject(const Handle<UIObject>& uiObject);

    void UpdateCameraControllerStack();

    Handle<UIObject> GetUIObjectForEntity(const Entity* entity) const;

    bool Remove(const Entity* entity);

    Vec2i m_surfaceSize;
    float m_contentScaleFactor = 1.0f;
    float m_uiScaleFactor = 1.0f;

    Vec2f ToLogicalCoords(Vec2f physicalCoords) const
    {
        return Vec2f(physicalCoords) / m_contentScaleFactor;
    }

    World* m_world;

    Handle<Scene> m_scene;
    Handle<Camera> m_camera;

    UIStageUpdateManager m_updateManager;

    Handle<FontAtlas> m_defaultFontAtlas;

    TMap<WeakHandle<UIObject>, UIObjectMouseState> m_objectMouseStates;
    TSet<WeakHandle<UIObject>> m_hoveredUiObjects;
    SparsePagedArray<TMap<WeakHandle<UIObject>, UIObjectKeyState>, 16> m_keyedDownObjects;

    WeakHandle<UIObject> m_focusedObject;

    DelegateHandler m_onCurrentWindowChangedHandler;
    DelegateHandler m_onWindowResizedHandler;
};

} // namespace Hyperion
