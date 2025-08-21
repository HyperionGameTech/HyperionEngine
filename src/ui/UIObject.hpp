/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/object/HypObject.hpp>

#include <core/containers/Array.hpp>

#include <core/functional/ScriptableDelegate.hpp>

#include <core/utilities/UniqueId.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Uuid.hpp>
#include <core/utilities/Pair.hpp>
#include <core/utilities/ForEach.hpp>

#include <scene/Node.hpp>
#include <scene/Scene.hpp>
#include <rendering/Material.hpp>

#include <core/math/Color.hpp>
#include <core/math/BlendVar.hpp>

#include <core/Types.hpp>

namespace hyperion {

struct ScriptComponent;

class UIObject;
class UIStage;
class UISubsystem;
class UIDataSourceBase;
class UIDataSource;
class Mesh;

// Helper function to get the scene from a UIStage
template <class UIStageType>
static inline Scene* GetScene(UIStageType* stage)
{
    Assert(stage != nullptr);

    return stage->GetScene().Get();
}

struct UIObjectInstanceData
{
    Matrix4 transform;
    Vec4f texcoords;
};

HYP_STRUCT(Size = 24)
struct alignas(8) UIEventHandlerResult
{
    enum Value : uint32
    {
        ERR = 0x1u << 31u,
        OK = 0x0,

        // Stop bubbling the event up the hierarchy
        STOP_BUBBLING = 0x1
    };

    UIEventHandlerResult()
        : value(OK),
          message(nullptr),
          functionName(nullptr)
    {
    }

    UIEventHandlerResult(uint32 value)
        : value(value),
          message(nullptr),
          functionName(nullptr)
    {
    }

    UIEventHandlerResult(uint32 value, const StaticMessage& message)
        : value(value),
          message(&message),
          functionName(nullptr)
    {
    }

    UIEventHandlerResult(uint32 value, const StaticMessage& message, const StaticMessage& functionName)
        : value(value),
          message(&message),
          functionName(&functionName)
    {
    }

    UIEventHandlerResult& operator=(uint32 value)
    {
        this->value = value;

        message = nullptr;
        functionName = nullptr;

        return *this;
    }

    UIEventHandlerResult(const UIEventHandlerResult& other) = default;
    UIEventHandlerResult& operator=(const UIEventHandlerResult& other) = default;

    UIEventHandlerResult(UIEventHandlerResult&& other) noexcept = default;
    UIEventHandlerResult& operator=(UIEventHandlerResult&& other) noexcept = default;

    ~UIEventHandlerResult() = default;

    // to allow delegate to stop iterating, the value must be convertible to IterationResult
    HYP_FORCE_INLINE operator IterationResult() const
    {
        return value == OK
            ? IterationResult::CONTINUE
            : IterationResult::STOP;
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return value != 0;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !value;
    }

    HYP_FORCE_INLINE explicit operator uint32() const
    {
        return value;
    }

    HYP_FORCE_INLINE bool operator==(const UIEventHandlerResult& other) const
    {
        return value == other.value;
    }

    HYP_FORCE_INLINE bool operator!=(const UIEventHandlerResult& other) const
    {
        return value != other.value;
    }

    HYP_FORCE_INLINE UIEventHandlerResult operator&(const UIEventHandlerResult& other) const
    {
        return UIEventHandlerResult(value & other.value);
    }

    HYP_FORCE_INLINE UIEventHandlerResult& operator&=(const UIEventHandlerResult& other)
    {
        value &= other.value;

        return *this;
    }

    HYP_FORCE_INLINE UIEventHandlerResult operator|(const UIEventHandlerResult& other) const
    {
        return UIEventHandlerResult(value | other.value);
    }

    HYP_FORCE_INLINE UIEventHandlerResult& operator|=(const UIEventHandlerResult& other)
    {
        value |= other.value;

        return *this;
    }

    HYP_FORCE_INLINE UIEventHandlerResult operator~() const
    {
        return UIEventHandlerResult(~value);
    }

    HYP_FORCE_INLINE Optional<ANSIStringView> GetMessage() const
    {
        if (!message)
        {
            return {};
        }

        return message->value;
    }

    HYP_FORCE_INLINE Optional<ANSIStringView> GetFunctionName() const
    {
        if (!functionName)
        {
            return {};
        }

        return functionName->value;
    }

    uint32 value;
    const StaticMessage* message;
    const StaticMessage* functionName;
};

HYP_ENUM()
enum class UIObjectAlignment : uint32
{
    TOP_LEFT = 0,
    TOP_RIGHT = 1,

    CENTER = 2,

    BOTTOM_LEFT = 3,
    BOTTOM_RIGHT = 4
};

HYP_ENUM()
enum class UIObjectFocusState : uint32
{
    NONE = 0x0,
    HOVER = 0x1,
    PRESSED = 0x2,
    TOGGLED = 0x4,
    FOCUSED = 0x8
};

HYP_MAKE_ENUM_FLAGS(UIObjectFocusState)

HYP_ENUM()
enum class UIObjectBorderFlags : uint32
{
    NONE = 0x0,
    TOP = 0x1,
    LEFT = 0x2,
    BOTTOM = 0x4,
    RIGHT = 0x8,
    ALL = TOP | LEFT | BOTTOM | RIGHT
};

HYP_MAKE_ENUM_FLAGS(UIObjectBorderFlags)

HYP_ENUM()
enum class UIObjectUpdateType : uint32
{
    NONE = 0x0,

    UPDATE_SIZE = 0x1,
    UPDATE_POSITION = 0x2,
    UPDATE_MATERIAL = 0x4,
    UPDATE_MESH_DATA = 0x8,
    UPDATE_COMPUTED_VISIBILITY = 0x10,
    UPDATE_CLAMPED_SIZE = 0x20,
    UPDATE_CUSTOM = 0x40, // Call the Update_Internal() method
    UPDATE_ALL = UINT16_MAX,

