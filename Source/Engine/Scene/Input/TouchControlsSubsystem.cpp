/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Input/TouchControlsSubsystem.hpp>
#include <Input/InputManager.hpp>

#include <UI/UIStage.hpp>
#include <UI/UIObject.hpp>
#include <UI/UIPanel.hpp>
#include <UI/UIImage.hpp>
#include <UI/UISubsystem.hpp>

#include <Scene/World.hpp>
#include <Scene/Scene.hpp>

#include <System/AppContext.hpp>

#include <Rendering/Material.hpp>
#include <Rendering/Texture.hpp>

#include <Asset/Assets.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/EngineGlobals.hpp>

#include <Core/Math/MathUtil.hpp>

#include <TouchControlsSubsystem.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Input);

TouchControlsSubsystem::TouchControlsSubsystem()
    : Subsystem(),
      m_enabled(true),
      m_joystickSize(120.0f),
      m_knobSize(50.0f),
      m_deadzone(0.1f),
      m_maxJoystickRadius(40.0f),
      m_lookSensitivity(2.0f),
      m_isInitialized(false),
      m_leftTouchId(-1),
      m_rightTouchId(-1)
{
}

TouchControlsSubsystem::~TouchControlsSubsystem()
{
    DestroyJoystickUI();
}

void TouchControlsSubsystem::OnAddedToWorld()
{
    CreateJoystickUI();
}

void TouchControlsSubsystem::OnRemovedFromWorld()
{
    DestroyJoystickUI();
}

void TouchControlsSubsystem::OnSceneAttached(const Handle<Scene>& scene)
{
    // Recreate joystick UI when scene changes
    DestroyJoystickUI();
    CreateJoystickUI();
}

void TouchControlsSubsystem::OnSceneDetached(Scene* scene)
{
    DestroyJoystickUI();
}

void TouchControlsSubsystem::CreateJoystickUI()
{
    if (m_isInitialized)
    {
        return;
    }

    World* world = GetWorld();
    if (!world)
    {
        return;
    }

    UISubsystem* uiSubsystem = world->GetSubsystem<UISubsystem>();
    if (!uiSubsystem)
    {
        HYP_LOG(Input, Warning, "TouchControlsSubsystem: No UISubsystem found, cannot create joystick UI");
        return;
    }

    Handle<UIStage> uiStage = uiSubsystem->GetUIStage();
    if (!uiStage.IsValid())
    {
        HYP_LOG(Input, Warning, "TouchControlsSubsystem: UIStage is not valid");
        return;
    }

    m_uiStage = uiStage;

    // Create joystick base (outer circle)
    m_joystickBase = uiStage->CreateUIObject<UIPanel>(
        NAME("TouchJoystick_Base"),
        Vec2i { 50, 50 },
        UIObjectSize({ static_cast<int32>(m_joystickSize), UIObjectSize::PIXEL },
                     { static_cast<int32>(m_joystickSize), UIObjectSize::PIXEL })
    );

    if (m_joystickBase.IsValid())
    {
        m_joystickBase->SetIsPositionAbsolute(true);
        m_joystickBase->SetIsVisible(false);
        m_joystickBase->SetDepth(100);  // Base layer
        m_joystickBase->SetBackgroundColor(Color(0.12f, 0.12f, 0.14f, 0.35f));
        m_joystickBase->SetBorderRadius(static_cast<uint32>(m_joystickSize * 0.5f));
        m_joystickBase->SetBorderFlags(UIObjectBorderFlags::ALL);

        uiStage->AddChildUIObject(m_joystickBase);

        const float shadowSize = m_knobSize;
        m_joystickShadow = m_joystickBase->CreateUIObject<UIPanel>(
            NAME("TouchJoystick_Shadow"),
            Vec2i { 0, 0 },
            UIObjectSize({ static_cast<int32>(shadowSize), UIObjectSize::PIXEL },
                         { static_cast<int32>(shadowSize), UIObjectSize::PIXEL })
        );

        if (m_joystickShadow.IsValid())
        {
            m_joystickShadow->SetDepth(101);  // Above base, below knob
            m_joystickShadow->SetBackgroundColor(Color(0.0f, 0.0f, 0.0f, 0.4f));
            m_joystickShadow->SetBorderRadius(static_cast<uint32>(shadowSize * 0.5f));
            m_joystickShadow->SetBorderFlags(UIObjectBorderFlags::ALL);

            m_joystickBase->AddChildUIObject(m_joystickShadow);
        }

        // Create the knob (inner circle)
        m_joystickKnob = m_joystickBase->CreateUIObject<UIPanel>(
            NAME("TouchJoystick_Knob"),
            Vec2i { 0, 0 },
            UIObjectSize({ static_cast<int32>(m_knobSize), UIObjectSize::PIXEL },
                         { static_cast<int32>(m_knobSize), UIObjectSize::PIXEL })
        );

        if (m_joystickKnob.IsValid())
        {
            m_joystickKnob->SetDepth(102);  // Top layer
            m_joystickKnob->SetBorderRadius(static_cast<uint32>(m_knobSize * 0.5f));
            m_joystickKnob->SetBorderFlags(UIObjectBorderFlags::ALL);
            m_joystickKnob->SetAllowMaterialUpdate(true);

            m_joystickBase->AddChildUIObject(m_joystickKnob);

            UpdateKnobAppearance(Vec2f::Zero());
        }
    }

    m_isInitialized = true;
}

