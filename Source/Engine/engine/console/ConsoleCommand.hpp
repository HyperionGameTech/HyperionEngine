/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <Core/utilities/Result.hpp>

#include <Core/cli/CommandLine.hpp>

namespace Hyperion {

HYP_CLASS(Abstract)
class HYP_API ConsoleCommandBase : public ObjectBase
{
    HYP_OBJECT_BODY(ConsoleCommandBase);

public:
    friend class ConsoleCommandManager;

    virtual ~ConsoleCommandBase() = default;

    HYP_FORCE_INLINE const CommandLineArgumentDefinitions& GetDefinitions() const
    {
        return m_definitions;
    }

    HYP_METHOD(Scriptable)
    Result Execute(const CommandLineArguments& args);

protected:
    virtual Result Execute_Impl(const CommandLineArguments& args) = 0;

    virtual CommandLineArgumentDefinitions GetDefinitions_Internal() const
    {
        return CommandLineArgumentDefinitions();
    }

private:
    CommandLineArgumentDefinitions m_definitions;
};

} // namespace Hyperion