    UPDATE_CHILDREN_SIZE = UPDATE_SIZE << 16,
    UPDATE_CHILDREN_POSITION = UPDATE_POSITION << 16,
    UPDATE_CHILDREN_MATERIAL = UPDATE_MATERIAL << 16,
    UPDATE_CHILDREN_MESH_DATA = UPDATE_MESH_DATA << 16,
    UPDATE_CHILDREN_COMPUTED_VISIBILITY = UPDATE_COMPUTED_VISIBILITY << 16,
    UPDATE_CHILDREN_CLAMPED_SIZE = UPDATE_CLAMPED_SIZE << 16,
    UPDATE_CHILDREN_CUSTOM = UPDATE_CUSTOM << 16,
    UPDATE_CHILDREN_ALL = UPDATE_ALL << 16
};

HYP_MAKE_ENUM_FLAGS(UIObjectUpdateType)

HYP_ENUM()
enum ScrollAxis : uint8
{
    SA_NONE = 0x0,
    SA_HORIZONTAL = 0x1,
    SA_VERTICAL = 0x2,
    SA_ALL = SA_HORIZONTAL | SA_VERTICAL
};

HYP_MAKE_ENUM_FLAGS(ScrollAxis)

static constexpr int g_scrollAxisIndices[4] = {
    -1, 0, 1, -1
};

static constexpr inline int ScrollAxisToIndex(ScrollAxis axis)
{
    return g_scrollAxisIndices[uint8(axis)];
}

HYP_STRUCT()
struct UIObjectAspectRatio
{
    HYP_FIELD()
    float x = 1.0f;

    HYP_FIELD()
    float y = 1.0f;

    UIObjectAspectRatio() = default;

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

    UIObjectAspectRatio(const UIObjectAspectRatio& other) = default;
    UIObjectAspectRatio& operator=(const UIObjectAspectRatio& other) = default;
    UIObjectAspectRatio(UIObjectAspectRatio&& other) noexcept = default;
    UIObjectAspectRatio& operator=(UIObjectAspectRatio&& other) noexcept = default;

    HYP_FORCE_INLINE bool IsValid() const
    {
        return float(*this) == float(*this);
    }

    HYP_FORCE_INLINE explicit operator float() const
    {
        return x / y;
    }
};

#pragma region UIObjectSize

HYP_STRUCT()
struct UIObjectSize
{
    enum Flags
    {
        AUTO = 0x04,

        PIXEL = 0x10,
        PERCENT = 0x20,

        FILL = 0x40,

        DEFAULT = PIXEL
    };

    HYP_FIELD()
    uint32 flags[2];

    HYP_FIELD()
    Vec2i value;

    UIObjectSize()
        : flags { DEFAULT, DEFAULT },
          value { 0, 0 }
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
          value { 0, 0 }
    {
        ApplyDefaultFlags();
    }

    /*! \brief Construct by providing specific flags for each axis. */
    UIObjectSize(const Pair<int, uint32>& x, const Pair<int, uint32>& y)
        : flags { x.second, y.second },
          value { x.first, y.first }
    {
        ApplyDefaultFlags();
    }

    UIObjectSize(const UIObjectSize& other) = default;
    UIObjectSize& operator=(const UIObjectSize& other) = default;
    UIObjectSize(UIObjectSize&& other) noexcept = default;
    UIObjectSize& operator=(UIObjectSize&& other) noexcept = default;
    ~UIObjectSize() = default;

    HYP_FORCE_INLINE const Vec2i& GetValue() const
    {
        return value;
    }

    HYP_FORCE_INLINE uint32 GetFlagsX() const
    {
        return flags[0];
    }

    HYP_FORCE_INLINE uint32 GetFlagsY() const
    {
        return flags[1];
    }

    HYP_FORCE_INLINE uint32 GetAllFlags() const
    {
        return flags[0] | flags[1];
    }

    HYP_FORCE_INLINE void ApplyDefaultFlags()
    {
        for (int i = 0; i < 2; i++)
        {
            if (!flags[i])
            {
                flags[i] = DEFAULT;
            }
        }
    }
};

static_assert(sizeof(UIObjectSize) == 16, "sizeof(UIObjectSize) must be 16 bytes to match C# struct size");

HYP_ENUM()
enum class UIObjectUpdateSizeFlags : uint32
{
    NONE = 0x0,

    MAX_SIZE = 0x1,
    INNER_SIZE = 0x2,
    OUTER_SIZE = 0x4,

    DEFAULT = MAX_SIZE | INNER_SIZE | OUTER_SIZE
};

HYP_MAKE_ENUM_FLAGS(UIObjectUpdateSizeFlags)

#pragma endregion UIObjectSize

#pragma region UIObjectQuadMeshHelper

class HYP_API UIObjectQuadMeshHelper
{
public:
    static const Handle<Mesh>& GetQuadMesh();
};

#pragma endregion UIObjectQuadMeshHelper

#pragma region UILockedUpdatesScope

struct UILockedUpdatesScope;

#pragma endregion UILockedUpdatesScope

#pragma region UIObjectSpawnContext

template <class UIObjectType>
struct UIObjectSpawnContext
{
    UIObjectType* spawnParent = nullptr;

    UIObjectSpawnContext(UIObjectType* spawnParent)
        : spawnParent(spawnParent)
    {
        static_assert(std::is_base_of_v<UIObject, UIObjectType>, "UIObjectType must be a subclass of UIObject");

        Assert(spawnParent != nullptr);
    }

    UIObjectSpawnContext(const Handle<UIObjectType>& spawnParent)
        : spawnParent(spawnParent.Get())
    {
        static_assert(std::is_base_of_v<UIObject, UIObjectType>, "UIObjectType must be a subclass of UIObject");

        Assert(spawnParent != nullptr);
    }

    UIObjectSpawnContext(const UIObjectSpawnContext& other) = delete;
    UIObjectSpawnContext& operator=(const UIObjectSpawnContext& other) = delete;
    UIObjectSpawnContext(UIObjectSpawnContext&& other) noexcept = delete;
    UIObjectSpawnContext& operator=(UIObjectSpawnContext&& other) noexcept = delete;

    ~UIObjectSpawnContext() = default;

    template <class T>
    Handle<T> CreateUIObject(Vec2i position, UIObjectSize size)
    {
        static_assert(std::is_base_of_v<UIObject, T>, "T must be a subclass of UIObject");

        return spawnParent->template CreateUIObject<T>(position, size);
    }

