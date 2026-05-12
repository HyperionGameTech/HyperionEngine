/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <scene/input/TouchControlsSubsystem.hpp>
#include <input/InputManager.hpp>

#include <ui/UIStage.hpp>
#include <ui/UIObject.hpp>
#include <ui/UIPanel.hpp>
#include <ui/UIImage.hpp>
#include <ui/UISubsystem.hpp>

#include <scene/World.hpp>
#include <scene/Scene.hpp>

#include <system/AppContext.hpp>

#include <rendering/MaterialInstance.hpp>
#include <rendering/MaterialDefinition.hpp>
#include <rendering/Texture.hpp>

#include <asset/Assets.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/EngineGlobals.hpp>

#include <Core/math/MathUtil.hpp>

#include <TouchControlsSubsystem.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Input);

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

void TouchControlsSubsystem::Init()
{
    Subsystem::Init();

    HYP_LOG(Input, Info, "TouchControlsSubsystem initialized");
}

void TouchControlsSubsystem::OnAddedToWorld()
{
    HYP_LOG(Input, Info, "TouchControlsSubsystem added to world");

    // Create joystick UI if UISubsystem exists
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

    // Create joystick base (semi-transparent circle background)
    m_joystickBase = uiStage->CreateUIObject<UIPanel>(
        NAME("TouchJoystick_Base"),
        Vec2i { 50, 50 },  // Initial position, will be hidden until touch
        UIObjectSize({ static_cast<int32>(m_joystickSize), UIObjectSize::PIXEL },
                     { static_cast<int32>(m_joystickSize), UIObjectSize::PIXEL })
    );

    if (m_joystickBase.IsValid())
    {
        // Add the base to the UIStage
        uiStage->AddChildUIObject(m_joystickBase);

        // Hidden until touched
        m_joystickBase->SetIsVisible(false);

        // Create the knob (inner circle that moves)
        m_joystickKnob = m_joystickBase->CreateUIObject<UIPanel>(
            NAME("TouchJoystick_Knob"),
            Vec2i { 0, 0 },  // Centered in parent
            UIObjectSize({ static_cast<int32>(m_knobSize), UIObjectSize::PIXEL },
                         { static_cast<int32>(m_knobSize), UIObjectSize::PIXEL })
        );

        if (m_joystickKnob.IsValid())
        {
            // Add the knob to the base
            m_joystickBase->AddChildUIObject(m_joystickKnob);
        }

        HYP_LOG(Input, Info, "TouchControlsSubsystem: Created joystick UI");
    }

    m_isInitialized = true;

    // Get screen size immediately
    if (g_appContext.IsValid() && g_appContext->GetMainWindow() != nullptr)
    {
        m_screenSize = Vec2f(g_appContext->GetMainWindow()->GetSize());
        HYP_LOG(Input, Info, "TouchControlsSubsystem: Screen size initialized to ({}, {})", m_screenSize.x, m_screenSize.y);
    }
    else
    {
        HYP_LOG(Input, Warning, "TouchControlsSubsystem: Could not get screen size during initialization");
    }
}

void TouchControlsSubsystem::DestroyJoystickUI()
{
    if (m_joystickKnob.IsValid())
    {
        m_joystickKnob->RemoveFromParent();
        m_joystickKnob.Reset();
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

    // Update screen size in case of resize
    if (g_appContext.IsValid() && g_appContext->GetMainWindow() != nullptr)
    {
        m_screenSize = Vec2f(g_appContext->GetMainWindow()->GetSize());
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
    const Vec2f position = touchEvent.position;

    const EventType eventType = touchEvent.baseEvent ? touchEvent.baseEvent->GetType() : EventType::INVALID;

    switch (eventType)
    {
    case EventType::TOUCH_DOWN:
    {
        HYP_LOG(Input, Debug, "TouchControlsSubsystem: TOUCH_DOWN pointerId={}, position=({}, {}), isLeftSide={}", 
            pointerId, position.x, position.y, IsLeftSideOfScreen(position));
        
        // Determine if left or right side of screen
        bool isLeftSide = IsLeftSideOfScreen(position);

        TouchPoint touchPoint;
        touchPoint.pointerId = pointerId;
        touchPoint.position = position;
        touchPoint.startPosition = position;
        touchPoint.isActive = true;
        touchPoint.isLeftSide = isLeftSide;

        m_activeTouches[pointerId] = touchPoint;

        // Only set as primary touch for a side if there isn't one already
        // This allows multiple touches on same side without overwriting
        if (isLeftSide)
        {
            if (m_leftTouchId == -1)
            {
                m_leftTouchId = pointerId;

                HYP_LOG(Input, Debug, "TouchControlsSubsystem: Left touch started, showing joystick at ({}, {})", position.x, position.y);
                
                // Show joystick at touch position
                if (m_joystickBase.IsValid())
                {
                    m_joystickBase->SetPosition(Vec2i(
                        static_cast<int32>(position.x - m_joystickSize * 0.5f),
                        static_cast<int32>(position.y - m_joystickSize * 0.5f)
                    ));
                    m_joystickBase->SetIsVisible(true);
                    HYP_LOG(Input, Debug, "TouchControlsSubsystem: Joystick base shown at ({}, {})", 
                        m_joystickBase->GetPosition().x, m_joystickBase->GetPosition().y);

                    // Center the knob
                    if (m_joystickKnob.IsValid())
                    {
                        m_joystickKnob->SetPosition(Vec2i(
                            static_cast<int32>((m_joystickSize - m_knobSize) * 0.5f),
                            static_cast<int32>((m_joystickSize - m_knobSize) * 0.5f)
                        ));
                    }
                }
            }
            else
            {
                HYP_LOG(Input, Debug, "TouchControlsSubsystem: Additional left touch ignored, already have primary at pointerId={}", m_leftTouchId);
            }
        }
        else // Right side
        {
            if (m_rightTouchId == -1)
            {
                m_rightTouchId = pointerId;
                HYP_LOG(Input, Debug, "TouchControlsSubsystem: Right touch started for look, pointerId={}", pointerId);
            }
            else
            {
                HYP_LOG(Input, Debug, "TouchControlsSubsystem: Additional right touch ignored, already have primary at pointerId={}", m_rightTouchId);
            }
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
            if (!m_screenSize.IsZero())
            {
                m_lookDelta = Vec2f(
                    lookDelta.x / m_screenSize.x * m_lookSensitivity,
                    lookDelta.y / m_screenSize.y * m_lookSensitivity
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

    // Clamp to max radius
    float distance = delta.Length();
    if (distance > m_maxJoystickRadius)
    {
        delta = delta / distance * m_maxJoystickRadius;
    }

    // Update knob position relative to base center
    Vec2f baseCenter(m_joystickSize * 0.5f, m_joystickSize * 0.5f);
    Vec2f knobCenter(m_knobSize * 0.5f, m_knobSize * 0.5f);
    Vec2f knobPos = baseCenter + delta - knobCenter;

    m_joystickKnob->SetPosition(Vec2i(
        static_cast<int32>(knobPos.x),
        static_cast<int32>(knobPos.y)
    ));
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
    // If screen size is not known yet, assume standard width and log warning
    if (m_screenSize.x <= 0.0f)
    {
        HYP_LOG(Input, Warning, "TouchControlsSubsystem: Screen size not initialized, assuming 1080px width");
        return position.x < 540.0f; // Assume 1080px screen, left half is < 540
    }
    
    return position.x < m_screenSize.x * 0.5f;
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
