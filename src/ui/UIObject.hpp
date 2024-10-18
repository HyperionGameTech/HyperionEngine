/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_OBJECT_HPP
#define HYPERION_UI_OBJECT_HPP

#include <core/Base.hpp>

#include <core/object/HypObject.hpp>

#include <core/containers/Array.hpp>

#include <core/functional/Delegate.hpp>

#include <core/utilities/UniqueID.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/UUID.hpp>

#include <scene/Node.hpp>
#include <scene/NodeProxy.hpp>
#include <scene/Scene.hpp>

#include <scene/ecs/components/UIComponent.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <math/Color.hpp>
#include <math/BlendVar.hpp>
#include <math/MathUtil.hpp>

#include <rendering/FullScreenPass.hpp>

#include <Types.hpp>

namespace hyperion {

struct ScriptComponent;

class UIObject;
class UIStage;
class UIRenderer;
class UIDataSourceBase;
class UIDataSource;

// Helper function to get the scene from a UIStage
template <class UIStageType>
static inline Scene *GetScene(UIStageType *stage)
{
    AssertThrow(stage != nullptr);

    return stage->GetScene().Get();
}

// Used for interop with the C# UIObject class.
// Ensure that the enum values match the C# UIObjectType enum.
enum class UIObjectType : uint32
{
    UNKNOWN             = ~0u,
    STAGE               = 0,
    BUTTON              = 1,
    TEXT                = 2,
    PANEL               = 3,
    IMAGE               = 4,
    TAB_VIEW            = 5,
    TAB                 = 6,
    GRID                = 7,
    GRID_ROW            = 8,
    GRID_COLUMN         = 9,
    MENU_BAR            = 10,
    MENU_ITEM           = 11,
    DOCKABLE_CONTAINER  = 12,
    DOCKABLE_ITEM       = 13,
    LIST_VIEW           = 14,
    LIST_VIEW_ITEM      = 15,
    TEXTBOX             = 16,
    WINDOW              = 17,
};

HYP_MAKE_ENUM_FLAGS(UIObjectType)

enum class UIObjectAlignment : uint32
{
    TOP_LEFT        = 0,
    TOP_RIGHT       = 1,

    CENTER          = 2,

    BOTTOM_LEFT     = 3,
    BOTTOM_RIGHT    = 4
};

enum class UIObjectFocusState : uint32
{
    NONE    = 0x0,
    HOVER   = 0x1,
    PRESSED = 0x2,
    TOGGLED = 0x4,
    FOCUSED = 0x8
};

HYP_MAKE_ENUM_FLAGS(UIObjectFocusState)

enum class UIObjectBorderFlags : uint32
{
    NONE    = 0x0,
    TOP     = 0x1,
    LEFT    = 0x2,
    BOTTOM  = 0x4,
    RIGHT   = 0x8,
    ALL     = TOP | LEFT | BOTTOM | RIGHT
};

HYP_MAKE_ENUM_FLAGS(UIObjectBorderFlags)

enum class UIObjectIterationResult : uint8
{
    CONTINUE = 0,
    STOP
};

enum class UIObjectUpdateType : uint32
{
    NONE                                = 0x0,

    UPDATE_SIZE                         = 0x1,
    UPDATE_POSITION                     = 0x2,
    UPDATE_MATERIAL                     = 0x4,
    UPDATE_MESH_DATA                    = 0x8,
    UPDATE_COMPUTED_VISIBILITY          = 0x10,

    UPDATE_CHILDREN_SIZE                = UPDATE_SIZE << 16,
    UPDATE_CHILDREN_POSITION            = UPDATE_POSITION << 16,
    UPDATE_CHILDREN_MATERIAL            = UPDATE_MATERIAL << 16,
    UPDATE_CHILDREN_MESH_DATA           = UPDATE_MESH_DATA << 16,
    UPDATE_CHILDREN_COMPUTED_VISIBILITY = UPDATE_COMPUTED_VISIBILITY << 16,
};

HYP_MAKE_ENUM_FLAGS(UIObjectUpdateType)

struct UIObjectAspectRatio
{
    float x = 1.0f;
    float y = 1.0f;

    UIObjectAspectRatio()                                                   = default;

    UIObjectAspectRatio(float ratio)
        : x(ratio),
          y(1.0f / ratio)
    {
    }

    UIObjectAspectRatio(float x, float y)
        : x(x),
          y(y)
    {
    }

    UIObjectAspectRatio(const UIObjectAspectRatio &other)                   = default;
    UIObjectAspectRatio &operator=(const UIObjectAspectRatio &other)        = default;
    UIObjectAspectRatio(UIObjectAspectRatio &&other) noexcept               = default;
    UIObjectAspectRatio &operator=(UIObjectAspectRatio &&other) noexcept    = default;

