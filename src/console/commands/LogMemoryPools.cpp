#include <HyperionPch.hpp>

#include <console/ConsoleCommand.hpp>

#include <core/reflection/ClassUtils.hpp>

#include <core/memory/MemoryPool.hpp>

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
        // Calculate memory pool usage
        Array<Pair<MemoryPoolBase*, SizeType>> memoryUsagePerPool;
        CalculateMemoryUsagePerPool(memoryUsagePerPool);

        SizeType totalMemoryPoolUsage = 0;
        for (SizeType i = 0; i < memoryUsagePerPool.Size(); i++)
        {
            HYP_LOG(Console, Debug, "Memory Usage for pool {} : {} MiB", memoryUsagePerPool[i].first->GetPoolName(), double(memoryUsagePerPool[i].second) / 1024 / 1024);
            totalMemoryPoolUsage += memoryUsagePerPool[i].second;
        }

        HYP_LOG(Console, Debug, "Total Memory Usage for pools : {} MiB", double(totalMemoryPoolUsage) / 1024 / 1024);

        return {}; // ok
    }
};

HYP_API const Class* g_clsLogMemoryPools = nullptr;

HYP_BEGIN_CLASS(LogMemoryPools, -1, 0, NAME("ConsoleCommandBase"), ClassAttribute("command", "logmemorypools"))
HYP_END_CLASS

HYP_REGISTER_STATIC_CLASS(LogMemoryPools);

} // namespace hyperion
