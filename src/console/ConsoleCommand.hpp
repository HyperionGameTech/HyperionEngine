/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CONSOLE_COMMAND_HPP
#define HYPERION_CONSOLE_COMMAND_HPP

#include <core/Defines.hpp>

#include <core/object/HypObject.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/utilities/Result.hpp>

#include <core/cli/CommandLine.hpp>

namespace hyperion {

HYP_CLASS(Abstract)
class HYP_API ConsoleCommandBase : public EnableRefCountedPtrFromThis<ConsoleCommandBase>
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

    virtual CommandLineArgumentDefinitions GetDefinitions_Internal() const = 0;

private:
    CommandLineArgumentDefinitions m_definitions;
};

} // namespace hyperion

#endif
