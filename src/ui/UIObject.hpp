/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_OBJECT_HPP
#define HYPERION_UI_OBJECT_HPP

#include <core/Base.hpp>

#include <core/object/HypObject.hpp>

#include <core/containers/Array.hpp>

#include <core/functional/ScriptableDelegate.hpp>

#include <core/utilities/UniqueID.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/UUID.hpp>

#include <core/utilities/ForEach.hpp>

#include <scene/Node.hpp>
#include <scene/NodeProxy.hpp>
#include <scene/Scene.hpp>
#include <scene/Material.hpp>

#include <core/math/Color.hpp>
#include <core/math/BlendVar.hpp>

#include <Types.hpp>

namespace hyperion {

struct ScriptComponent;

class UIObject;
class UIStage;
class UIRenderSubsystem;
class UIDataSourceBase;
class UIDataSource;

// Helper function to get the scene from a UIStage
template <class UIStageType>
static inline Scene *GetScene(UIStageType *stage)
{
    AssertThrow(stage != nullptr);

    return stage->GetScene().Get();
}

struct UIObjectInstanceData
{
    Matrix4 transform;
    Vec4f   texcoords;
};

HYP_ENUM()
enum class UIObjectType : uint32
{
    UNKNOWN             = ~0u,
    OBJECT              = 0,
    STAGE               = 1,
    BUTTON              = 2,
    TEXT                = 3,
    PANEL               = 4,
    IMAGE               = 5,
    TAB_VIEW            = 6,
    TAB                 = 7,
    GRID                = 8,
    GRID_ROW            = 9,
    GRID_COLUMN         = 10,
    MENU_BAR            = 11,
    MENU_ITEM           = 12,
    DOCKABLE_CONTAINER  = 13,
    DOCKABLE_ITEM       = 14,
    LIST_VIEW           = 15,
    LIST_VIEW_ITEM      = 16,
    TEXTBOX             = 17,
    WINDOW              = 18,
};

HYP_MAKE_ENUM_FLAGS(UIObjectType)

HYP_STRUCT(Size=24)
struct alignas(8) UIEventHandlerResult
{
    enum Value : uint32
    {
        ERR             = 0x1u << 31u,
        OK              = 0x0,

        // Stop bubbling the event up the hierarchy
        STOP_BUBBLING   = 0x1
    };

    UIEventHandlerResult()
        : value(OK),
          message(nullptr),
          function_name(nullptr)
    {
    }

    UIEventHandlerResult(uint32 value)
        : value(value),
          message(nullptr),
          function_name(nullptr)
    {
    }

    UIEventHandlerResult(uint32 value, const StaticMessage &message)
        : value(value),
          message(&message),
          function_name(nullptr)
    {
    }

    UIEventHandlerResult(uint32 value, const StaticMessage &message, const StaticMessage &function_name)
        : value(value),
          message(&message),
          function_name(&function_name)
    {
    }

    UIEventHandlerResult &operator=(uint32 value)
    {
        this->value = value;
        
        message = nullptr;
        function_name = nullptr;

        return *this;
    }

    UIEventHandlerResult(const UIEventHandlerResult &other)                 = default;
    UIEventHandlerResult &operator=(const UIEventHandlerResult &other)      = default;

    UIEventHandlerResult(UIEventHandlerResult &&other) noexcept             = default;
    UIEventHandlerResult &operator=(UIEventHandlerResult &&other) noexcept  = default;

    ~UIEventHandlerResult()                                                 = default;

    HYP_FORCE_INLINE explicit operator bool() const
        { return value != 0; }

    HYP_FORCE_INLINE bool operator!() const
        { return !value; }

    HYP_FORCE_INLINE explicit operator uint32() const
        { return value; }

    HYP_FORCE_INLINE bool operator==(const UIEventHandlerResult &other) const
        { return value == other.value; }

    HYP_FORCE_INLINE bool operator!=(const UIEventHandlerResult &other) const
        { return value != other.value; }

    HYP_FORCE_INLINE UIEventHandlerResult operator&(const UIEventHandlerResult &other) const
        { return UIEventHandlerResult(value & other.value); }

    HYP_FORCE_INLINE UIEventHandlerResult &operator&=(const UIEventHandlerResult &other)
    {
        value &= other.value;

        return *this;
    }