    template <class T>
    Handle<T> CreateUIObject(Name name, Vec2i position, UIObjectSize size)
    {
        static_assert(std::is_base_of_v<UIObject, T>, "T must be a subclass of UIObject");

        return spawnParent->template CreateUIObject<T>(name, position, size);
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return spawnParent != nullptr;
    }
};

#pragma endregion UIObjectSpawnContext

#pragma region UIObject

HYP_CLASS(Abstract)
class HYP_API UIObject : public HypObjectBase
{
    HYP_OBJECT_BODY(UIObject);

protected:
    enum class UpdateSizePhase
    {
        BEFORE_CHILDREN,
        AFTER_CHILDREN
    };

    UIObject(const ThreadId& ownerThreadId);

public:
    friend class UISubsystem;
    friend class UIStage;
    friend struct UILockedUpdatesScope;
    friend class UIUpdateManager;

    static constexpr int scrollbarSize = 15;

    HYP_FIELD()
    static constexpr int tmpField = 4;

    UIObject();
    UIObject(const UIObject& other) = delete;
    UIObject& operator=(const UIObject& other) = delete;
    UIObject(UIObject&& other) noexcept = delete;
    UIObject& operator=(UIObject&& other) noexcept = delete;
    virtual ~UIObject();

    virtual void Update(float delta) final;

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Entity>& GetEntity() const
    {
        return ObjCast<Entity>(m_node);
    }

    HYP_METHOD()
    UIStage* GetStage() const;

    HYP_METHOD()
    void SetStage(UIStage* stage);

    HYP_METHOD(Property = "Name")
    Name GetName() const;

    HYP_METHOD(Property = "Name")
    void SetName(Name name);

    HYP_METHOD(Property = "Position")
    Vec2i GetPosition() const;

    HYP_METHOD(Property = "Position")
    void SetPosition(Vec2i position);

    HYP_METHOD()
    Vec2f GetOffsetPosition() const;

    HYP_METHOD()
    Vec2f GetAbsolutePosition() const;

    HYP_METHOD(Property = "IsPositionAbsolute", XMLAttribute = "absolute")
    HYP_FORCE_INLINE bool IsPositionAbsolute() const
    {
        return m_isPositionAbsolute;
    }

    HYP_METHOD(Property = "IsPositionAbsolute", XMLAttribute = "absolute")
    void SetIsPositionAbsolute(bool isPositionAbsolute);

    HYP_METHOD(Property = "Size")
    UIObjectSize GetSize() const;

    HYP_METHOD(Property = "Size")
    void SetSize(UIObjectSize size);

    HYP_METHOD(Property = "InnerSize")
    UIObjectSize GetInnerSize() const;

    HYP_METHOD(Property = "InnerSize")
    void SetInnerSize(UIObjectSize size);

    HYP_METHOD(Property = "MaxSize")
    UIObjectSize GetMaxSize() const;

    HYP_METHOD(Property = "MaxSize")
    void SetMaxSize(UIObjectSize size);

    /*! \brief Get the computed size (in pixels) of the UI object.
     *  The actual size of the UI object is calculated based on the size of the parent object and the size of the object itself.
     *  \return The computed size of the UI object */
    HYP_METHOD()
    HYP_FORCE_INLINE Vec2i GetActualSize() const
    {
        return m_actualSize;
    }

    /*! \brief Get the computed size (in pixels) of the UIObject, clamped within the parent's boundary.
     *  The actual size of the UI object is calculated based on the size of the parent object and the size of the object itself.
     *  \return The computed size of the UI object, clamped within the parent's boundary. */
    HYP_METHOD()
    HYP_FORCE_INLINE Vec2i GetActualSizeClamped() const
    {
        return m_actualSizeClamped;
    }

    /*! \brief Get the computed inner size (in pixels) of the UI object.
     *  \return The computed inner size of the UI object */
    HYP_METHOD()
    HYP_FORCE_INLINE Vec2i GetActualInnerSize() const
    {
        return m_actualInnerSize;
    }

    HYP_FORCE_INLINE bool UseAutoSizing() const
    {
        return (GetSize().GetAllFlags() | GetInnerSize().GetAllFlags() | GetMaxSize().GetAllFlags()) & UIObjectSize::AUTO;
    }

    /*! \brief Get the scroll offset (in pixels) of the UI object.
     *  \return The scroll offset of the UI object */
    HYP_METHOD()
    Vec2f GetScrollOffset() const;

    /*! \brief Set the scroll offset (in pixels) of the UI object.
     *  \param scrollOffset The scroll offset of the UI object
     *  \param smooth Whether or not to interpolate the scroll offset to the given value. If false, it will immediately move to the given value. */
    HYP_METHOD()
    void SetScrollOffset(Vec2f scrollOffset, bool smooth);

    HYP_METHOD()
    void ScrollToChild(UIObject* child);

    HYP_METHOD()
    virtual int GetVerticalScrollbarSize() const
    {
        return scrollbarSize;
    }

    HYP_METHOD()
    virtual int GetHorizontalScrollbarSize() const
    {
        return scrollbarSize;
    }

    HYP_METHOD()
    virtual bool CanScrollOnAxis(ScrollAxis axis) const
    {
        return false;
    }

    /*! \brief Get the depth of the UI object, or the computed depth from the Node  if none has been explicitly set.
     *  \see{Node::CalculateDepth}
     *  \return The depth of the UI object */
    HYP_METHOD()
    int GetComputedDepth() const;

    /*! \brief Get the depth of the UI object
     *  The depth of the UI object is used to determine the rendering order of the object in the scene relative to its sibling elements, with higher depth values being rendered on top of lower depth values.
     *  If the depth value is set to 0, the depth will be determined by the node's depth in the scene.
     *  \return The depth of the UI object */
    HYP_METHOD(Property = "Depth")
    int GetDepth() const;

    /*! \brief Set the depth of the UI object
     *  The depth of the UI object is used to determine the rendering order of the object in the scene relative to its sibling elements, with higher depth values being rendered on top of lower depth values.
     *  Set the depth to a value between UIStage::g_minDepth and UIStage::g_maxDepth. If the depth value is set to 0, the depth will be determined by the node's depth in the scene.
     *  \param depth The depth of the UI object */
    HYP_METHOD(Property = "Depth")
    void SetDepth(int depth);