    HYP_FORCE_INLINE bool IsValid() const
        { return float(*this) == float(*this); }

    HYP_FORCE_INLINE explicit operator float() const
        { return x / y; }
};

#pragma region UIObjectSize

HYP_STRUCT()
struct UIObjectSize
{
    enum Flags
    {
        AUTO    = 0x04,

        PIXEL   = 0x10,
        PERCENT = 0x20,

        FILL    = 0x40,

        DEFAULT = PIXEL
    };

    HYP_FIELD()
    uint32  flags[2];

    HYP_FIELD()
    Vec2i   value;

    UIObjectSize()
        : flags { DEFAULT, DEFAULT },
          value(0, 0)
    {
    }

    explicit UIObjectSize(Vec2i value)
        : flags { DEFAULT, DEFAULT },
          value(value)
    {
    }

    UIObjectSize(Vec2i value, uint32 flags)
        : flags { flags, flags },
          value(value)
    {
        ApplyDefaultFlags();
    }

    /*! \brief Construct by only providing flags. Used primarily for the DYNAMIC type. */
    UIObjectSize(uint32 flags)
        : flags { flags, flags },
          value(0, 0)
    {
        ApplyDefaultFlags();
    }

    /*! \brief Construct by providing specific flags for each axis. */
    UIObjectSize(Pair<int, uint32> x, Pair<int, uint32> y)
        : flags { x.second, y.second },
          value(x.first, y.first)
    {
        ApplyDefaultFlags();
    }

    UIObjectSize(const UIObjectSize &other)                 = default;
    UIObjectSize &operator=(const UIObjectSize &other)      = default;
    UIObjectSize(UIObjectSize &&other) noexcept             = default;
    UIObjectSize &operator=(UIObjectSize &&other) noexcept  = default;
    ~UIObjectSize()                                         = default;

    HYP_FORCE_INLINE const Vec2i &GetValue() const
        { return value; }

    HYP_FORCE_INLINE uint32 GetFlagsX() const
        { return flags[0]; }

    HYP_FORCE_INLINE uint32 GetFlagsY() const
        { return flags[1]; }

    HYP_FORCE_INLINE uint32 GetAllFlags() const
        { return flags[0] | flags[1]; }

    template <uint32 Mask>
    HYP_FORCE_INLINE void ApplyDefaultFlagMask()
    { 
        for (int i = 0; i < 2; i++) {
            if (!(flags[i] & Mask)) {
                flags[i] |= (DEFAULT & Mask);
            }
        }
    }

    HYP_FORCE_INLINE void ApplyDefaultFlags()
    {
        ApplyDefaultFlagMask<PIXEL | PERCENT | FILL>();
    }
};

static_assert(sizeof(UIObjectSize) == 16, "sizeof(UIObjectSize) must be 16 bytes to match C# struct size");

enum class UIObjectUpdateSizeFlags : uint32
{
    NONE                = 0x0,

    MAX_SIZE            = 0x1,
    INNER_SIZE          = 0x2,
    OUTER_SIZE          = 0x4,

    CLAMP_OUTER_SIZE    = 0x8,

    DEFAULT             = MAX_SIZE | INNER_SIZE | OUTER_SIZE | CLAMP_OUTER_SIZE
};

HYP_MAKE_ENUM_FLAGS(UIObjectUpdateSizeFlags)

#pragma endregion UIObjectSize

#pragma region UIObjectQuadMeshHelper

class HYP_API UIObjectQuadMeshHelper
{
public:
    static const Handle<Mesh> &GetQuadMesh();
};

#pragma endregion UIObjectQuadMeshHelper

#pragma region UIObjectRenderProxy


#pragma endregion UIObjectRenderProxy

#pragma region UIObject

struct UIObjectID : UniqueID { };

HYP_CLASS(Abstract)
class HYP_API UIObject : public EnableRefCountedPtrFromThis<UIObject>
{
    HYP_OBJECT_BODY(UIObject);

protected:
    enum class UpdateSizePhase
    {
        BEFORE_CHILDREN,
        AFTER_CHILDREN
    };

    UIObject(UIObjectType type);

public:
    friend class UIRenderer;
    friend class UIStage;

    UIObject(UIStage *stage, NodeProxy node_proxy, UIObjectType type);
    UIObject(const UIObject &other)                 = delete;
    UIObject &operator=(const UIObject &other)      = delete;
    UIObject(UIObject &&other) noexcept             = delete;
    UIObject &operator=(UIObject &&other) noexcept  = delete;
    virtual ~UIObject();