    HYP_FORCE_INLINE UIEventHandlerResult operator|(const UIEventHandlerResult &other) const
        { return UIEventHandlerResult(value | other.value); }

    HYP_FORCE_INLINE UIEventHandlerResult &operator|=(const UIEventHandlerResult &other)
    {
        value |= other.value;

        return *this;
    }

    HYP_FORCE_INLINE UIEventHandlerResult operator~() const
        { return UIEventHandlerResult(~value); }

    HYP_FORCE_INLINE Optional<ANSIStringView> GetMessage() const
    {
        if (!message) {
            return { };
        }

        return message->value;
    }

    HYP_FORCE_INLINE Optional<ANSIStringView> GetFunctionName() const
    {
        if (!function_name) {
            return { };
        }

        return function_name->value;
    }

    uint32              value;
    const StaticMessage *message;
    const StaticMessage *function_name;
};

HYP_ENUM()
enum class UIObjectAlignment : uint32
{
    TOP_LEFT        = 0,
    TOP_RIGHT       = 1,

    CENTER          = 2,

    BOTTOM_LEFT     = 3,
    BOTTOM_RIGHT    = 4
};

HYP_ENUM()
enum class UIObjectFocusState : uint32
{
    NONE    = 0x0,
    HOVER   = 0x1,
    PRESSED = 0x2,
    TOGGLED = 0x4,
    FOCUSED = 0x8
};

HYP_MAKE_ENUM_FLAGS(UIObjectFocusState)

HYP_ENUM()
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

HYP_ENUM()
enum class UIObjectUpdateType : uint32
{
    NONE                                = 0x0,

    UPDATE_SIZE                         = 0x1,
    UPDATE_POSITION                     = 0x2,
    UPDATE_MATERIAL                     = 0x4,
    UPDATE_MESH_DATA                    = 0x8,
    UPDATE_COMPUTED_VISIBILITY          = 0x10,
    UPDATE_CLAMPED_SIZE                 = 0x20,
    UPDATE_TEXT_RENDER_DATA             = 0x40,

    UPDATE_CHILDREN_SIZE                = UPDATE_SIZE << 16,
    UPDATE_CHILDREN_POSITION            = UPDATE_POSITION << 16,
    UPDATE_CHILDREN_MATERIAL            = UPDATE_MATERIAL << 16,
    UPDATE_CHILDREN_MESH_DATA           = UPDATE_MESH_DATA << 16,
    UPDATE_CHILDREN_COMPUTED_VISIBILITY = UPDATE_COMPUTED_VISIBILITY << 16,
    UPDATE_CHILDREN_CLAMPED_SIZE        = UPDATE_CLAMPED_SIZE << 16,
    UPDATE_CHILDREN_TEXT_RENDER_DATA    = UPDATE_TEXT_RENDER_DATA << 16
};

HYP_MAKE_ENUM_FLAGS(UIObjectUpdateType)

HYP_ENUM()
enum class UIObjectScrollbarOrientation : uint8
{
    NONE        = 0x0,
    HORIZONTAL  = 0x1,
    VERTICAL    = 0x2,
    ALL         = HORIZONTAL | VERTICAL
};

HYP_MAKE_ENUM_FLAGS(UIObjectScrollbarOrientation)

static constexpr inline int UIObjectScrollbarOrientationToIndex(UIObjectScrollbarOrientation orientation)
{
    return (orientation == UIObjectScrollbarOrientation::HORIZONTAL)
        ? 0
        : (orientation == UIObjectScrollbarOrientation::VERTICAL)
            ? 1
            : -1;
}

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

    HYP_FORCE_INLINE void ApplyDefaultFlags()
    {
        for (int i = 0; i < 2; i++) {
            if (!flags[i]) {
                flags[i] = DEFAULT;
            }
        }
    }
};

static_assert(sizeof(UIObjectSize) == 16, "sizeof(UIObjectSize) must be 16 bytes to match C# struct size");

HYP_ENUM()
enum class UIObjectUpdateSizeFlags : uint32
{
    NONE                = 0x0,

    MAX_SIZE            = 0x1,
    INNER_SIZE          = 0x2,
    OUTER_SIZE          = 0x4,

