/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Reflection/Handle.hpp>
#include <Core/Reflection/ObjectMacros.hpp>

#include <Core/Math/Vector3.hpp>
#include <Core/Memory/SharedPtr.hpp>

#include <Core/Utilities/Traits.hpp>

#include <Physics/RigidBody.hpp>

namespace Hyperion {

class InputHandlerBase;

namespace net {
enum class NetConnectionId : uint32;
} // namespace net

HYP_STRUCT(Component,
    Label = "Player Component",
    Serialize = false,
    Editor = false)
struct PlayerComponent
{
    HYP_STRUCT_BODY(PlayerComponent);

    HYP_FIELD(Serialize = false)
    net::NetConnectionId connectionId;

    PlayerComponent()
        : connectionId(Invalid<net::NetConnectionId>)
    {
    }

    explicit PlayerComponent(net::NetConnectionId connectionId)
        : connectionId(connectionId)
    {
    }

    // True if this component's connection is the local client's own connection.
    bool IsLocalPlayer() const;
};

} // namespace Hyperion
