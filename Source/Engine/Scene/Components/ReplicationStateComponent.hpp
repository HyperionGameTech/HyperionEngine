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

HYP_STRUCT(Component, Serialize = false, Editor = false)
struct ReplicationStateComponent
{
    HYP_STRUCT_BODY(ReplicationStateComponent);

    HYP_FIELD()
    NetId netId;

    ReplicationStateComponent()
        : netId(Invalid<NetId>)
    {
    }

    explicit ReplicationStateComponent(NetId netId)
        : netId(netId)
    {
    }
};

} // namespace Hyperion