    DEFAULT             = MAX_SIZE | INNER_SIZE | OUTER_SIZE
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

#pragma region UILockedUpdatesScope

struct UILockedUpdatesScope;

#pragma endregion UILockedUpdatesScope

#pragma region UIObjectSpawnContext

template <class UIObjectType>
struct UIObjectSpawnContext
{
    UIObjectType    *spawn_parent = nullptr;

    UIObjectSpawnContext(UIObjectType *spawn_parent)
        : spawn_parent(spawn_parent)
    {
        static_assert(std::is_base_of_v<UIObject, UIObjectType>, "UIObjectType must be a subclass of UIObject");

        AssertThrow(spawn_parent != nullptr);
    }

    UIObjectSpawnContext(const RC<UIObjectType> &spawn_parent)
        : spawn_parent(spawn_parent.Get())
    {
        static_assert(std::is_base_of_v<UIObject, UIObjectType>, "UIObjectType must be a subclass of UIObject");

        AssertThrow(spawn_parent != nullptr);
    }

    UIObjectSpawnContext(const UIObjectSpawnContext &other)                 = delete;
    UIObjectSpawnContext &operator=(const UIObjectSpawnContext &other)      = delete;
    UIObjectSpawnContext(UIObjectSpawnContext &&other) noexcept             = delete;
    UIObjectSpawnContext &operator=(UIObjectSpawnContext &&other) noexcept  = delete;

    ~UIObjectSpawnContext()                                                 = default;

    template <class T>
    RC<T> CreateUIObject(Vec2i position, UIObjectSize size)
    {
        static_assert(std::is_base_of_v<UIObject, T>, "T must be a subclass of UIObject");

        return spawn_parent->template CreateUIObject<T>(position, size);
    }

    template <class T>
    RC<T> CreateUIObject(Name name, Vec2i position, UIObjectSize size)
    {
        static_assert(std::is_base_of_v<UIObject, T>, "T must be a subclass of UIObject");

        return spawn_parent->template CreateUIObject<T>(name, position, size);
    }

    HYP_FORCE_INLINE explicit operator bool() const
        { return spawn_parent != nullptr; }
};

#pragma endregion UIObjectSpawnContext

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

    UIObject(UIObjectType type, const ThreadID &owner_thread_id = ThreadID::invalid);

public:
    friend class UIRenderSubsystem;
    friend class UIStage;
    friend struct UILockedUpdatesScope;

    static constexpr int scrollbar_size = 15;

    UIObject();
    UIObject(const UIObject &other)                 = delete;
    UIObject &operator=(const UIObject &other)      = delete;
    UIObject(UIObject &&other) noexcept             = delete;
    UIObject &operator=(UIObject &&other) noexcept  = delete;
    virtual ~UIObject();

    HYP_METHOD()
    virtual void Init();

    virtual void Update(GameCounter::TickUnit delta) final;

    HYP_FORCE_INLINE const UIObjectID &GetID() const
        { return m_id; }

    HYP_METHOD()
    HYP_FORCE_INLINE UIObjectType GetType() const
        { return m_type; }

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Entity> &GetEntity() const
        { return m_node_proxy.IsValid() ? m_node_proxy->GetEntity() : Handle<Entity>::empty; }

    HYP_METHOD()
    UIStage *GetStage() const;

    HYP_METHOD()
    void SetStage(UIStage *stage);

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
    HYP_FORCE_INLINE bool IsPositionAbsolute() const
        { return m_is_position_absolute; }

    HYP_METHOD()
    void SetIsPositionAbsolute(bool is_position_absolute);

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

    /*! \brief Get the computed size (in pixels) of the UIObject, clamped within the parent's boundary.
     *  The actual size of the UI object is calculated based on the size of the parent object and the size of the object itself.
     *  \return The computed size of the UI object, clamped within the parent's boundary. */
    HYP_METHOD()
    HYP_FORCE_INLINE Vec2i GetActualSizeClamped() const
        { return m_actual_size_clamped; }

    /*! \brief Get the computed inner size (in pixels) of the UI object.
     *  \return The computed inner size of the UI object */
    HYP_METHOD()
    HYP_FORCE_INLINE Vec2i GetActualInnerSize() const
        { return m_actual_inner_size; }