    virtual void Init();
    virtual void Update(GameCounter::TickUnit delta) final;

    HYP_FORCE_INLINE const UIObjectID &GetID() const
        { return m_id; }

    HYP_METHOD()
    HYP_FORCE_INLINE UIObjectType GetType() const
        { return m_type; }

    HYP_METHOD()
    HYP_FORCE_INLINE ID<Entity> GetEntity() const
        { return m_node_proxy.IsValid() ? m_node_proxy->GetEntity() : ID<Entity>::invalid; }

    HYP_METHOD()
    HYP_FORCE_INLINE UIStage *GetStage() const
        { return m_stage; }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsInit() const
        { return m_is_init; }

    HYP_METHOD()
    Name GetName() const;
    
    HYP_METHOD()
    void SetName(Name name);

    HYP_METHOD()
    Vec2i GetPosition() const;

    HYP_METHOD()
    void SetPosition(Vec2i position);

    HYP_METHOD()
    Vec2f GetOffsetPosition() const;

    HYP_METHOD()
    Vec2f GetAbsolutePosition() const;

    HYP_METHOD()
    UIObjectSize GetSize() const;

    HYP_METHOD()
    void SetSize(UIObjectSize size);

    HYP_METHOD()
    UIObjectSize GetInnerSize() const;

    HYP_METHOD()
    void SetInnerSize(UIObjectSize size);

    HYP_METHOD()
    UIObjectSize GetMaxSize() const;

    HYP_METHOD()
    void SetMaxSize(UIObjectSize size);
    
    /*! \brief Get the computed size (in pixels) of the UI object.
     *  The actual size of the UI object is calculated based on the size of the parent object and the size of the object itself.
     *  \return The computed size of the UI object */
    HYP_METHOD()
    HYP_FORCE_INLINE Vec2i GetActualSize() const
        { return m_actual_size; }

    /*! \brief Get the computed inner size (in pixels) of the UI object.
     *  \return The computed inner size of the UI object */
    HYP_METHOD()
    HYP_FORCE_INLINE Vec2i GetActualInnerSize() const
        { return m_actual_inner_size; }

    /*! \brief Get the scroll offset (in pixels) of the UI object.
     *  \return The scroll offset of the UI object */
    HYP_METHOD()
    Vec2i GetScrollOffset() const;

    /*! \brief Set the scroll offset (in pixels) of the UI object.
     *  \param scroll_offset The scroll offset of the UI object
     *  \param smooth Whether or not to interpolate the scroll offset to the given value. If false, it will immediately move to the given value. */
    HYP_METHOD()
    void SetScrollOffset(Vec2i scroll_offset, bool smooth);

    /*! \brief Get the depth of the UI object, or the computed depth from the Node  if none has been explicitly set.
     *  \see{Node::CalculateDepth}
     *  \return The depth of the UI object */
    HYP_METHOD()
    int GetComputedDepth() const;

    /*! \brief Get the depth of the UI object
     *  The depth of the UI object is used to determine the rendering order of the object in the scene relative to its sibling elements, with higher depth values being rendered on top of lower depth values.
     *  If the depth value is set to 0, the depth will be determined by the node's depth in the scene.
     *  \return The depth of the UI object */
    HYP_METHOD()
    int GetDepth() const;

    /*! \brief Set the depth of the UI object
     *  The depth of the UI object is used to determine the rendering order of the object in the scene relative to its sibling elements, with higher depth values being rendered on top of lower depth values.
     *  Set the depth to a value between UIStage::min_depth and UIStage::max_depth. If the depth value is set to 0, the depth will be determined by the node's depth in the scene.
     *  \param depth The depth of the UI object */
    HYP_METHOD()
    void SetDepth(int depth);

    /*! \brief Check if the UI object accepts focus. All UIObjects accept focus by default, unless overridden by derived classes or set using \ref{SetAcceptsFocus}.
     *  \return True if the this object accepts focus, false otherwise */
    HYP_METHOD()
    virtual bool AcceptsFocus() const
        { return m_accepts_focus; }

    /*! \brief Set whether the UI object accepts focus.
     *  \details If set to true, the UI object can receive focus. If set to false, the UI object cannot receive focus.
     *  \note If a class deriving \ref{UIObject} overrides \ref{AcceptsFocus}, this function has no effect. */
    HYP_METHOD()
    void SetAcceptsFocus(bool accepts_focus);

    /*! \brief Set the focus to this UI object, if AcceptsFocus() returns true.
     * This function is called when the UI object is focused. */
    HYP_METHOD()
    virtual void Focus();