    /*! \brief Check if the UI object accepts focus. All UIObjects accept focus by default, unless overridden by derived classes or set using \ref{SetAcceptsFocus}.
     *  \return True if the this object accepts focus, false otherwise */
    HYP_METHOD(Property = "AcceptsFocus")
    virtual bool AcceptsFocus() const;

    /*! \brief Set whether the UI object accepts focus.
     *  \details If set to true, the UI object can receive focus. If set to false, the UI object cannot receive focus.
     *  \note If a class deriving \ref{UIObject} overrides \ref{AcceptsFocus}, this function has no effect. */
    HYP_METHOD(Property = "AcceptsFocus")
    void SetAcceptsFocus(bool acceptsFocus);

    HYP_METHOD()
    virtual bool NeedsUpdate() const
    {
        return m_deferredUpdates
            || ((CanScrollOnAxis(SA_HORIZONTAL) || CanScrollOnAxis(SA_VERTICAL)) && m_scrollOffset.GetTarget() != m_scrollOffset.GetValue());
    }

    /*! \brief Set the focus to this UI object, if AcceptsFocus() returns true.
     * This function is called when the UI object is focused. */
    HYP_METHOD()
    virtual void Focus();

    /*! \brief Remove the focus from this UI object, if AcceptsFocus() returns true.
     *  This function is called when the UI object loses focus.
     *  \param blurChildren If true, also remove focus from all child objects. */
    HYP_METHOD()
    virtual void Blur(bool blurChildren = true);

    /*! \brief Set whether the UI object affects the size of its parent.
     *  \details If true, the size of the parent object will be include the size of this object when calculating the parent's size.
     *  \param affectsParentSize Whether the UI object affects the size of its parent */
    HYP_METHOD(Property = "AffectsParentSize")
    void SetAffectsParentSize(bool affectsParentSize);

    /*! \brief Check if the UI object affects the size of its parent.
     *  \details If true, the size of the parent object will be include the size of this object when calculating the parent's size.
     *  \return True if the UI object affects the size of its parent, false otherwise */
    HYP_METHOD(Property = "AffectsParentSize")
    HYP_FORCE_INLINE bool AffectsParentSize() const
    {
        return m_affectsParentSize;
    }

    /*! \brief Get the border radius of the UI object
     *  \details The border radius of the UI object is used to create rounded corners for the object's border.
     *  \return The border radius of the UI object */
    HYP_METHOD(Property = "BorderRadius")
    HYP_FORCE_INLINE uint32 GetBorderRadius() const
    {
        return m_borderRadius;
    }

    /*! \brief Set the border radius of the UI object
     *  \details The border radius of the UI object is used to create rounded corners for the object's border.
     *  \param borderRadius The border radius of the UI object */
    HYP_METHOD(Property = "BorderRadius")
    void SetBorderRadius(uint32 borderRadius);

    /*! \brief Get the border flags of the UI object
     *  \details The border flags of the UI object are used to determine which borders of the object should be rounded, if the border radius is set to a non-zero value.
     *  \example To display a border radius the top left and right corners of the object, set the border flags to \code{UOB_TOP | UOB_LEFT | UOB_RIGHT}.
     *  \return The border flags of the UI object */
    HYP_METHOD(Property = "BorderFlags")
    HYP_FORCE_INLINE EnumFlags<UIObjectBorderFlags> GetBorderFlags() const
    {
        return m_borderFlags;
    }

    HYP_METHOD(Property = "BorderFlags")
    void SetBorderFlags(EnumFlags<UIObjectBorderFlags> borderFlags);

    UIObjectAlignment GetOriginAlignment() const;
    void SetOriginAlignment(UIObjectAlignment alignment);

    UIObjectAlignment GetParentAlignment() const;
    void SetParentAlignment(UIObjectAlignment alignment);

    HYP_FORCE_INLINE bool IsPositionDependentOnSize() const
    {
        return m_originAlignment != UIObjectAlignment::TOP_LEFT;
    }

    HYP_FORCE_INLINE bool IsPositionDependentOnParentSize() const
    {
        return m_parentAlignment != UIObjectAlignment::TOP_LEFT;
    }

    HYP_METHOD(Property = "AspectRatio")
    HYP_FORCE_INLINE UIObjectAspectRatio GetAspectRatio() const
    {
        return m_aspectRatio;
    }

    HYP_METHOD(Property = "AspectRatio")
    void SetAspectRatio(UIObjectAspectRatio aspectRatio);

    /*! \brief Get the padding of the UI object
     *  The padding of the UI object is used to add space around the object's content.
     *  \return The padding of the UI object */
    HYP_METHOD(Property = "Padding")
    HYP_FORCE_INLINE Vec2i GetPadding() const
    {
        return m_padding;
    }

    /*! \brief Set the padding of the UI object
     *  The padding of the UI object is used to add space around the object's content.
     *  \param padding The padding of the UI object */
    HYP_METHOD(Property = "Padding")
    void SetPadding(Vec2i padding);

    /*! \brief Get the background color of the UI object
     * \return The background color of the UI object */
    HYP_METHOD(Property = "BackgroundColor")
    HYP_FORCE_INLINE Color GetBackgroundColor() const
    {
        return m_backgroundColor;
    }

    /*! \brief Set the background color of the UI object
     *  \param backgroundColor The background color of the UI object */
    HYP_METHOD(Property = "BackgroundColor")
    void SetBackgroundColor(const Color& backgroundColor);

    /*! \brief Get the blended background color of the UI object, including its parents. Blends RGB components until alpha adds up to 1.0.
     *  \note Images and other objects that use a texture will not be blended, only the background color.
     *  \return The blended background color of the UI object */
    Color ComputeBlendedBackgroundColor() const;

    /*! \brief Get the text color of the UI object
     * \return The text color of the UI object */
    HYP_METHOD(Property = "TextColor")
    Color GetTextColor() const;

    /*! \brief Set the text color of the UI object
     *  \param textColor The text color of the UI object */
    HYP_METHOD(Property = "TextColor")
    virtual void SetTextColor(const Color& textColor);