    HYP_FORCE_INLINE bool UseAutoSizing() const
    {
        return (GetSize().GetAllFlags() | GetInnerSize().GetAllFlags() | GetMaxSize().GetAllFlags()) & UIObjectSize::AUTO;
    }

    /*! \brief Get the scroll offset (in pixels) of the UI object.
     *  \return The scroll offset of the UI object */
    HYP_METHOD()
    Vec2i GetScrollOffset() const;

    /*! \brief Set the scroll offset (in pixels) of the UI object.
     *  \param scroll_offset The scroll offset of the UI object
     *  \param smooth Whether or not to interpolate the scroll offset to the given value. If false, it will immediately move to the given value. */
    HYP_METHOD()
    void SetScrollOffset(Vec2i scroll_offset, bool smooth);

    HYP_METHOD()
    virtual int GetVerticalScrollbarSize() const
        { return scrollbar_size; }

    HYP_METHOD()
    virtual int GetHorizontalScrollbarSize() const
        { return scrollbar_size; }

    HYP_METHOD()
    virtual bool CanScroll(UIObjectScrollbarOrientation orientation) const
        { return false; }

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

    /*! \brief Check if the UI object receives update events.
     *  \return True if the UI object receives update events, false otherwise */
    HYP_METHOD()
    virtual bool ReceivesUpdate() const
        { return m_receives_update; }

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
    HYP_METHOD(Property="AffectsParentSize")
    void SetAffectsParentSize(bool affects_parent_size);

    /*! \brief Check if the UI object affects the size of its parent.
     *  \details If true, the size of the parent object will be include the size of this object when calculating the parent's size.
     *  \return True if the UI object affects the size of its parent, false otherwise */
    HYP_METHOD(Property="AffectsParentSize")
    HYP_FORCE_INLINE bool AffectsParentSize() const
        { return m_affects_parent_size; }

    /*! \brief Get the border radius of the UI object
     *  \details The border radius of the UI object is used to create rounded corners for the object's border.
     *  \return The border radius of the UI object */
    HYP_METHOD(Property="BorderRadius")
    HYP_FORCE_INLINE uint32 GetBorderRadius() const
        { return m_border_radius; }

    /*! \brief Set the border radius of the UI object
     *  \details The border radius of the UI object is used to create rounded corners for the object's border.
     *  \param border_radius The border radius of the UI object */
    HYP_METHOD(Property="BorderRadius")
    void SetBorderRadius(uint32 border_radius);

    /*! \brief Get the border flags of the UI object
     *  \details The border flags of the UI object are used to determine which borders of the object should be rounded, if the border radius is set to a non-zero value.
     *  \example To display a border radius the top left and right corners of the object, set the border flags to \code{UOB_TOP | UOB_LEFT | UOB_RIGHT}.
     *  \return The border flags of the UI object */
    HYP_METHOD(Property="BorderFlags")
    HYP_FORCE_INLINE EnumFlags<UIObjectBorderFlags> GetBorderFlags() const
        { return m_border_flags; }

    HYP_METHOD(Property="BorderFlags")
    void SetBorderFlags(EnumFlags<UIObjectBorderFlags> border_flags);

    UIObjectAlignment GetOriginAlignment() const;
    void SetOriginAlignment(UIObjectAlignment alignment);

    UIObjectAlignment GetParentAlignment() const;
    void SetParentAlignment(UIObjectAlignment alignment);

    HYP_FORCE_INLINE bool IsPositionDependentOnSize() const
        { return m_origin_alignment != UIObjectAlignment::TOP_LEFT; }

    HYP_FORCE_INLINE bool IsPositionDependentOnParentSize() const
        { return m_parent_alignment != UIObjectAlignment::TOP_LEFT; }

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

    /*! \brief Get the blended background color of the UI object, including its parents. Blends RGB components until alpha adds up to 1.0.
     *  \note Images and other objects that use a texture will not be blended, only the background color.
     *  \return The blended background color of the UI object */
    Color ComputeBlendedBackgroundColor() const;

    /*! \brief Get the text color of the UI object
     * \return The text color of the UI object */
    HYP_METHOD(Property="TextColor")
    Color GetTextColor() const;