    /*! \brief Remove the focus from this UI object, if AcceptsFocus() returns true.
     *  This function is called when the UI object loses focus.
     *  \param blur_children If true, also remove focus from all child objects. */
    HYP_METHOD()
    virtual void Blur(bool blur_children = true);

    /*! \brief Set whether the UI object affects the size of its parent.
     *  \details If true, the size of the parent object will be include the size of this object when calculating the parent's size.
     *  \param affects_parent_size Whether the UI object affects the size of its parent */
    HYP_METHOD()
    void SetAffectsParentSize(bool affects_parent_size);

    /*! \brief Check if the UI object affects the size of its parent.
     *  \details If true, the size of the parent object will be include the size of this object when calculating the parent's size.
     *  \return True if the UI object affects the size of its parent, false otherwise */
    HYP_FORCE_INLINE bool AffectsParentSize() const
        { return m_affects_parent_size; }

    /*! \brief Returns whether or not this UIObject type acts as a container for other objects. 
     *  Container types can have objects that sit outside and move independently of this object itself,
     *  and is useful for things like tab views, panels that scroll, etc. However, it comes with the added cost
     *  of more bookkeeping and more RenderGroup creation for rendering. See \ref{CollectObjects} and \ref{UIRenderer::OnUpdate} for example. */
    virtual bool IsContainer() const
        { return false; }

    /*! \brief Get the border radius of the UI object
     *  \details The border radius of the UI object is used to create rounded corners for the object's border.
     *  \return The border radius of the UI object */
    HYP_FORCE_INLINE uint32 GetBorderRadius() const
        { return m_border_radius; }

    /*! \brief Set the border radius of the UI object
     *  \details The border radius of the UI object is used to create rounded corners for the object's border.
     *  \param border_radius The border radius of the UI object */
    void SetBorderRadius(uint32 border_radius);

    /*! \brief Get the border flags of the UI object
     *  \details The border flags of the UI object are used to determine which borders of the object should be rounded, if the border radius is set to a non-zero value.
     *  \example To display a border radius the top left and right corners of the object, set the border flags to \code{UOB_TOP | UOB_LEFT | UOB_RIGHT}.
     *  \return The border flags of the UI object */
    HYP_FORCE_INLINE EnumFlags<UIObjectBorderFlags> GetBorderFlags() const
        { return m_border_flags; }

    void SetBorderFlags(EnumFlags<UIObjectBorderFlags> border_flags);

    UIObjectAlignment GetOriginAlignment() const;

    void SetOriginAlignment(UIObjectAlignment alignment);

    UIObjectAlignment GetParentAlignment() const;

    void SetParentAlignment(UIObjectAlignment alignment);

    HYP_FORCE_INLINE UIObjectAspectRatio GetAspectRatio() const
        { return m_aspect_ratio; }

    void SetAspectRatio(UIObjectAspectRatio aspect_ratio);

    /*! \brief Get the padding of the UI object
     *  The padding of the UI object is used to add space around the object's content.
     *  \return The padding of the UI object */
    HYP_METHOD(Property="Padding")
    HYP_FORCE_INLINE Vec2i GetPadding() const
        { return m_padding; }

    /*! \brief Set the padding of the UI object
     *  The padding of the UI object is used to add space around the object's content.
     *  \param padding The padding of the UI object */
    HYP_METHOD(Property="Padding")
    void SetPadding(Vec2i padding);

    /*! \brief Get the background color of the UI object
     * \return The background color of the UI object */
    HYP_METHOD(Property="BackgroundColor")
    HYP_FORCE_INLINE Color GetBackgroundColor() const
        { return m_background_color; }

    /*! \brief Set the background color of the UI object
     *  \param background_color The background color of the UI object */
    HYP_METHOD(Property="BackgroundColor")
    void SetBackgroundColor(const Color &background_color);

    /*! \brief Get the text color of the UI object
     * \return The text color of the UI object */
    HYP_METHOD(Property="TextColor")
    Color GetTextColor() const;

    /*! \brief Set the text color of the UI object
     *  \param text_color The text color of the UI object */
    HYP_METHOD(Property="TextColor")
    void SetTextColor(const Color &text_color);

    /*! \brief Gets the text to render.
     * 
     * \return The text to render. */
    HYP_METHOD()
    HYP_FORCE_INLINE const String &GetText() const
        { return m_text; }

    HYP_METHOD()
    virtual void SetText(const String &text);

    HYP_METHOD()
    float GetTextSize() const;

    HYP_METHOD()
    void SetTextSize(float text_size); 

