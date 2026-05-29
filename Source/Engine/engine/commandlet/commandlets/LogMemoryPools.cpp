#include <HyperionPch.hpp>

#include <engine/commandlet/Commandlet.hpp>

#include <Core/reflection/ClassUtils.hpp>
#include <Core/reflection/ClassRegistry.hpp>

#include <Core/cli/CommandLine.hpp>

namespace Hyperion {

class LogMemoryPools : public CommandletBase
{
    HYP_OBJECT_BODY(LogMemoryPools);

public:
    virtual ~LogMemoryPools() override = default;

    HYP_METHOD()
    static const CommandLineArgumentDefinitions& GetArgumentDefinitions()
    {
        static CommandLineArgumentDefinitions s_definitions;
        return s_definitions;
    }

protected:
    virtual Result Run_Impl(const CommandLineArguments& args) override
    {
        // @TODO
        return {}; // ok
    }
};

ENGINE_API const Class* g_clsLogMemoryPools = nullptr;

HYP_BEGIN_CLASS(LogMemoryPools, -1, 0, NAME("CommandletBase"), ClassAttribute("command", "logmemorypools"))
    Method(NAME("GetArgumentDefinitions"), &Type::GetArgumentDefinitions)
HYP_END_CLASS

HYP_REGISTER_STATIC_CLASS(LogMemoryPools);

} // namespace Hyperion