    /*! \brief Set the text color of the UI object
     *  \param text_color The text color of the UI object */
    HYP_METHOD(Property="TextColor")
    virtual void SetTextColor(const Color &text_color);

    /*! \brief Gets the text to render.
     * 
     * \return The text to render. */
    HYP_METHOD(Property="Text")
    HYP_FORCE_INLINE const String &GetText() const
        { return m_text; }

    HYP_METHOD(Property="Text")
    virtual void SetText(const String &text);

    HYP_METHOD(Property="TextSize")
    float GetTextSize() const;

    HYP_METHOD(Property="TextSize")
    void SetTextSize(float text_size); 

    /*! \brief Check if the UI object is set to visible or not. This does not include computed visibility.
     *  \returns True if the object is visible, false otherwise. */
    HYP_METHOD(Property="IsVisible")
    bool IsVisible() const;

    /*! \brief Set the visibility of the UI object.
     *  \details The visibility of the UI object determines whether the object is rendered in the UI scene.
     *  Can be used to hide the object without removing it from the scene.
     *  \param is_visible Whether to set the object as visible or not. */
    HYP_METHOD(Property="IsVisible")
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
    HYP_METHOD()
    UIObject *GetParentUIObject() const;

    /*! \brief Get the closest parent UIObject with UIObjectType \ref{type} if one exists.
     *  \returns A pointer to the closest parent UIObject with UIObjectType \ref{type} or nullptr if none exists. */ 
    UIObject *GetClosestParentUIObject(UIObjectType type) const;

    template <class T>
    RC<T> GetClosestParentUIObject() const
    {
        static_assert(std::is_base_of_v<UIObject, T>, "T must be a subclass of UIObject");

        return GetClosestParentUIObject_Proc([](const UIObject *parent) -> bool
        {
            return parent->IsInstanceOf<T>();
        }).template CastUnsafe<T>();
    }

    template <class T>
    RC<T> GetClosestSpawnParent() const
    {
        static_assert(std::is_base_of_v<UIObject, T>, "T must be a subclass of UIObject");

        return GetClosestSpawnParent_Proc([](const UIObject *parent) -> bool
        {
            return parent->IsInstanceOf<T>();
        }).template CastUnsafe<T>();
    }

    /*! \brief The UIObject that this was spawned from. Not necessarily the parent UIObject that this is attached to in the graph.
     *  \returns A weak reference to the UIObject that this was spawned from. */
    HYP_FORCE_INLINE const Weak<UIObject> &GetSpawnParent() const
        { return m_spawn_parent; }

    HYP_METHOD()
    virtual void AddChildUIObject(const RC<UIObject> &ui_object);

    HYP_METHOD()
    virtual bool RemoveChildUIObject(UIObject *ui_object);

    /*! \brief Remove all child UIObjects from this object.
     *  \returns The number of child UIObjects removed. */
    virtual int RemoveAllChildUIObjects();

    /*! \brief Remove all child UIObjects from this object that match the predicate.
     *  \param predicate The predicate to match against the child UIObjects.
     *  \returns The number of child UIObjects removed. */
    virtual int RemoveAllChildUIObjects(ProcRef<bool(UIObject *)> predicate);

    /*! \brief Remove this object from its parent UI object, if applicable.
     *  \note It is possible that you are removing the last strong reference to `this` by calling this method,
     *  invalidating the pointer. Proper care must be taken to ensure `this` is not reused after calling this method.
     *  If you need to use the object again, use \ref{DetachFromParent} which returns a strong reference counted pointer to `this`,
     *  ensuring it does not get instantly deleted.
     *  \returns A boolean indicating whether or not the object could be removed from its parent */
    HYP_METHOD()
    virtual bool RemoveFromParent();

    /*! \brief Remove this object from its parent UI object, if applicable. Ensures the object is not immediately deleted
     *  in the case that the parent UIObject holds the last reference to `this`.
     *  \returns A reference counted pointer to `this`. */
    HYP_METHOD()
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
    RC<UIObject> FindChildUIObject(ProcRef<bool(UIObject *)> predicate, bool deep = true) const;

    /*! \brief Check if the UI object has any child UIObjects.
     *  \return True if the object has child UIObjects, false otherwise. */ 
    HYP_METHOD()
    bool HasChildUIObjects() const;

