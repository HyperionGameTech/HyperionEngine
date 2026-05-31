#include <HyperionPch.hpp>

#include <Framework/Commandlet/Commandlet.hpp>

#include <Core/Reflection/ClassUtils.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

#include <Core/CLI/CommandLine.hpp>

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

const Class* LogMemoryPools::StaticClass()
{
    return g_clsLogMemoryPools;
}

HYP_BEGIN_CLASS(LogMemoryPools, -1, 0, NAME("CommandletBase"), ClassAttribute("command", "logmemorypools"))
    Method(NAME("GetArgumentDefinitions"), &Type::GetArgumentDefinitions)
HYP_END_CLASS

HYP_REGISTER_STATIC_CLASS(LogMemoryPools);

} // namespace Hyperion