    /*! \brief Gets the text to render.
     *
     * \return The text to render. */
    HYP_METHOD(Property = "Text")
    HYP_FORCE_INLINE const String& GetText() const
    {
        return m_text;
    }

    HYP_METHOD(Property = "Text")
    virtual void SetText(const String& text);

    HYP_METHOD(Property = "TextSize")
    float GetTextSize() const;

    HYP_METHOD(Property = "TextSize")
    void SetTextSize(float textSize);

    /*! \brief Check if the UI object is set to visible or not. This does not include computed visibility.
     *  \returns True if the object is visible, false otherwise. */
    HYP_METHOD(Property = "IsVisible")
    bool IsVisible() const;

    /*! \brief Set the visibility of the UI object.
     *  \details The visibility of the UI object determines whether the object is rendered in the UI scene.
     *  Can be used to hide the object without removing it from the scene.
     *  \param isVisible Whether to set the object as visible or not. */
    HYP_METHOD(Property = "IsVisible")
    void SetIsVisible(bool isVisible);

    /*! \brief Get the computed visibility of the UI object.
     *  \details The computed visibility of the UI object is used to determine if the object is currently visible.
     *  \return The computed visibility of the UI object. */
    HYP_FORCE_INLINE bool GetComputedVisibility() const
    {
        return m_computedVisibility;
    }

    /*! \brief Check if the UI object is set to enabled or not. Subclasses can use this state to provide custom behavior.
     *  \details The enabled state of the UI object is used to determine if the object is currently enabled.
     *  \return True if the object is enabled, false otherwise. */
    HYP_METHOD(Property = "IsEnabled")
    bool IsEnabled() const;

    /*! \brief Set the enabled state of the UI object.
     *  \details The enabled state of the UI object is used to determine if the object is currently enabled.
     *  \param isEnabled Whether to set the object as enabled or not. */
    HYP_METHOD(Property = "IsEnabled")
    void SetIsEnabled(bool isEnabled);
    
    HYP_FORCE_INLINE const HypData& GetCurrentValue() const
    {
        return m_currentValue;
    }
    
    void SetCurrentValue(HypData&& value, bool triggerEvent = true);

    /*! \brief Check if the UI object has focus. If \ref{includeChildren} is true, also return true if any child objects have focus.
     *  \details The focus state of the UI object is used to determine if the object is currently focused.
     *  \param includeChildren If true, check if any child objects have focus.
     *  \return True if the object has focus, false otherwise. */
    bool HasFocus(bool includeChildren = true) const;

    /*! \brief Check if \ref{other} is either a parent of this object or is equal to the current object.
     *  \details Comparison is performed by using \ref{Node::IsOrHasParent}. If either this or \ref{other} does not have a Node,
     *  false is returned.
     *  \param other The UIObject to check if it is a parent of this object.
     *  \return Whether \ref{other} is a parent of this object or equal to the current object.
     */
    bool IsOrHasParent(const UIObject* other) const;

    /*! \brief Get the parent UIObject to this object, if one exists.
     *  \returns A pointer to the parent UIObject or nullptr if none exists. */
    HYP_METHOD()
    UIObject* GetParentUIObject() const;

    template <class T>
    Handle<T> GetClosestParentUIObject() const
    {
        static_assert(std::is_base_of_v<UIObject, T>, "T must be a subclass of UIObject");

        return ObjCast<T>(GetClosestParentUIObject_Proc([](const UIObject* parent) -> bool
            {
                return parent->IsA<T>();
            }));
    }

    template <class T>
    Handle<T> GetClosestSpawnParent() const
    {
        static_assert(std::is_base_of_v<UIObject, T>, "T must be a subclass of UIObject");

        return ObjCast<T>(GetClosestSpawnParent_Proc([](const UIObject* parent) -> bool
            {
                return parent->IsA<T>();
            }));
    }

    /*! \brief The UIObject that this was spawned from. Not necessarily the parent UIObject that this is attached to in the graph.
     *  \returns A weak reference to the UIObject that this was spawned from. */
    HYP_FORCE_INLINE const WeakHandle<UIObject>& GetSpawnParent() const
    {
        return m_spawnParent;
    }

    HYP_METHOD()
    virtual void AddChildUIObject(const Handle<UIObject>& uiObject);

    HYP_METHOD()
    virtual bool RemoveChildUIObject(UIObject* uiObject);

    /*! \brief Remove all child UIObjects from this object.
     *  \returns The number of child UIObjects removed. */
    virtual int RemoveAllChildUIObjects();

    /*! \brief Remove all child UIObjects from this object that match the predicate.
     *  \param predicate The predicate to match against the child UIObjects.
     *  \returns The number of child UIObjects removed. */
    virtual int RemoveAllChildUIObjects(ProcRef<bool(UIObject*)> predicate);

    /*! \brief Remove all child UIObjects from this object, including all descendants from their parents.
     *  \details This will remove all child UIObjects from this object, including all descendants from their parents.
        Use this primarily when the entire UIObject and all descendants should be freed from memory, and will not be used again.
     *  This is a deep removal, meaning that all child UIObjects and their children will be removed.
     *  \note This does not remove the UIObject itself, only its children. To remove the UIObject itself, use \ref{RemoveFromParent}.
     *  \returns The number of child UIObjects removed. */
    HYP_METHOD()
    virtual void ClearDeep();

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
    virtual Handle<UIObject> DetachFromParent();

    /*! \brief Find a child UIObject by its Name. Checks descendants recursively. If multiple children have the same Name, the first one found is returned.
     *  If no child UIObject with the specified Name is found, nullptr is returned.
     *  \param name The Name of the child UIObject to find.
     *  \param deep If true, search all descendants. If false, only search immediate children.
     *  \return The child UIObject with the specified Name, or nullptr if no child UIObject with the specified Name was found. */
    Handle<UIObject> FindChildUIObject(WeakName name, bool deep = true) const;

    /*! \brief Find a child UIObject by predicate. Checks descendants using breadth-first search. If multiple children match the predicate, the first one found is returned.
     *  If no child UIObject matches the predicate, nullptr is returned.
     *  \param predicate The predicate to match against the child UIObjects.
     *  \param deep If true, search all descendants. If false, only search immediate children.
     *  \return The child UIObject that matches the predicate, or nullptr if no child UIObject matches the predicate. */
    Handle<UIObject> FindChildUIObject(ProcRef<bool(UIObject*)> predicate, bool deep = true) const;