void TouchControlsSubsystem::DestroyJoystickUI()
{
    if (m_joystickKnob.IsValid())
    {
        m_joystickKnob->RemoveFromParent();
        m_joystickKnob.Reset();
    }

    if (m_joystickShadow.IsValid())
    {
        m_joystickShadow->RemoveFromParent();
        m_joystickShadow.Reset();
    }

    if (m_joystickBase.IsValid())
    {
        m_joystickBase->RemoveFromParent();
        m_joystickBase.Reset();
    }

    m_isInitialized = false;
}

void TouchControlsSubsystem::PreUpdate(float delta)
{
}

void TouchControlsSubsystem::Update(float delta)
{
    if (!m_enabled || !m_isInitialized)
    {
        return;
    }

    // Update active touches and compute deltas
    UpdateActiveTouches(delta);

    // Update joystick visuals
    UpdateJoystickVisuals();
}

void TouchControlsSubsystem::ProcessTouchEvent(const TouchEvent& touchEvent)
{
    if (!m_enabled)
    {
        return;
    }

    const int32 pointerId = touchEvent.pointerId;

    // Convert from physical to logical coordinates using content scale factor
    float contentScaleFactor = 1.0f;
    if (touchEvent.baseEvent && touchEvent.baseEvent->GetWindow())
    {
        contentScaleFactor = touchEvent.baseEvent->GetWindow()->GetContentScaleFactor();
    }

    const Vec2f position = Vec2f(touchEvent.position) / contentScaleFactor;

    const EventType eventType = touchEvent.baseEvent ? touchEvent.baseEvent->GetType() : EventType::INVALID;

    switch (eventType)
    {
    case EventType::TOUCH_DOWN:
    {
        bool isLeftSide = IsLeftSideOfScreen(position);

        TouchPoint touchPoint;
        touchPoint.pointerId = pointerId;
        touchPoint.position = position;
        touchPoint.startPosition = position;
        touchPoint.isActive = true;
        touchPoint.isLeftSide = isLeftSide;

        m_activeTouches[pointerId] = touchPoint;

        // Track primary touches for each side
        if (isLeftSide && m_leftTouchId == -1)
        {
            m_leftTouchId = pointerId;

            if (m_joystickBase.IsValid())
            {
                // Position joystick base centered on touch
                m_joystickBase->SetPosition(Vec2i(
                    static_cast<int32>(position.x - m_joystickSize * 0.5f),
                    static_cast<int32>(position.y - m_joystickSize * 0.5f)
                ));
                m_joystickBase->SetIsVisible(true);

                // Center knob and shadow in base initially
                if (m_joystickKnob.IsValid())
                {
                    m_joystickKnob->SetPosition(Vec2i(
                        static_cast<int32>((m_joystickSize - m_knobSize) * 0.5f),
                        static_cast<int32>((m_joystickSize - m_knobSize) * 0.5f)
                    ));
                }
                if (m_joystickShadow.IsValid())
                {
                    m_joystickShadow->SetPosition(Vec2i(
                        static_cast<int32>((m_joystickSize - (m_knobSize + 8.0f)) * 0.5f),
                        static_cast<int32>((m_joystickSize - (m_knobSize + 8.0f)) * 0.5f)
                    ));
                }
            }
        }
        else if (!isLeftSide && m_rightTouchId == -1)
        {
            m_rightTouchId = pointerId;
        }

        break;
    }

    case EventType::TOUCH_UP:
    {
        auto it = m_activeTouches.Find(pointerId);
        if (it != m_activeTouches.End())
        {
            if (pointerId == m_leftTouchId)
            {
                m_leftTouchId = -1;
                m_movementDelta = Vec2f::Zero();

                // Hide joystick
                if (m_joystickBase.IsValid())
                {
                    m_joystickBase->SetIsVisible(false);
                }
            }
            else if (pointerId == m_rightTouchId)
            {
                m_rightTouchId = -1;
                m_lookDelta = Vec2f::Zero();
            }

            m_activeTouches.Erase(it);
        }

        break;
    }

    case EventType::TOUCH_MOVE:
    {
        auto it = m_activeTouches.Find(pointerId);
        if (it != m_activeTouches.End())
        {
            it->second.position = position;
        }

        break;
    }

    default:
        break;
    }
}