    /*! \brief Gets the child UIObject at the specified index.
     *  \param index The index of the child UIObject to get.
     *  \return The child UIObject at the specified index. */
    HYP_METHOD()
    RC<UIObject> GetChildUIObject(int index) const;

    Array<UIObject *> GetChildUIObjects(bool deep) const;

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

    HYP_METHOD()
    World *GetWorld() const;

    virtual Scene *GetScene() const;

    const Handle<Material> &GetMaterial() const;

    /*! \brief Get the AABB of the UIObject (calculated with absolute positioning, or world space) */
    HYP_METHOD()
    HYP_FORCE_INLINE const BoundingBox &GetAABB() const
        { return m_aabb; }

    /*! \brief Get the AABB of the UIObject (calculated with absolute positioning, or world space), clamped within the parent's boundary. */
    HYP_METHOD()
    HYP_FORCE_INLINE const BoundingBox &GetAABBClamped() const
        { return m_aabb_clamped; }

    BoundingBox GetWorldAABB() const;
    BoundingBox GetLocalAABB() const;

    const NodeTag &GetNodeTag(WeakName key) const;
    void SetNodeTag(NodeTag &&tag);
    bool HasNodeTag(WeakName key) const;
    bool RemoveNodeTag(WeakName key);

    /*! \brief The default event handler result which is combined with the results of bound event handlers, if the result is equal to OK.
     *  E.g UIButton could return OK if the button was clicked, and the default event handler result could be set to STOP_BUBBLING so that the event does not propagate to objects behind it. */
    virtual UIEventHandlerResult GetDefaultEventHandlerResult() const
        {  return UIEventHandlerResult(); }

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
    void CollectObjects(ProcRef<void(UIObject *)> proc, bool only_visible = true) const;

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
    void ForEachChildUIObject_Proc(ProcRef<IterationResult(UIObject *)> proc, bool deep = true) const;

    /*! \brief Spawn a new UIObject with the given HypClass \ref{hyp_class}. The object will not be attached to the current UIStage.
     *  The object will not be named. To name the object, use the other CreateUIObject overload.
     * 
     *  \param hyp_class The HypClass associated with the UIObject type you wish to spawn.
     *  \param name The name of the UIObject.
     *  \param position The position of the UI object.
     *  \param size The size of the UI object.
     *  \return A handle to the created UI object. */
    HYP_NODISCARD RC<UIObject> CreateUIObject(const HypClass *hyp_class, Name name, Vec2i position, UIObjectSize size);

    /*! \brief Spawn a new UIObject of type T. The object will not be attached to the current UIStage.
     * 
     *  \tparam T The type of UI object to create. Must be a derived class of UIObject.
     *  \return A handle to the created UI object. */
    template <class T>
    HYP_NODISCARD RC<T> CreateUIObject()
    {
        return CreateUIObject<T>(Name::Invalid(), Vec2i::Zero(), UIObjectSize(UIObjectSize::AUTO));
    }

    /*! \brief Spawn a new UIObject of type T. The object will not be attached to the current UIStage.
     * 
     *  \tparam T The type of UI object to create. Must be a derived class of UIObject.
     *  \param position The position of the UI object.
     *  \param size The size of the UI object.
     *  \return A handle to the created UI object. */
    template <class T>
    HYP_NODISCARD RC<T> CreateUIObject(
        Vec2i position,
        UIObjectSize size
    )
    {
        return CreateUIObject<T>(Name::Invalid(), position, size);
    }