    /*! \brief Check if the UI object has any child UIObjects.
     *  \return True if the object has child UIObjects, false otherwise. */
    HYP_METHOD()
    bool HasChildUIObjects() const;

    /*! \brief Gets the child UIObject at the specified index.
     *  \param index The index of the child UIObject to get.
     *  \return The child UIObject at the specified index. */
    HYP_METHOD()
    Handle<UIObject> GetChildUIObject(int index) const;

    Array<UIObject*> GetChildUIObjects(bool deep) const;

    /*! \brief Gets the relevant script component for this UIObject, if one exists.
     *  The script component is the closest script component to this UIObject in the scene hierarchy, starting from the parent and moving up.
     *  \param deep If set to true, will find the closest parent ScriptComponent if none is attached to this UIObject.
     *  \return A pointer to a ScriptComponent for this UIObject or any of its parents, or nullptr if none exists. */
    ScriptComponent* GetScriptComponent(bool deep = false) const;

    /*! \brief Sets the script component for this UIObject.
     *  Adds the ScriptComponent directly to the UIObject. If the UIObject already has a ScriptComponent, it will be replaced.
     *  \param scriptComponent A ScriptComponent, passed as an rvalue reference. */
    void SetScriptComponent(ScriptComponent&& scriptComponent);

    /*! \brief Removes the script component from this UIObject.
     *  If the UIObject has a script component, it will be removed. Only the script component directly attached to the UIObject will be removed.
     *  Subsequent calls to \ref{GetScriptComponent} will return the closest script component to this UIObject in the scene hierarchy, if one exists. */
    void RemoveScriptComponent();

    HYP_METHOD()
    const Handle<Node>& GetNode() const;

    HYP_METHOD()
    World* GetWorld() const;

    virtual Scene* GetScene() const;

    const Handle<Material>& GetMaterial() const;

    /*! \brief Get the AABB of the UIObject (calculated with absolute positioning, or world space) */
    HYP_METHOD()
    HYP_FORCE_INLINE const BoundingBox& GetAABB() const
    {
        return m_aabb;
    }

    /*! \brief Get the AABB of the UIObject (calculated with absolute positioning, or world space), clamped within the parent's boundary. */
    HYP_METHOD()
    HYP_FORCE_INLINE const BoundingBox& GetAABBClamped() const
    {
        return m_aabbClamped;
    }

    BoundingBox GetWorldAABB() const;
    BoundingBox GetLocalAABB() const;

    const NodeTag& GetNodeTag(WeakName key) const;
    void SetNodeTag(NodeTag&& tag);
    bool HasNodeTag(WeakName key) const;
    bool RemoveNodeTag(WeakName key);

    /*! \brief The default event handler result which is combined with the results of bound event handlers, if the result is equal to OK.
     *  E.g UIButton could return OK if the button was clicked, and the default event handler result could be set to STOP_BUBBLING so that the event does not propagate to objects behind it. */
    virtual UIEventHandlerResult GetDefaultEventHandlerResult() const
    {
        return UIEventHandlerResult();
    }

    virtual void UpdatePosition(bool updateChildren = true);
    virtual void UpdateSize(bool updateChildren = true);

    /*! \brief Set deferred updates to apply to the UI object.
     *  \details Deferred updates are used to defer updates to the UI object until the next Update() call.
     *  \param updateType The type of update to apply.
     *  \param updateChildren If true, also apply the update on all child UIObjects when the deferred update is processed. */
    void SetDeferredUpdate(EnumFlags<UIObjectUpdateType> updateType, bool updateChildren = true);

    void SetUpdatesLocked(EnumFlags<UIObjectUpdateType> updateType, bool locked)
    {
        if (locked)
        {
            m_lockedUpdates |= updateType;
        }
        else
        {
            m_lockedUpdates &= ~updateType;
        }
    }

    /*! \brief Get the focus state of the UI object.
     *  \details The focus state of the UI object is used to determine if the object is currently focused, hovered, pressed, etc.
     *  \return The focus state of the UI object. */
    HYP_FORCE_INLINE EnumFlags<UIObjectFocusState> GetFocusState() const
    {
        return m_focusState;
    }

    /*! \brief Set the focus state of the UI object.
     *  \details The focus state of the UI object is used to determine if the object is currently focused, hovered, pressed, etc.
     *  \param focusState The focus state of the UI object. */
    void SetFocusState(EnumFlags<UIObjectFocusState> focusState);

    /*! \brief Collect all nested UIObjects in the hierarchy, calling `proc` for each collected UIObject.
     *  \param proc The function to call for each collected UIObject.
     *  \param onlyVisible If true, skips objects with computed visibility as non-visible */
    void CollectObjects(ProcRef<void(UIObject*)> proc, bool onlyVisible = true) const;

    /*! \brief Collect all nested UIObjects in the hierarchy and push them to the `outObjects` array.
     *  \param outObjects The array to store the collected UIObjects in.
     *  \param onlyVisible If true, skips objects with computed visibility as non-visible */
    void CollectObjects(Array<UIObject*>& outObjects, bool onlyVisible = true) const;

    /*! \brief Transform a screen coordinate to a relative coordinate within the UIObject.
     *  \param coords The screen coordinates to transform.
     *  \return The relative coordinates within the UIObject. */
    Vec2f TransformScreenCoordsToRelative(Vec2i coords) const;
    
    /*! \brief Does this object allow the material to be updated?
     *  If true, a dynamic material will be created for this object. */
    bool AllowMaterialUpdate() const
    {
        return m_allowMaterialUpdate;
    }
    
    void SetAllowMaterialUpdate(bool allowMaterialUpdate);

    /*! \brief Get the data source associated with this UIObject. The data source is used to populate the UIObject with data.
     *  \return The data source associated with this UIObject. */
    HYP_METHOD(Property = "DataSource")
    HYP_FORCE_INLINE const Handle<UIDataSourceBase>& GetDataSource() const
    {
        return m_dataSource;
    }