void TouchControlsSubsystem::UpdateActiveTouches(float delta)
{
    // Calculate movement from left side touch
    if (m_leftTouchId != -1)
    {
        auto it = m_activeTouches.Find(m_leftTouchId);
        if (it != m_activeTouches.End())
        {
            const TouchPoint& touch = it->second;
            Vec2f delta = touch.position - touch.startPosition;
            m_movementDelta = NormalizeJoystickInput(delta);
        }
        else
        {
            m_movementDelta = Vec2f::Zero();
        }
    }
    else
    {
        m_movementDelta = Vec2f::Zero();
    }

    // Calculate look from right side touch
    if (m_rightTouchId != -1)
    {
        auto it = m_activeTouches.Find(m_rightTouchId);
        if (it != m_activeTouches.End())
        {
            const TouchPoint& touch = it->second;

            // For look, we use delta from previous frame
            // Store previous position and calculate delta
            static Vec2f prevRightPosition = Vec2f::Zero();

            if (prevRightPosition.IsZero())
            {
                prevRightPosition = touch.startPosition;
            }

            Vec2f lookDelta = touch.position - prevRightPosition;
            prevRightPosition = touch.position;

            // Normalize to -1 to 1 range based on screen size
            const Vec2f screenSize = Vec2f(m_uiStage->GetSurfaceSize());
            if (!screenSize.IsZero())
            {
                m_lookDelta = Vec2f(
                    lookDelta.x / screenSize.x * m_lookSensitivity,
                    lookDelta.y / screenSize.y * m_lookSensitivity
                );
            }
        }
        else
        {
            m_lookDelta = Vec2f::Zero();
        }
    }
    else
    {
        m_lookDelta = Vec2f::Zero();
        // Reset the static prev position when touch is released
        static Vec2f prevRightPosition = Vec2f::Zero();
        prevRightPosition = Vec2f::Zero();
    }
}

void TouchControlsSubsystem::UpdateJoystickVisuals()
{
    if (!m_joystickBase.IsValid() || !m_joystickKnob.IsValid())
    {
        return;
    }

    if (m_leftTouchId == -1)
    {
        return;
    }

    auto it = m_activeTouches.Find(m_leftTouchId);
    if (it == m_activeTouches.End())
    {
        return;
    }

    const TouchPoint& touch = it->second;
    Vec2f delta = touch.position - touch.startPosition;

    // Clamp delta to max radius so knob stays within base circle
    float distance = delta.Length();
    if (distance > m_maxJoystickRadius)
    {
        delta = delta / distance * m_maxJoystickRadius;
    }

    // Calculate normalized delta (-1 to 1 range) for lighting
    Vec2f normalizedDelta = Vec2f::Zero();
    if (m_maxJoystickRadius > 0.0f)
    {
        normalizedDelta = delta / m_maxJoystickRadius;
    }

    // Position knob: base center + delta - half knob size to center it
    Vec2f knobPos(
        (m_joystickSize - m_knobSize) * 0.5f + delta.x,
        (m_joystickSize - m_knobSize) * 0.5f + delta.y
    );

    m_joystickKnob->SetPosition(Vec2i(
        static_cast<int32>(knobPos.x),
        static_cast<int32>(knobPos.y)
    ));

    // Position shadow slightly offset in opposite direction of movement (fake depth)
    if (m_joystickShadow.IsValid())
    {
        const float shadowOffset = 3.0f;
        Vec2f shadowPos(
            (m_joystickSize - (m_knobSize + 8.0f)) * 0.5f + delta.x - normalizedDelta.x * shadowOffset,
            (m_joystickSize - (m_knobSize + 8.0f)) * 0.5f + delta.y - normalizedDelta.y * shadowOffset
        );
        m_joystickShadow->SetPosition(Vec2i(
            static_cast<int32>(shadowPos.x),
            static_cast<int32>(shadowPos.y)
        ));
    }

    // Update knob appearance with dynamic lighting
    UpdateKnobAppearance(normalizedDelta);
}

