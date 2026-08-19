/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/HashCode.hpp>

#include <Core/Reflection/ObjectMacros.hpp>

#include <Core/Logging/LoggerFwd.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Replication);

enum class NetId : uint32;

namespace net {
enum class NetConnectionId : uint32;
} // namespace net

HYP_STRUCT(Component, Serialize = false, Editor = false)
struct ReplicationStateComponent
{
    HYP_STRUCT_BODY(ReplicationStateComponent);

    HYP_FIELD()
    NetId netId;

    // Connection currently allowed to submit authoritative requests (e.g. transform) for this
    // entity. 0 (connection ids are allocated starting at 1) means unowned -- any client may
    // claim it implicitly by request.
    HYP_FIELD()
    net::NetConnectionId ownerConnectionId {};
};

} // namespace Hyperion
