/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once
#include <engine/console/ConsoleCommand.hpp>

namespace hyperion {

HYP_CLASS(Command = "log_entities")
class HYP_API LogEntitiesCommand : public ConsoleCommandBase
{
    HYP_OBJECT_BODY(LogEntitiesCommand);

public:
    virtual ~LogEntitiesCommand() override = default;

protected:
    virtual Result Execute_Impl(const CommandLineArguments& args) override;

    virtual CommandLineArgumentDefinitions GetDefinitions_Internal() const override;
};

} // namespace hyperion

