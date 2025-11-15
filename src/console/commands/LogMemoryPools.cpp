#include <HyperionPch.hpp>

#include <console/ConsoleCommand.hpp>

#include <core/reflection/ClassUtils.hpp>

#include <core/memory/MemoryPool.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Console);

class LogMemoryPools : public ConsoleCommandBase
{
    HYP_OBJECT_BODY(LogMemoryPools);

public:
    virtual ~LogMemoryPools() override = default;

protected:
    virtual Result Execute_Impl(const CommandLineArguments& args) override
    {
        // This needs to be reimplemented as our memory pools have been rewritten since this was made

        return {}; // ok
    }
};

HYP_API const Class* g_clsLogMemoryPools = nullptr;

HYP_BEGIN_CLASS(LogMemoryPools, -1, 0, NAME("ConsoleCommandBase"), ClassAttribute("command", "logmemorypools"))
HYP_END_CLASS

HYP_REGISTER_STATIC_CLASS(LogMemoryPools);

} // namespace hyperion
