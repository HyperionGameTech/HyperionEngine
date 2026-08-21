/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Components/PlayerComponent.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Framework/Client/GameClient.hpp>

#include <Net/NetClient.hpp>

#include <PlayerComponent.generated.inl>

namespace Hyperion {

bool PlayerComponent::IsLocalPlayer() const
{
    return g_gameClient != nullptr
        && g_gameClient->IsConnected()
        && g_gameClient->GetNetClient().GetConnectionId() == connectionId;
}

} // namespace Hyperion
