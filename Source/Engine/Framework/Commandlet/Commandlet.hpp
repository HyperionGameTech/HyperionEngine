/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Utilities/Result.hpp>

#include <Core/Reflection/ObjectBase.hpp>

namespace Hyperion {

namespace cli {
class CommandLineArguments;
struct CommandLineArgumentDefinitions;
} // namespace cli

using cli::CommandLineArguments;
using cli::CommandLineArgumentDefinitions;

HYP_CLASS(Abstract)
class ENGINE_API CommandletBase : public ObjectBase
{
    HYP_OBJECT_BODY(CommandletBase);

public:
    virtual ~CommandletBase() override = default;

    /// Add a decorated static method decorated to override argument deifnitions for a derived CommandletBase
    /// This implementation just returns an empty CommandLineArgumentDefinitions object
    HYP_METHOD()
    static const CommandLineArgumentDefinitions& GetArgumentDefinitions();

    HYP_METHOD()
    virtual Result Run(const CommandLineArguments& args) = 0;
};

} // namespace Hyperion