    /*! \brief Check if the UI object is set to visible or not. This does not include computed visibility.
     *  \returns True if the object is visible, false otherwise. */
    bool IsVisible() const;

    /*! \brief Set the visibility of the UI object.
     *  \details The visibility of the UI object determines whether the object is rendered in the UI scene.
     *  Can be used to hide the object without removing it from the scene.
     *  \param is_visible Whether to set the object as visible or not. */
    void SetIsVisible(bool is_visible);

    /*! \brief Get the computed visibility of the UI object.
     *  \details The computed visibility of the UI object is used to determine if the object is currently visible.
     *  \return The computed visibility of the UI object. */
    HYP_FORCE_INLINE bool GetComputedVisibility() const
        { return m_computed_visibility; }

    /*! \brief Check if the UI object has focus. If \ref{include_children} is true, also return true if any child objects have focus.
     *  \details The focus state of the UI object is used to determine if the object is currently focused.
     *  \param include_children If true, check if any child objects have focus.
     *  \return True if the object has focus, false otherwise. */ 
    bool HasFocus(bool include_children = true) const;

    /*! \brief Check if \ref{other} is either a parent of this object or is equal to the current object.
     *  \details Comparison is performed by using \ref{Node::IsOrHasParent}. If either this or \ref{other} does not have a Node,
     *  false is returned.
     *  \param other The UIObject to check if it is a parent of this object.
     *  \return Whether \ref{other} is a parent of this object or equal to the current object.
     */ 
    bool IsOrHasParent(const UIObject *other) const;

    /*! \brief Get the parent UIObject to this object, if one exists.
     *  \returns A pointer to the parent UIObject or nullptr if none exists. */ 
    RC<UIObject> GetParentUIObject() const;

    /*! \brief Get the closest parent UIObject with UIObjectType \ref{type} if one exists.
     *  \returns A pointer to the closest parent UIObject with UIObjectType \ref{type} or nullptr if none exists. */ 
    RC<UIObject> GetClosestParentUIObject(UIObjectType type) const;

    template <class T>
    RC<T> GetClosestParentUIObject() const
    {
        static_assert(std::is_base_of_v<UIObject, T>, "T must be a subclass of UIObject");

        return GetClosestParentUIObjectWithPredicate([](const RC<UIObject> &parent) -> bool
        {
            return parent.Is<T>();
        }).template CastUnsafe<T>();
    }

    virtual void AddChildUIObject(UIObject *ui_object);
    virtual bool RemoveChildUIObject(UIObject *ui_object);

    /*! \brief Remove all child UIObjects from this object.
     *  \returns The number of child UIObjects removed. */
    virtual int RemoveAllChildUIObjects();

    /*! \brief Remove all child UIObjects from this object that match the predicate.
     *  \param predicate The predicate to match against the child UIObjects.
     *  \returns The number of child UIObjects removed. */
    virtual int RemoveAllChildUIObjects(ProcRef<bool, const RC<UIObject> &> predicate);

    /*! \brief Remove this object from its parent UI object, if applicable.
     *  \note It is possible that you are removing the last strong reference to `this` by calling this method,
     *  invalidating the pointer. Proper care must be taken to ensure `this` is not reused after calling this method.
     *  If you need to use the object again, use \ref{DetachFromParent} which returns a strong reference counted pointer to `this`,
     *  ensuring it does not get instantly deleted.
     *  \returns A boolean indicating whether or not the object could be removed from its parent */
    virtual bool RemoveFromParent();

    /*! \brief Remove this object from its parent UI object, if applicable. Ensures the object is not immediately deleted
     *  in the case that the parent UIObject holds the last reference to `this`.
     *  \returns A reference counted pointer to `this`. */
    virtual RC<UIObject> DetachFromParent();

    /*! \brief Find a child UIObject by its Name. Checks descendents recursively. If multiple children have the same Name, the first one found is returned.
     *  If no child UIObject with the specified Name is found, nullptr is returned.
     *  \param name The Name of the child UIObject to find.
     *  \param deep If true, search all descendents. If false, only search immediate children.
     *  \return The child UIObject with the specified Name, or nullptr if no child UIObject with the specified Name was found. */
    RC<UIObject> FindChildUIObject(WeakName name, bool deep = true) const;

    /*! \brief Find a child UIObject by predicate. Checks descendents using breadth-first search. If multiple children match the predicate, the first one found is returned.
     *  If no child UIObject matches the predicate, nullptr is returned.
     *  \param predicate The predicate to match against the child UIObjects.
     *  \param deep If true, search all descendents. If false, only search immediate children.
     *  \return The child UIObject that matches the predicate, or nullptr if no child UIObject matches the predicate. */
    RC<UIObject> FindChildUIObject(ProcRef<bool, const RC<UIObject> &> predicate, bool deep = true) const;