    /*! \brief Set the data source associated with this UIObject. The data source is used to populate the UIObject with data.
     *  \param dataSource The data source to associate with this UIObject. */
    HYP_METHOD(Property = "DataSource")
    void SetDataSource(const Handle<UIDataSourceBase>& dataSource);

    /*! \brief Gets the UUID of the associated data source element (if applicable).
     *  Otherwise, returns an empty UUID.
     *
     * \return The UUID of the associated data source element. */
    HYP_FORCE_INLINE UUID GetDataSourceElementUUID() const
    {
        return m_dataSourceElementUuid;
    }

    /*! \brief Sets the UUID of the associated data source element.
     *  \internal This is used by a parent UIObject to set the UUID of the associated data source element.
     *
     *  \param dataSourceElementUuid The UUID of the associated data source element. */
    HYP_FORCE_INLINE void SetDataSourceElementUUID(UUID dataSourceElementUuid)
    {
        m_dataSourceElementUuid = dataSourceElementUuid;
    }

    /*! \internal */
    void ForEachChildUIObject_Proc(ProcRef<IterationResult(UIObject*)> proc, bool deep = true) const;
    
    /*! \brief Spawn a new UIObject with the given HypClass \ref{hypClass}. The object will not be attached to the current UIStage.
     *  The object will not be named. To name the object, use the other CreateUIObject overload.
     *
     *  \param hypClass The HypClass associated with the UIObject type you wish to spawn.
     *  \param name The name of the UIObject.
     *  \param position The position of the UI object.
     *  \param size The size of the UI object.
     *  \return A handle to the created UI object. */
    HYP_NODISCARD Handle<UIObject> CreateUIObject(const HypClass* hypClass, Name name, Vec2i position, UIObjectSize size);

    /*! \brief Spawn a new UIObject of type T. The object will not be attached to the current UIStage.
     *
     *  \tparam T The type of UI object to create. Must be a derived class of UIObject.
     *  \return A handle to the created UI object. */
    template <class T>
    HYP_NODISCARD Handle<T> CreateUIObject()
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
    HYP_NODISCARD Handle<T> CreateUIObject(Vec2i position, UIObjectSize size)
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
    HYP_NODISCARD Handle<T> CreateUIObject(Name name, Vec2i position, UIObjectSize size)
    {
        AssertOnOwnerThread();

        Assert(GetNode().IsValid());

        if (!name.IsValid())
        {
            name = Name::Unique(ANSIString("Unnamed_") + TypeNameHelper<T, true>::value.Data());
        }

        Handle<Entity> entity = CreateObject<Entity>();
        entity->SetName(name);

        // Set it to ignore parent scale so size of the UI object is not affected by the parent
        entity->SetFlags(entity->GetFlags() | NodeFlags::IGNORE_PARENT_SCALE);

        Handle<UIObject> uiObject = CreateUIObjectInternal<T>(name, entity, false /* init */);
        uiObject->SetPosition(position);
        uiObject->SetSize(size);

        InitObject(uiObject);

        Handle<T> result = ObjCast<T>(uiObject);
        Assert(result.IsValid(), "Failed to cast created UIObject to the requested type");

        return result;
    }

    // Events
    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult> OnInit;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult> OnAttached;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult> OnRemoved;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, UIObject*> OnChildAttached;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, UIObject*> OnChildRemoved;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, const MouseEvent&> OnMouseDown;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, const MouseEvent&> OnMouseUp;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, const MouseEvent&> OnMouseDrag;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, const MouseEvent&> OnMouseHover;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, const MouseEvent&> OnMouseLeave;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, const MouseEvent&> OnMouseMove;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, const MouseEvent&> OnGainFocus;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, const MouseEvent&> OnLoseFocus;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, const MouseEvent&> OnScroll;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, const MouseEvent&> OnClick;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, const MouseEvent&> OnRightClick;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, const KeyboardEvent&> OnKeyDown;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, const KeyboardEvent&> OnKeyUp;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, const String&> OnTextChange;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult> OnSizeChange;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult> OnComputedVisibilityChange;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult> OnEnabled;

    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult> OnDisabled;
    
    HYP_FIELD()
    ScriptableDelegate<UIEventHandlerResult, const HypData&> OnValueChange;

protected:
    HYP_METHOD()
    virtual void Init();

    HYP_FORCE_INLINE void AssertOnOwnerThread() const
    {
        if (Scene* scene = GetScene())
        {
            Threads::AssertOnThread(scene->GetOwnerThreadId());
        }
    }

    Handle<UIObject> GetClosestParentUIObject_Proc(const ProcRef<bool(UIObject*)>& proc) const;
    Handle<UIObject> GetClosestSpawnParent_Proc(const ProcRef<bool(UIObject*)>& proc) const;

    virtual void UpdateSize_Internal(bool updateChildren);

    virtual void SetDataSource_Internal(UIDataSourceBase* dataSource);

    virtual void SetFocusState_Internal(EnumFlags<UIObjectFocusState> focusState);

    virtual void Update_Internal(float delta);

    virtual void OnAttached_Internal(UIObject* parent);
    virtual void OnRemoved_Internal();

    /*! \brief Check if the object has been computed as visible or not. E.g scrolled out of view in a parent container */
    virtual void UpdateComputedVisibility(bool updateChildren = true);

    void UpdateComputedDepth(bool updateChildren = true);

    void UpdateComputedTextSize();

    /*! \brief Sets the Handle<Node> for this UIObject.
     *  \internal To be called internally from UIStage */
    void SetNodeProxy(Handle<Node>);

    /*! \brief Get the shared quad mesh used for rendering UI objects. Vertices are in range: 0..1, with the origin at the top-left corner.
     *
     * \return The shared quad mesh
     */
    static Handle<Mesh> GetQuadMesh();

    /*! \brief Calculate the local (object space) bounding box (in pixels) of this object.
     *  should be in range of (0,0,0):(size,size,size). */
    BoundingBox CalculateAABB() const;

    virtual BoundingBox CalculateInnerAABB_Internal() const;

    const Handle<Mesh>& GetMesh() const;