void TouchControlsSubsystem::UpdateKnobAppearance(const Vec2f& normalizedDelta)
{
    if (!m_joystickKnob.IsValid())
    {
        return;
    }

    // Simulate 3D lighting effect based on movement direction
    // Light appears to come from top-left, so when knob moves:
    // - Moving right: left side gets lighter (facing light)
    // - Moving down: top gets lighter
    // - etc.

    float lightIntensity = 0.85f;  // Base brightness
    float highlightStrength = 0.15f;  // How much the light varies

    // Calculate lighting shift based on direction
    // Moving "into" the light makes it brighter, "away" makes it darker
    float lightShift = (normalizedDelta.x * -0.3f + normalizedDelta.y * -0.5f) * highlightStrength;

    float r = MathUtil::Clamp(lightIntensity + lightShift, 0.6f, 1.0f);
    float g = MathUtil::Clamp(lightIntensity + lightShift, 0.6f, 1.0f);
    float b = MathUtil::Clamp(lightIntensity + lightShift * 0.8f, 0.6f, 1.0f);  // Slightly blue-tinted shadow

    m_joystickKnob->SetBackgroundColor(Color(r, g, b, 0.95f));
}

Vec2f TouchControlsSubsystem::GetMovementDelta() const
{
    return m_movementDelta;
}

Vec2f TouchControlsSubsystem::GetLookDelta() const
{
    return m_lookDelta;
}

bool TouchControlsSubsystem::IsActive() const
{
    return m_leftTouchId != -1 || m_rightTouchId != -1;
}

void TouchControlsSubsystem::SetEnabled(bool enabled)
{
    m_enabled = enabled;

    if (!enabled)
    {
        // Hide joystick when disabled
        if (m_joystickBase.IsValid())
        {
            m_joystickBase->SetIsVisible(false);
        }

        // Reset all touch state
        m_activeTouches.Clear();
        m_leftTouchId = -1;
        m_rightTouchId = -1;
        m_movementDelta = Vec2f::Zero();
        m_lookDelta = Vec2f::Zero();
    }
}

void TouchControlsSubsystem::SetJoystickSize(float size)
{
    m_joystickSize = size;

    if (m_joystickBase.IsValid())
    {
        m_joystickBase->SetSize(UIObjectSize(
            { static_cast<int32>(size), UIObjectSize::PIXEL },
            { static_cast<int32>(size), UIObjectSize::PIXEL }
        ));
    }
}

void TouchControlsSubsystem::SetDeadzone(float deadzone)
{
    m_deadzone = MathUtil::Clamp(deadzone, 0.0f, 1.0f);
}

bool TouchControlsSubsystem::IsLeftSideOfScreen(const Vec2f& position) const
{
    if (!m_uiStage.IsValid())
    {
        return position.x < 540.0f;
    }

    const Vec2i surfaceSize = m_uiStage->GetSurfaceSize();
    return position.x < float(surfaceSize.x) * 0.5f;
}

Vec2f TouchControlsSubsystem::NormalizeJoystickInput(const Vec2f& delta) const
{
    if (delta.IsZero())
    {
        return Vec2f::Zero();
    }

    float maxDist = m_maxJoystickRadius;
    float distance = delta.Length();

    // Normalize to 0-1 range
    float normalizedDist = MathUtil::Min(distance / maxDist, 1.0f);

    // Apply deadzone
    if (normalizedDist < m_deadzone)
    {
        return Vec2f::Zero();
    }

    // Recalculate with deadzone
    normalizedDist = (normalizedDist - m_deadzone) / (1.0f - m_deadzone);

    // Return normalized vector
    Vec2f direction = delta / distance;
    return direction * normalizedDist;
}

bool TouchControlsSubsystem::GetTouchPoint(int32 pointerId, TouchPoint& outTouchPoint) const
{
    auto it = m_activeTouches.Find(pointerId);
    if (it != m_activeTouches.End())
    {
        outTouchPoint = it->second;
        return true;
    }
    return false;
}

bool TouchControlsSubsystem::IsTouchLeftSide(int32 pointerId) const
{
    auto it = m_activeTouches.Find(pointerId);
    if (it != m_activeTouches.End())
    {
        return it->second.isLeftSide;
    }
    return false;
}

} // namespace Hyperion