    /*! \brief Check if the UI object has any child UIObjects.
     *  \return True if the object has child UIObjects, false otherwise. */ 
    bool HasChildUIObjects() const;

    /*! \brief Gets the child UIObject at the specified index.
     *  \param index The index of the child UIObject to get.
     *  \return The child UIObject at the specified index. */
    const RC<UIObject> &GetChildUIObject(int index) const;

    /*! \brief Gets the relevant script component for this UIObject, if one exists.
     *  The script component is the closest script component to this UIObject in the scene hierarchy, starting from the parent and moving up.
     *  \param deep If set to true, will find the closest parent ScriptComponent if none is attached to this UIObject.
     *  \return A pointer to a ScriptComponent for this UIObject or any of its parents, or nullptr if none exists. */
    ScriptComponent *GetScriptComponent(bool deep = false) const;

    /*! \brief Sets the script component for this UIObject.
     *  Adds the ScriptComponent directly to the UIObject. If the UIObject already has a ScriptComponent, it will be replaced.
     *  \param script_component A ScriptComponent, passed as an rvalue reference. */
    void SetScriptComponent(ScriptComponent &&script_component);

    /*! \brief Removes the script component from this UIObject.
     *  If the UIObject has a script component, it will be removed. Only the script component directly attached to the UIObject will be removed.
     *  Subsequent calls to \ref{GetScriptComponent} will return the closest script component to this UIObject in the scene hierarchy, if one exists. */
    void RemoveScriptComponent();

    HYP_METHOD()
    const NodeProxy &GetNode() const;

    virtual Scene *GetScene() const;

    const Handle<Material> &GetMaterial() const;

    BoundingBox GetWorldAABB() const;
    BoundingBox GetLocalAABB() const;

    const NodeTag &GetNodeTag(Name key) const;
    void SetNodeTag(Name key, const NodeTag &tag);
    bool HasNodeTag(Name key) const;
    bool RemoveNodeTag(Name key);

    virtual void UpdatePosition(bool update_children = true);
    virtual void UpdateSize(bool update_children = true);

    /*! \brief Set deferred updates to apply to the UI object.
     *  \details Deferred updates are used to defer updates to the UI object until the next Update() call.
     *  \param update_type The type of update to apply.
     *  \param update_children If true, also apply the update on all child UIObjects when the deferred update is processed. */
    void SetDeferredUpdate(EnumFlags<UIObjectUpdateType> update_type, bool update_children = true)
    {
        if (update_children) {
            update_type |= update_type << 16;
        }

        m_deferred_updates |= update_type;
    }

    void SetUpdatesLocked(EnumFlags<UIObjectUpdateType> update_type, bool locked)
    {
        if (locked) {
            m_locked_updates |= update_type;
        } else {
            m_locked_updates &= ~update_type;
        }
    }

    /*! \brief Get the focus state of the UI object.
     *  \details The focus state of the UI object is used to determine if the object is currently focused, hovered, pressed, etc.
     *  \return The focus state of the UI object. */
    HYP_FORCE_INLINE EnumFlags<UIObjectFocusState> GetFocusState() const
        { return m_focus_state; }

    /*! \brief Set the focus state of the UI object.
     *  \details The focus state of the UI object is used to determine if the object is currently focused, hovered, pressed, etc.
     *  \param focus_state The focus state of the UI object. */
    void SetFocusState(EnumFlags<UIObjectFocusState> focus_state);

    /*! \brief Collect all nested UIObjects in the hierarchy, calling `proc` for each collected UIObject.
     *  \param proc The function to call for each collected UIObject.
     *  \param only_visible If true, skips objects with computed visibility as non-visible */
    void CollectObjects(ProcRef<void, UIObject *> proc, bool only_visible = true) const;

    /*! \brief Collect all nested UIObjects in the hierarchy and push them to the `out_objects` array.
     *  \param out_objects The array to store the collected UIObjects in.
     *  \param only_visible If true, skips objects with computed visibility as non-visible */
    void CollectObjects(Array<UIObject *> &out_objects, bool only_visible = true) const;

    /*! \brief Transform a screen coordinate to a relative coordinate within the UIObject.
     *  \param coords The screen coordinates to transform.
     *  \return The relative coordinates within the UIObject. */
    Vec2f TransformScreenCoordsToRelative(Vec2i coords) const;

