/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <console/commands/LogEntitiesCommand.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Console);
HYP_DEFINE_LOG_SUBCHANNEL(LogEntities, Console);

Result LogEntitiesCommand::Execute_Impl(const CommandLineArguments &args)
{
    HYP_LOG(LogEntities, Info, "LogEntitiesCommand");

    return { };
}

CommandLineArgumentDefinitions LogEntitiesCommand::GetDefinitions_Internal() const
{
    return { };
}

} // namespace hyperion
