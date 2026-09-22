/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/ObjectMacros.hpp>

namespace Hyperion {

/// Editor state for networking. (PIE)

HYP_ENUM()
enum class EditorPlayNetMode : uint8
{
    Standalone = 0,
    Client,
    DedicatedServer
};

HYP_ENUM()
enum class EditorPlayNetStatus : uint8
{
    None = 0,
    Connecting,
    Connected,
    Failed,
    Disconnected,
    Hosting,
    StartingServer
};

struct EditorPlayNetState
{
    EditorPlayNetMode m_playNetMode;
    String m_playNetHost;
    uint32 m_playNetPort;
    bool m_playNetAutoLaunchServer;
    uint32 m_playNetCachePort;

    // latched from the settings above when a simulation starts
    EditorPlayNetMode m_activeNetMode;
    bool m_activeAutoLaunchServer;
    EditorPlayNetStatus status;

    EditorPlayNetState();
};

} // namespace Hyperion