    /*! \brief Get the data source associated with this UIObject. The data source is used to populate the UIObject with data.
     *  \return The data source associated with this UIObject. */
    HYP_METHOD(Property="DataSource")
    HYP_FORCE_INLINE const RC<UIDataSourceBase> &GetDataSource() const
        { return m_data_source; }

    /*! \brief Set the data source associated with this UIObject. The data source is used to populate the UIObject with data.
     *  \param data_source The data source to associate with this UIObject. */
    HYP_METHOD(Property="DataSource")
    void SetDataSource(const RC<UIDataSourceBase> &data_source);

    /*! \brief Gets the UUID of the associated data source element (if applicable).
     *  Otherwise, returns an empty UUID.
     * 
     * \return The UUID of the associated data source element. */
    HYP_FORCE_INLINE UUID GetDataSourceElementUUID() const
        { return m_data_source_element_uuid; }

    /*! \brief Sets the UUID of the associated data source element.
     *  \internal This is used by a parent UIObject to set the UUID of the associated data source element.
     * 
     *  \param data_source_element_uuid The UUID of the associated data source element. */
    HYP_FORCE_INLINE void SetDataSourceElementUUID(UUID data_source_element_uuid)
        { m_data_source_element_uuid = data_source_element_uuid; }

    /*! \internal */
    void ForEachChildUIObject_Proc(ProcRef<UIObjectIterationResult, const RC<UIObject> &> proc, bool deep = true) const;

    // Events
    Delegate<UIEventHandlerResult>                          OnInit;
    Delegate<UIEventHandlerResult>                          OnAttached;
    Delegate<UIEventHandlerResult>                          OnRemoved;
    Delegate<UIEventHandlerResult, const MouseEvent &>      OnMouseDown;
    Delegate<UIEventHandlerResult, const MouseEvent &>      OnMouseUp;
    Delegate<UIEventHandlerResult, const MouseEvent &>      OnMouseDrag;
    Delegate<UIEventHandlerResult, const MouseEvent &>      OnMouseHover;
    Delegate<UIEventHandlerResult, const MouseEvent &>      OnMouseLeave;
    Delegate<UIEventHandlerResult, const MouseEvent &>      OnMouseMove;
    Delegate<UIEventHandlerResult, const MouseEvent &>      OnGainFocus;
    Delegate<UIEventHandlerResult, const MouseEvent &>      OnLoseFocus;
    Delegate<UIEventHandlerResult, const MouseEvent &>      OnScroll;
    Delegate<UIEventHandlerResult, const MouseEvent &>      OnClick;
    Delegate<UIEventHandlerResult, const KeyboardEvent &>   OnKeyDown;
    Delegate<UIEventHandlerResult, const KeyboardEvent &>   OnKeyUp;

protected:
    RC<UIObject> GetClosestParentUIObjectWithPredicate(const ProcRef<bool, const RC<UIObject> &> &predicate) const;

    HYP_FORCE_INLINE bool UseAutoSizing() const
    {
        return (GetSize().GetAllFlags() | GetInnerSize().GetAllFlags() | GetMaxSize().GetAllFlags()) & UIObjectSize::AUTO;
    }

    virtual void UpdateSize_Internal(bool update_children);

    virtual void SetDataSource_Internal(UIDataSourceBase *data_source);

    virtual void SetFocusState_Internal(EnumFlags<UIObjectFocusState> focus_state);

    virtual void Update_Internal(GameCounter::TickUnit delta);

    virtual void OnAttached_Internal(UIObject *parent);
    virtual void OnRemoved_Internal();

    /*! \brief Check if the object has been computed as visible or not. E.g scrolled out of view in a parent container */
    virtual void UpdateComputedVisibility(bool update_children = true);

    virtual void OnComputedVisibilityChange_Internal() { }

    /*! \brief Sets the NodeProxy for this UIObject.
     *  \note To be called internally from UIStage */
    void SetNodeProxy(NodeProxy);

    /*! \brief Get the shared quad mesh used for rendering UI objects. Vertices are in range: 0..1, with the origin at the top-left corner.
     *
     * \return The shared quad mesh
     */
    static Handle<Mesh> GetQuadMesh();

    /*! \brief Calculate the local (object space) bounding box (in pixels) of this object.
     *  should be in range of (0,0,0):(size,size,size). */
    BoundingBox CalculateAABB() const;

    virtual BoundingBox CalculateInnerAABB_Internal() const;

    const Handle<Mesh> &GetMesh() const;

    /*! \brief Does this object allow the material to be updated?
     *  If true, a dynamic material will be created for this object. */
    virtual bool AllowMaterialUpdate() const
        { return false; }