    virtual MaterialAttributes GetMaterialAttributes() const;
    virtual Material::ParameterTable GetMaterialParameters() const;
    virtual Material::TextureSet GetMaterialTextures() const;

    Vec2f GetParentScrollOffset() const;

    /*! \brief Add a DelegateHandler to be removed when the object is destructed */
    void AddDelegateHandler(DelegateHandler&& delegateHandler)
    {
        m_delegateHandlers.PushBack(std::move(delegateHandler));
    }

    void UpdateMeshData(bool updateChildren = true);
    virtual void UpdateMeshData_Internal();

    void UpdateMaterial(bool updateChildren = true);
    virtual void UpdateMaterial_Internal();

    Array<UIObject*> FilterChildUIObjects(ProcRef<bool(UIObject*)> predicate, bool deep) const;

    virtual void SetStage_Internal(UIStage* stage);

    void OnFontAtlasUpdate();

    virtual void OnFontAtlasUpdate_Internal()
    {
    }

    void OnTextSizeUpdate();

    virtual void OnTextSizeUpdate_Internal()
    {
    }

    void OnScrollOffsetUpdate(Vec2f delta);

    virtual void OnScrollOffsetUpdate_Internal(Vec2f delta)
    {
    }

    UIStage* m_stage;
    WeakHandle<UIObject> m_spawnParent;

    Name m_name;

    Vec2i m_position;
    Vec2f m_offsetPosition;

    UIObjectSize m_size;
    Vec2i m_actualSize;
    Vec2i m_actualSizeClamped;

    UIObjectSize m_innerSize;
    Vec2i m_actualInnerSize;

    UIObjectSize m_maxSize;
    Vec2i m_actualMaxSize;

    BoundingBox m_aabb;
    BoundingBox m_aabbClamped;

    Handle<UIObject> m_verticalScrollbar;
    Handle<UIObject> m_horizontalScrollbar;

    Vec2i m_padding;

    int m_depth; // manually set depth; otherwise defaults to the node's depth in the scene

    uint32 m_borderRadius;
    EnumFlags<UIObjectBorderFlags> m_borderFlags;

    BlendVar<Vec2f> m_scrollOffset;

    UIObjectAlignment m_originAlignment;
    UIObjectAlignment m_parentAlignment;

    UIObjectAspectRatio m_aspectRatio;

    Color m_backgroundColor;
    Color m_textColor;

    String m_text;

    Handle<UIDataSourceBase> m_dataSource;
    DelegateHandler m_dataSourceOnChangeHandler;
    DelegateHandler m_dataSourceOnElementAddHandler;
    DelegateHandler m_dataSourceOnElementRemoveHandler;
    DelegateHandler m_dataSourceOnElementUpdateHandler;

    UUID m_dataSourceElementUuid;

    EnumFlags<UIObjectUpdateType> m_deferredUpdates;
    EnumFlags<UIObjectUpdateType> m_lockedUpdates;
    
    HypData m_currentValue;

private:
    template <class T>
    Handle<UIObject> CreateUIObjectInternal(Name name, const Handle<Entity>& entity, bool init = false)
    {
        Assert(entity != nullptr);

        static_assert(std::is_base_of_v<UIObject, T>, "T must be a derived class of UIObject");

        Handle<UIObject> uiObject = CreateObject<T>();

        UIStage* stage = IsA<UIStage>() ? ObjCast<UIStage>(this) : GetStage();

        uiObject->m_spawnParent = WeakHandleFromThis();
        uiObject->m_stage = stage;

        // Small optimization so UpdateComputedTextSize() doesn't need to be called when attached.
        // Computed text size is based on spawn parent rather than the node it's attached to,
        // so we can set it right here.
        uiObject->m_computedTextSize = m_computedTextSize;

        uiObject->SetNodeProxy(entity);
        uiObject->SetName(name);

        if (init)
        {
            InitObject(uiObject);

            uiObject->SetStage_Internal(stage);
        }

        return uiObject;
    }

    void ComputeOffsetPosition();

    void UpdateActualSizes(UpdateSizePhase phase, EnumFlags<UIObjectUpdateSizeFlags> flags);
    virtual void ComputeActualSize(const UIObjectSize& size, Vec2i& actualSize, UpdateSizePhase phase, bool isInner = false);

    template <class Lambda>
    void ForEachChildUIObject(Lambda&& lambda, bool deep = true) const;

    template <class Lambda>
    void ForEachParentUIObject(Lambda&& lambda) const;

    void SetEntityAABB(const BoundingBox& aabb);

    void UpdateClampedSize(bool updateChildren = true);

    void UpdateNodeTransform();

    Handle<Material> CreateMaterial() const;

    EnumFlags<UIObjectFocusState> m_focusState;

    bool m_isVisible : 1;
    bool m_computedVisibility : 1;
    bool m_isEnabled : 1;
    bool m_acceptsFocus : 1;
    bool m_affectsParentSize : 1;
    bool m_isPositionAbsolute : 1;
    bool m_allowMaterialUpdate : 1;

    int m_computedDepth;

    float m_textSize;
    float m_computedTextSize;

    Handle<Node> m_node;

    Array<Handle<UIObject>> m_childUiObjects;

    Array<DelegateHandler> m_delegateHandlers;
};

#pragma endregion UIObject

#pragma region UILockedUpdatesScope

struct UILockedUpdatesScope
{
    UILockedUpdatesScope(UIObject& uiObject, EnumFlags<UIObjectUpdateType> value)
        : uiObject(uiObject),
          value(value & ~uiObject.m_lockedUpdates)
    {
        uiObject.m_lockedUpdates |= this->value;
    }

    UILockedUpdatesScope(const UILockedUpdatesScope& other) = delete;
    UILockedUpdatesScope& operator=(const UILockedUpdatesScope& other) = delete;

    UILockedUpdatesScope(UILockedUpdatesScope&& other) noexcept = delete;
    UILockedUpdatesScope& operator=(UILockedUpdatesScope&& other) noexcept = delete;

    ~UILockedUpdatesScope()
    {
        uiObject.m_lockedUpdates &= ~value;
    }

    UIObject& uiObject;
    EnumFlags<UIObjectUpdateType> value;
};

#pragma endregion UILockedUpdatesScope

} // namespace hyperion

