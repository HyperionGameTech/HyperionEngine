/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/utilities/Result.hpp>

#include <core/reflection/ObjectBase.hpp>

namespace Hyperion {

namespace cli {
class CommandLineArguments;
} // namespace cli

using cli::CommandLineArguments;

HYP_CLASS(Abstract)
class HYP_API CommandletBase : public ObjectBase
{
    HYP_OBJECT_BODY(CommandletBase);

public:
    virtual ~CommandletBase() override = default;

    HYP_METHOD(Scriptable)
    Result Run(const CommandLineArguments& args);

protected:
    HYP_METHOD()
    virtual Result Run_Impl(const CommandLineArguments& args) = 0;
};

} // namespace Hyperion