    virtual MaterialAttributes GetMaterialAttributes() const;
    virtual Material::ParameterTable GetMaterialParameters() const;
    virtual Material::TextureSet GetMaterialTextures() const;

    void SetAABB(const BoundingBox &aabb);

    Vec2i GetParentScrollOffset() const;

    /*! \brief Add a DelegateHandler to be removed when the object is destructed */
    void AddDelegateHandler(DelegateHandler &&delegate_handler)
        { m_delegate_handlers.PushBack(std::move(delegate_handler)); }

    void UpdateMeshData(bool update_children = true);
    void UpdateMaterial(bool update_children = true);

    Array<RC<UIObject>> GetChildUIObjects(bool deep) const;
    Array<RC<UIObject>> FilterChildUIObjects(ProcRef<bool, const RC<UIObject> &> predicate, bool deep) const;

    virtual void SetStage_Internal(UIStage *stage);

    void OnFontAtlasUpdate();
    virtual void OnFontAtlasUpdate_Internal() { }

    void OnTextSizeUpdate();
    virtual void OnTextSizeUpdate_Internal() { }

    bool NeedsRepaint() const;
    void SetNeedsRepaintFlag(bool needs_repaint = true);

    void Repaint();
    virtual bool Repaint_Internal();

    UIStage                         *m_stage;

    Name                            m_name;

    Vec2i                           m_position;
    Vec2f                           m_offset_position;

    UIObjectSize                    m_size;
    Vec2i                           m_actual_size;

    UIObjectSize                    m_inner_size;
    Vec2i                           m_actual_inner_size;

    UIObjectSize                    m_max_size;
    Vec2i                           m_actual_max_size;

    BoundingBox                     m_aabb;

    BlendVar<Vec2f>                 m_scroll_offset;

    Vec2i                           m_padding;

    int                             m_depth; // manually set depth; otherwise defaults to the node's depth in the scene

    uint32                          m_border_radius;
    EnumFlags<UIObjectBorderFlags>  m_border_flags;

    UIObjectAlignment               m_origin_alignment;
    UIObjectAlignment               m_parent_alignment;

    UIObjectAspectRatio             m_aspect_ratio;

    Color                           m_background_color;
    Color                           m_text_color;

    String                          m_text;

    float                           m_text_size;

    RC<UIDataSourceBase>            m_data_source;
    DelegateHandler                 m_data_source_on_change_handler;
    DelegateHandler                 m_data_source_on_element_add_handler;
    DelegateHandler                 m_data_source_on_element_remove_handler;
    DelegateHandler                 m_data_source_on_element_update_handler;

    UUID                            m_data_source_element_uuid;

private:
    /*! \brief Collect all nested UIObjects in the hierarchy, calling `proc` for each collected UIObject.
     *  \param proc The function to call for each collected UIObject.
     *  \param out_deferred_child_objects Array to push child objects to, in the case that its parent type is not a container type (IsContainer() returns false)
     *  \param only_visible If true, skips objects with computed visibility as non-visible */
    void CollectObjects(ProcRef<void, UIObject *> proc, Array<UIObject *> &out_deferred_child_objects, bool only_visible = true) const;

    void ComputeOffsetPosition();

    void UpdateActualSizes(UpdateSizePhase phase, EnumFlags<UIObjectUpdateSizeFlags> flags);
    virtual void ComputeActualSize(const UIObjectSize &size, Vec2i &actual_size, UpdateSizePhase phase, bool is_inner = false);

    template <class Lambda>
    void ForEachChildUIObject(Lambda &&lambda, bool deep = true) const;

    template <class Lambda>
    void ForEachParentUIObject(Lambda &&lambda) const;

    // For UIStage only.
    void SetAllChildUIObjectsStage(UIStage *stage);

    void SetEntityAABB(const BoundingBox &aabb);

    Handle<Material> CreateMaterial() const;

    const UIObjectID                m_id;
    const UIObjectType              m_type;

    bool                            m_is_init;

    EnumFlags<UIObjectFocusState>   m_focus_state;

    bool                            m_is_visible;
    bool                            m_computed_visibility;
    
    bool                            m_accepts_focus;

    bool                            m_affects_parent_size;

    AtomicVar<bool>                 m_needs_repaint;

    EnumFlags<UIObjectUpdateType>   m_deferred_updates;
    EnumFlags<UIObjectUpdateType>   m_locked_updates;

    NodeProxy                       m_node_proxy;

    Array<DelegateHandler>          m_delegate_handlers;
};

#pragma endregion UIObject

} // namespace hyperion

#endif