/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Steam/SteamInput.hpp>

#include <Input/Controller.hpp>
#include <Input/InputManager.hpp>
#include <Input/Event.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Core/Core.hpp>

#include <Core/Logging/Logger.hpp>

#include <Core/Utilities/Pair.hpp>
#include <Core/Utilities/BitField.hpp>

#include <System/AppContext.hpp>

#include <steam/steam_api.h>

namespace Hyperion {
namespace Steam {

HYP_DECLARE_LOG_CHANNEL(Steam);

bool IsInitialized();

static inline ControllerHandle MakeSteamInputControllerHandle(uint8 controllerIndex)
{
    uint64 value = (1u << controllerIndex) | 0x100;
    return reinterpret_cast<ControllerHandle>(value);
}

struct ActionSetDesc
{
    const char* setName;
    const char* const* analogActions;
    Pair<const char*, ControllerButton> const* digitalActions;
};

static constexpr const char* FPSControls_AnalogActions[] = {
    "Move",
    "Look",
    "LeftTrigger",
    "RightTrigger",
    nullptr
};

static constexpr Pair<const char*, ControllerButton> FPSControls_DigitalActions[] = {
    { "A", ControllerButton::A },
    { "B", ControllerButton::B },
    { "X", ControllerButton::X },
    { "Y", ControllerButton::Y },
    { "DPad_Up", ControllerButton::DPad_Up },
    { "DPad_Down", ControllerButton::DPad_Down },
    { "DPad_Left", ControllerButton::DPad_Left },
    { "DPad_Right", ControllerButton::DPad_Right },
    { "Left_Bumper", ControllerButton::Left_Bumper },
    { "Right_Bumper", ControllerButton::Right_Bumper },
    { "Left_Trigger", ControllerButton::Left_Trigger },
    { "Right_Trigger", ControllerButton::Right_Trigger },
    { "Left_Stick", ControllerButton::Left_Stick },
    { "Right_Stick", ControllerButton::Right_Stick },
    { "Start", ControllerButton::Start },
    { "Select", ControllerButton::Select },
    { "Guide", ControllerButton::Guide },
    { nullptr, ControllerButton::None }
};

static constexpr const ActionSetDesc ActionSetDescs[] = {
    // FPSControls
    ActionSetDesc {
        "FPSControls",
        FPSControls_AnalogActions,
        FPSControls_DigitalActions }
};

SteamInputManager s_steamInputManager;

SteamInputManager::SteamInputManager()
    : m_isInitialized(false),
      m_currentActionSet(0),
      m_actionSets {},
      m_windowState {}
{
}

SteamInputManager::~SteamInputManager()
{
    Shutdown();
}

SteamInputManager& SteamInputManager::GetInstance()
{
    return s_steamInputManager;
}

void SteamInputManager::Initialize()
{
    AssertOnThread(g_mainThread);

    if (m_isInitialized)
    {
        return;
    }

    if (!Steam::IsInitialized())
    {
        HYP_LOG(Steam, Error, "Steam API is not initialized; must be initialized before initializing Steam Input.");
        return;
    }

    if (!SteamInput()->Init(true))
    {
        HYP_LOG(Steam, Error, "Failed to initialize Steam Input!");
        return;
    }

    for (size_t i = 0; i < GetArrayCount(ActionSetDescs); i++)
    {
        InitializeActionSet(ActionSetDescs[i], m_actionSets[i]);

        m_actionSets[i].index = uint8(i);
    }

    m_onMainWindowChanged = AppContextBase::OnCurrentWindowChanged.Bind(
        g_appContext,
        [this](ApplicationWindow* window)
        {
            if (window == m_windowState.window)
            {
                return;
            }

            if (m_windowState.window != nullptr)
            {
                ShutdownWindowState(m_windowState);
            }

            if (window != nullptr)
            {
                InitializeWindowState(m_windowState, window);
            }
        });

    ApplicationWindow* window = g_appContext->GetMainWindow();
    if (window != nullptr)
    {
        InitializeWindowState(m_windowState, window);
    }
    else
    {
        Memory::Zero(&m_windowState, sizeof(WindowState));
    }

    m_isInitialized = true;
}

void SteamInputManager::Shutdown()
{
    AssertOnThread(g_mainThread);

    if (!m_isInitialized)
    {
        return;
    }

    m_isInitialized = false;

    if (m_windowState.window != nullptr)
    {
        ShutdownWindowState(m_windowState);
    }

    m_onMainWindowChanged.Reset();

    if (!SteamInput()->Shutdown())
    {
        HYP_LOG(Steam, Error, "Failed to properly shutdown Steam Input!");
    }
}

bool SteamInputManager::InitializeActionSet(const ActionSetDesc& desc, ActionSet& outSet)
{
    Memory::Zero(&outSet, sizeof(ActionSet));

    outSet.handle = SteamInput()->GetActionSetHandle(desc.setName);

    if (outSet.handle == 0)
    {
        return false;
    }

    for (uint32 actionIndex = 0; actionIndex < MaxAnalogActionHandles; actionIndex++)
    {
        if (!desc.analogActions[actionIndex])
        {
            break;
        }

        outSet.analogActionHandles[actionIndex] = SteamInput()->GetAnalogActionHandle(desc.analogActions[actionIndex]);
    }

    for (uint32 actionIndex = 0; actionIndex < MaxDigitalActionHandles; actionIndex++)
    {
        if (!desc.digitalActions[actionIndex].first)
        {
            break;
        }

        outSet.digitalActionHandles[actionIndex] = SteamInput()->GetDigitalActionHandle(desc.digitalActions[actionIndex].first);
    }

    return true;
}

void SteamInputManager::Update()
{
    AssertOnThread(g_mainThread);

    if (!m_isInitialized)
    {
        return;
    }

    ActionSet& actionSet = m_actionSets[m_currentActionSet];

    if (actionSet.handle == 0)
    {
        if (!InitializeActionSet(ActionSetDescs[m_currentActionSet], actionSet))
        {
            HYP_LOG(Steam, Verbose, "Invalid action set {}", ActionSetDescs[m_currentActionSet].setName);
            return;
        }

        actionSet.index = m_currentActionSet;
    }

    UpdateControllers(actionSet);
    ProcessControllerInput(actionSet);
}

void SteamInputManager::UpdateControllers(const ActionSet& set)
{
    if (m_windowState.window == nullptr)
    {
        return;
    }

    Handle<InputManager> inputManager = m_windowState.window->GetInputManager();
    if (!inputManager.IsValid())
    {
        return;
    }

    // @TODO Steamworks docs recommends only calling every 10hz or less.
    SteamAPI_RunCallbacks();

    SteamInput()->RunFrame();

    InputHandle_t steamControllers[STEAM_INPUT_MAX_COUNT];
    const int controllerCount = SteamInput()->GetConnectedControllers(steamControllers);

    BitField<MaxConnectedControllers> mask {};

    for (int i = 0; i < controllerCount && i < int(MaxConnectedControllers); i++)
    {
        const InputHandle_t steamHandle = steamControllers[i];

        // Check if this steam handle is already mapped to a slot
        bool found = false;
        for (uint8 controllerIndex = 0; controllerIndex < MaxConnectedControllers; controllerIndex++)
        {
            if (m_windowState.m_controllers[controllerIndex] == steamHandle)
            {
                mask.Set(controllerIndex, true);
                found = true;

                break;
            }
        }

        if (!found)
        {
            for (uint8 controllerIndex = 0; controllerIndex < MaxConnectedControllers; controllerIndex++)
            {
                if (m_windowState.m_controllers[controllerIndex] == 0)
                {
                    m_windowState.m_controllers[controllerIndex] = steamHandle;
                    mask.Set(controllerIndex, true);

                    ControllerHandle controllerHandle = MakeSteamInputControllerHandle(controllerIndex);
                    inputManager->AddController(controllerHandle);

                    SteamInput()->ActivateActionSet(steamHandle, set.handle);

                    HYP_LOG(Steam, Info, "Steam controller connected at index {}", controllerIndex);

                    break;
                }
            }
        }
        else
        {
            SteamInput()->ActivateActionSet(steamHandle, set.handle);
        }
    }

    // disconnect removed controllers
    for (uint8 controllerIndex = 0; controllerIndex < MaxConnectedControllers; controllerIndex++)
    {
        if (m_windowState.m_controllers[controllerIndex] != 0 && !mask.Test(controllerIndex))
        {
            ControllerHandle controllerHandle = MakeSteamInputControllerHandle(controllerIndex);
            inputManager->RemoveController(controllerHandle);

            m_windowState.m_controllers[controllerIndex] = 0;

            // reset states for this controller
            m_windowState.digitalActionStates[controllerIndex] = {};

            HYP_LOG(Steam, Info, "Steam controller disconnected from index {}", controllerIndex);
        }
    }
}

void SteamInputManager::ProcessControllerInput(const ActionSet& set)
{
    if (m_windowState.window == nullptr)
    {
        return;
    }

    Handle<InputManager> inputManager = m_windowState.window->GetInputManager();
    Assert(inputManager.IsValid());

    PlatformEvent platformEvent = {};

    for (uint8 controllerIndex = 0; controllerIndex < MaxConnectedControllers; controllerIndex++)
    {
        const InputHandle_t steamHandle = static_cast<InputHandle_t>(m_windowState.m_controllers[controllerIndex]);

        if (steamHandle == 0)
        {
            continue;
        }

        for (uint32 actionIndex = 0; actionIndex < MaxDigitalActionHandles; actionIndex++)
        {
            if (!set.digitalActionHandles[actionIndex])
            {
                continue;
            }

            const InputDigitalActionData_t actionData = SteamInput()->GetDigitalActionData(
                steamHandle, set.digitalActionHandles[actionIndex]);

            if (actionData.bActive)
            {
                if (actionData.bState != m_windowState.digitalActionStates[controllerIndex].Test(actionIndex))
                {
                    const EventType eventType = actionData.bState
                        ? EventType::CONTROLLER_BUTTON_DOWN
                        : EventType::CONTROLLER_BUTTON_UP;

                    Event event(eventType, m_windowState.window, platformEvent);

                    // Get ControllerButton from ActionSetDesc.
                    event.GetEventData().Set(ActionSetDescs[set.index].digitalActions[actionIndex].second);

                    inputManager->ProcessEvent(std::move(event));

                    m_windowState.digitalActionStates[controllerIndex].Set(actionIndex, actionData.bState);
                }
            }
        }

        for (uint32 actionIndex = 0; actionIndex < MaxAnalogActionHandles; actionIndex++)
        {
            if (!set.analogActionHandles[actionIndex])
            {
                continue;
            }

            const InputAnalogActionData_t actionData = SteamInput()->GetAnalogActionData(
                steamHandle, set.analogActionHandles[actionIndex]);

            if (actionData.bActive)
            {
                ControllerAnalogData analogData = {};
                analogData.controllerIndex = controllerIndex;
                analogData.actionIndex = uint8(actionIndex);
                analogData.value = Vec2f(actionData.x, actionData.y);

                Event event(EventType::CONTROLLER_ANALOG_MOVE, m_windowState.window, platformEvent);
                event.GetEventData().Set(analogData);

                inputManager->ProcessEvent(std::move(event));
            }
        }
    }
}

void SteamInputManager::InitializeWindowState(WindowState& windowState, ApplicationWindow* window)
{
    Assert(window != nullptr);

    static_assert(std::is_trivial_v<WindowState>);
    Memory::Zero(&windowState, sizeof(WindowState));

    InputManager* inputManager = window->GetInputManager();
    Assert(inputManager != nullptr);

    windowState.window = window;
}

void SteamInputManager::ShutdownWindowState(WindowState& windowState)
{
    Assert(windowState.window != nullptr);

    InputManager* inputManager = windowState.window->GetInputManager();

    for (uint8 controllerIndex = 0; controllerIndex < MaxConnectedControllers; controllerIndex++)
    {
        if (windowState.m_controllers[controllerIndex] != 0)
        {
            if (inputManager != nullptr)
            {
                ControllerHandle controllerHandle = MakeSteamInputControllerHandle(controllerIndex);
                inputManager->RemoveController(controllerHandle);
            }

            windowState.m_controllers[controllerIndex] = 0;
        }
    }

    windowState.window = nullptr;
}

} // namespace Steam
} // namespace Hyperion