    /*! \brief Spawn a new UIObject of type T. The object will not be attached to the current UIStage.
     *  The object will not be named. To name the object, use the other CreateUIObject overload.
     * 
     *  \tparam T The type of UI object to create. Must be a derived class of UIObject.
     *  \param name The name of the UI object.
     *  \param position The position of the UI object.
     *  \param size The size of the UI object.
     *  \return A handle to the created UI object. */
    template <class T>
    HYP_NODISCARD RC<T> CreateUIObject(
        Name name,
        Vec2i position,
        UIObjectSize size
    )
    {
        AssertOnOwnerThread();

        // AssertThrow(IsInit());
        AssertThrow(GetNode().IsValid());

        if (!name.IsValid()) {
            name = Name::Unique(ANSIString("Unnamed_") + TypeNameHelper<T, true>::value.Data());
        }

        NodeProxy node_proxy(MakeRefCountedPtr<Node>(name.LookupString()));
        
        // if (attach_to_root) {
        //     node_proxy = GetNode()->AddChild(node_proxy);
        // }
        
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

    // Events
    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult>                        OnInit;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult>                        OnAttached;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult>                        OnRemoved;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, UIObject *>            OnChildAttached;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, UIObject *>            OnChildRemoved;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, const MouseEvent &>    OnMouseDown;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, const MouseEvent &>    OnMouseUp;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, const MouseEvent &>    OnMouseDrag;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, const MouseEvent &>    OnMouseHover;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, const MouseEvent &>    OnMouseLeave;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, const MouseEvent &>    OnMouseMove;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, const MouseEvent &>    OnGainFocus;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, const MouseEvent &>    OnLoseFocus;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, const MouseEvent &>    OnScroll;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, const MouseEvent &>    OnClick;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, const KeyboardEvent &> OnKeyDown;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, const KeyboardEvent &> OnKeyUp;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult>                        OnTextChange;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult>                        OnSizeChange;

protected:
    HYP_FORCE_INLINE void AssertOnOwnerThread() const
    {
        if (Scene *scene = GetScene()) {
            Threads::AssertOnThread(scene->GetOwnerThreadID());
        }
    }

    RC<UIObject> GetClosestParentUIObject_Proc(const ProcRef<bool(UIObject *)> &proc) const;
    RC<UIObject> GetClosestSpawnParent_Proc(const ProcRef<bool(UIObject *)> &proc) const;

    HYP_FORCE_INLINE void SetReceivesUpdate(bool receives_update)
        { m_receives_update = receives_update; }

    virtual void UpdateSize_Internal(bool update_children);

    virtual void SetDataSource_Internal(UIDataSourceBase *data_source);

    virtual void SetFocusState_Internal(EnumFlags<UIObjectFocusState> focus_state);

    virtual void Update_Internal(GameCounter::TickUnit delta);

    virtual void OnAttached_Internal(UIObject *parent);
    virtual void OnRemoved_Internal();

    /*! \brief Check if the object has been computed as visible or not. E.g scrolled out of view in a parent container */
    virtual void UpdateComputedVisibility(bool update_children = true);
    virtual void OnComputedVisibilityChange_Internal() { }

    void UpdateComputedDepth(bool update_children = true);

    void UpdateComputedTextSize();

    /*! \brief Sets the NodeProxy for this UIObject.
     *  \internal To be called internally from UIStage */
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

    Vec2i GetParentScrollOffset() const;

    /*! \brief Add a DelegateHandler to be removed when the object is destructed */
    void AddDelegateHandler(DelegateHandler &&delegate_handler)
        { m_delegate_handlers.PushBack(std::move(delegate_handler)); }

    void UpdateMeshData(bool update_children = true);
    virtual void UpdateMeshData_Internal();

    void UpdateMaterial(bool update_children = true);

    Array<UIObject *> FilterChildUIObjects(ProcRef<bool(UIObject *)> predicate, bool deep) const;

    virtual void SetStage_Internal(UIStage *stage);

    void OnFontAtlasUpdate();
    virtual void OnFontAtlasUpdate_Internal() { }

    void OnTextSizeUpdate();
    virtual void OnTextSizeUpdate_Internal() { }

    void OnScrollOffsetUpdate(Vec2f delta);
    virtual void OnScrollOffsetUpdate_Internal(Vec2f delta) { }

    bool NeedsRepaint() const;
    void SetNeedsRepaintFlag(bool needs_repaint = true);

    void Repaint();
    virtual bool Repaint_Internal();

    UIStage                         *m_stage;
    Weak<UIObject>                  m_spawn_parent;

    Name                            m_name;

    Vec2i                           m_position;
    Vec2f                           m_offset_position;
    bool                            m_is_position_absolute;

    UIObjectSize                    m_size;
    Vec2i                           m_actual_size;
    Vec2i                           m_actual_size_clamped;

    UIObjectSize                    m_inner_size;
    Vec2i                           m_actual_inner_size;

    UIObjectSize                    m_max_size;
    Vec2i                           m_actual_max_size;

    BoundingBox                     m_aabb;
    BoundingBox                     m_aabb_clamped;

    BlendVar<Vec2f>                 m_scroll_offset;

    RC<UIObject>                    m_vertical_scrollbar;
    RC<UIObject>                    m_horizontal_scrollbar;

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

    RC<UIDataSourceBase>            m_data_source;
    DelegateHandler                 m_data_source_on_change_handler;
    DelegateHandler                 m_data_source_on_element_add_handler;
    DelegateHandler                 m_data_source_on_element_remove_handler;
    DelegateHandler                 m_data_source_on_element_update_handler;

    UUID                            m_data_source_element_uuid;

    EnumFlags<UIObjectUpdateType>   m_deferred_updates;
    EnumFlags<UIObjectUpdateType>   m_locked_updates;

private:
    template <class T>
    RC<UIObject> CreateUIObjectInternal(Name name, NodeProxy &node_proxy, bool init = false)
    {
        AssertThrow(node_proxy.IsValid());

        static_assert(std::is_base_of_v<UIObject, T>, "T must be a derived class of UIObject");

        RC<UIObject> ui_object = MakeRefCountedPtr<T>();

        UIStage *stage = GetStage();

        ui_object->m_spawn_parent = WeakRefCountedPtrFromThis();
        ui_object->m_stage = stage;

        // Small optimization so UpdateComputedTextSize() doesn't need to be called when attached.
        // Computed text size is based on spawn parent rather than the node it's attached to,
        // so we can set it right here.
        ui_object->m_computed_text_size = m_computed_text_size;

        ui_object->SetNodeProxy(node_proxy);
        ui_object->SetName(name);

        if (init) {
            ui_object->Init();
            ui_object->SetStage_Internal(stage);
        }

        return ui_object;
    }

    void ComputeOffsetPosition();

    void UpdateActualSizes(UpdateSizePhase phase, EnumFlags<UIObjectUpdateSizeFlags> flags);
    virtual void ComputeActualSize(const UIObjectSize &size, Vec2i &actual_size, UpdateSizePhase phase, bool is_inner = false);

    template <class Lambda>
    void ForEachChildUIObject(Lambda &&lambda, bool deep = true) const;

    template <class Lambda>
    void ForEachParentUIObject(Lambda &&lambda) const;

    void SetEntityAABB(const BoundingBox &aabb);

    void UpdateClampedSize(bool update_children = true);

    void UpdateNodeTransform();

    Handle<Material> CreateMaterial() const;

    const UIObjectID                        m_id;
    const UIObjectType                      m_type;

    bool                                    m_is_init;

    EnumFlags<UIObjectFocusState>           m_focus_state;

    bool                                    m_is_visible;
    bool                                    m_computed_visibility;

    int                                     m_computed_depth;

    float                                   m_text_size;
    float                                   m_computed_text_size;
    
    bool                                    m_accepts_focus;

    bool                                    m_receives_update;

    bool                                    m_affects_parent_size;

    AtomicVar<bool>                         m_needs_repaint;

    NodeProxy                               m_node_proxy;

    Array<RC<UIObject>>                     m_child_ui_objects;

    Array<DelegateHandler>                  m_delegate_handlers;
};

#pragma endregion UIObject

#pragma region UILockedUpdatesScope

struct UILockedUpdatesScope
{
    UILockedUpdatesScope(UIObject &ui_object, EnumFlags<UIObjectUpdateType> value)
        : ui_object(ui_object),
          value(value & ~ui_object.m_locked_updates)
    {
        ui_object.m_locked_updates |= this->value;
    }

    UILockedUpdatesScope(const UILockedUpdatesScope &other)                 = delete;
    UILockedUpdatesScope &operator=(const UILockedUpdatesScope &other)      = delete;

    UILockedUpdatesScope(UILockedUpdatesScope &&other) noexcept             = delete;
    UILockedUpdatesScope &operator=(UILockedUpdatesScope &&other) noexcept  = delete;

    ~UILockedUpdatesScope()
    {
        ui_object.m_locked_updates &= ~value;
    }

    UIObject                        &ui_object;
    EnumFlags<UIObjectUpdateType>   value;
};

#pragma endregion UILockedUpdatesScope

} // namespace hyperion

#endif