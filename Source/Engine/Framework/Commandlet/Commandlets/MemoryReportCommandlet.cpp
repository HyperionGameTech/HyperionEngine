#include <HyperionPch.hpp>

#include <Framework/Commandlet/Commandlet.hpp>

#include <Core/Reflection/ClassUtils.hpp>
#include <Core/Reflection/ClassRegistry.hpp>
#include <Core/Reflection/ObjectPool.hpp>

#include <Core/CLI/CommandLine.hpp>

#include <Core/Memory/Pool/Pool.hpp>
#include <Core/Memory/MemoryMetrics.hpp>

#include <Framework/EngineMemory.hpp>
#include <Rendering/RenderMemory.hpp>

#include <Scripting/ScriptObjectResource.hpp>

#ifdef HYP_DOTNET
#include <DotNET/DotNETHost.hpp>
#include <DotNET/ManagedClass.hpp>
#endif ?? HYP_DOTNET

#ifdef HYP_EDITOR
#include <Editor/EditorMemory.hpp>
#include <Baking/BakerMemory.hpp>
#endif // HYP_EDITOR

#include <cstdio>

namespace Hyperion {

static ANSIString FormatBytes(size_t bytes)
{
    static constexpr const char* Suffixes[] = { "B", "KiB", "MiB", "GiB" };
    
    double dbl = double(bytes);
    
    int suffix = 0;

    while (dbl >= 1024.0 && suffix < int(sizeof(Suffixes) / sizeof(Suffixes[0]) - 1))
    {
        dbl /= 1024.0;
        suffix++;
    }

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.2f %s", dbl, Suffixes[suffix]);

    return ANSIString(buf);
}

class MemoryReportCommandlet : public CommandletBase
{
    HYP_OBJECT_BODY(MemoryReportCommandlet);

public:
    virtual ~MemoryReportCommandlet() override = default;

    HYP_METHOD()
    static const CommandLineArgumentDefinitions& GetArgumentDefinitions()
    {
        static CommandLineArgumentDefinitions s_definitions;

        static bool s_initialized = false;
        if (!s_initialized)
        {
            s_initialized = true;

            s_definitions.Add(
                "pools",
                "p",
                "Comma-separated list of pools to report (object,scene,asset,streaming,render,vulkan,dx12,editor,baker), or 'all'",
                CommandLineArgumentFlags::NONE,
                Array<String> { "all" },
                JSON::Value("all"));

            s_definitions.Add(
                "objects",
                "o",
                "Include per-type ObjectContainer breakdown",
                CommandLineArgumentFlags::NONE,
                {},
                JSON::Value(false));

            s_definitions.Add(
                "csharp",
                "c",
                "Include C# managed object breakdown",
                CommandLineArgumentFlags::NONE,
                {},
                JSON::Value(false));
        }

        return s_definitions;
    }

protected:
    struct PoolInfo
    {
        ANSIString name;
        Pool* pool;
        size_t blockSize;
    };

    struct ObjectTypeStats
    {
        ANSIString typeName;
        uint32 count = 0;
        size_t instanceSize = 0;
        size_t totalMemory = 0;
        double avgRefCount = 0.0;
        int32 minRefCount = INT32_MAX;
        int32 maxRefCount = 0;
        uint32 csharpCompanionCount = 0;
    };

    // Ignore the warning, this is fine as we are using string literal
    // by virtue of HYP_STATIC_STRING.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"

    template <auto Fmt, class... Args>
    static HYP_FORCE_INLINE void LogLineEx(Args&&... args)
    {
        char buf[512];
        std::snprintf(buf, sizeof(buf), Fmt.Data(), std::forward<Args>(args)...);

        HYP_LOG(Engine, Info, "{}", ANSIString(buf));
    }

#pragma clang diagnostic pop

#define LOG_LINE(fmt, ...) LogLineEx<HYP_STATIC_STRING(fmt)>(__VA_ARGS__)

    static Array<PoolInfo> GetAllPools(const CommandLineArguments& args)
    {
        ANSIString poolFilter = "all";
        if (args.Contains("pools"))
        {
            poolFilter = args["pools"].ToString().ToLower();
        }

        Array<PoolInfo> pools;

        auto tryAddPool = [&](const ANSIString& name, Pool* pool)
        {
            if (!pool)
            {
                return;
            }

            if (poolFilter == "all" || poolFilter.ToLower().Contains(name))
            {
                pools.PushBack({ name, pool, pool->GetBlockSize() });
            }
        };

        tryAddPool("object", g_objectPool);
        tryAddPool("scene", g_scenePool);
        tryAddPool("asset", g_assetPool);
        tryAddPool("streaming", g_streamingPool);
        tryAddPool("render", g_renderPool);

#ifdef HYP_EDITOR
        tryAddPool("editor", g_editorPool);
        tryAddPool("baker", Baking::g_bakerPool);
#endif // HYP_EDITOR

#if HYP_VULKAN
        tryAddPool("vulkan", g_vulkanPool);
#elif HYP_DX12
        tryAddPool("dx12", g_dx12Pool);
#endif

        return pools;
    }

    void ReportPools(const CommandLineArguments& args)
    {
        Array<PoolInfo> pools = GetAllPools(args);

        if (pools.Empty())
        {
            HYP_LOG(Engine, Info, "No pools selected.");
            return;
        }

        LOG_LINE("");
        LOG_LINE("======= Memory Pools =======");
        LOG_LINE("%-18s %-10s %-14s %-14s %-14s %-14s %-6s %-5s",
                 "Pool", "Block", "Committed", "Used", "Free", "Peak", "Util", "Frag");

        MemoryMetrics totalMetrics;

        for (const auto& pi : pools)
        {
            MemoryMetrics metrics = pi.pool->GetMemoryMetrics();

            totalMetrics += metrics;

            float utilPct = metrics.GetUtilization() * 100.0f;
            float fragPct = metrics.GetFragmentation() * 100.0f;

            LOG_LINE("%-18s %-10s %-14s %-14s %-14s %-14s %5.1f%% %5.1f%%",
                     pi.name.Data(),
                     FormatBytes(pi.blockSize).Data(),
                     FormatBytes(metrics[MemoryMetrics::MM_BYTES_COMMITTED]).Data(),
                     FormatBytes(metrics[MemoryMetrics::MM_BYTES_USED]).Data(),
                     FormatBytes(metrics[MemoryMetrics::MM_BYTES_FREE]).Data(),
                     FormatBytes(metrics[MemoryMetrics::MM_BYTES_PEAK]).Data(),
                     utilPct, fragPct);
        }

        float totalUtil = totalMetrics.GetUtilization() * 100.0f;
        float totalFrag = totalMetrics.GetFragmentation() * 100.0f;

        LOG_LINE("%-30s %-14s %-14s %-14s %-14s %5.1f%% %5.1f%%",
                 "Total",
                 FormatBytes(totalMetrics[MemoryMetrics::MM_BYTES_COMMITTED]).Data(),
                 FormatBytes(totalMetrics[MemoryMetrics::MM_BYTES_USED]).Data(),
                 FormatBytes(totalMetrics[MemoryMetrics::MM_BYTES_FREE]).Data(),
                 FormatBytes(totalMetrics[MemoryMetrics::MM_BYTES_PEAK]).Data(),
                 totalUtil, totalFrag);
    }

    void ReportObjectBreakdown()
    {
        ObjectContainerMap& containerMap = GetObjectContainerMap();

        Array<ObjectTypeStats> allStats;

        auto iterateContainers = [&](TypeId typeId, ObjectContainerBase* container)
        {
            const Class* cls = container->GetClass();
            if (!cls)
                return;

            ObjectTypeStats stats;
            stats.typeName = cls->GetName().LookupString();
            stats.instanceSize = cls->GetSize();

            auto iterateHeaders = [&](const ObjectHeader* header)
            {
                if (!header || !header->cls)
                {
                    return;
                }

                stats.count++;
                stats.totalMemory += stats.instanceSize;

                int32 refCount = header->GetRefCountStrong();
                stats.avgRefCount += double(refCount);
                stats.minRefCount = MathUtil::Min(stats.minRefCount, refCount);
                stats.maxRefCount = MathUtil::Max(stats.maxRefCount, refCount);

#if defined(HYP_DOTNET) || defined(HYP_SCRIPT)
                ObjectBase* obj = ObjectHeader::GetObjectPointer(const_cast<ObjectHeader*>(header));
                if (obj)
                {
                    if (ScriptObjectResource* res = obj->GetScriptObjectResource())
                    {
                        if (ScriptObjectFunctions::GetScriptLanguageMask && (ScriptObjectFunctions::GetScriptLanguageMask(res) & (1u << uint32(ScriptLanguage::CSharp))))
                        {
                            stats.csharpCompanionCount++;
                        }
                    }
                }
#endif
            };

            container->ForEachHeader(iterateHeaders);

            if (stats.count > 0)
            {
                stats.avgRefCount /= double(stats.count);
                allStats.PushBack(std::move(stats));
            }
        };

        containerMap.ForEachContainer(iterateContainers);

        if (allStats.Empty())
        {
            HYP_LOG(Engine, Info, "No objects currently allocated.");
            return;
        }

        LOG_LINE("");
        LOG_LINE("=== Object Containers (per-type) ===");
        LOG_LINE("%-30s %-7s %-7s %-14s %-7s %-7s %-7s %-6s",
                 "Type", "Count", "Sizeof", "Total", "AvgRef", "MinRef", "MaxRef", "C#Cmp");

        size_t totalObjectMemory = 0;
        uint32 totalObjectCount = 0;
        uint32 totalCSharpCompanionCount = 0;

        for (const auto& s : allStats)
        {
            totalObjectMemory += s.totalMemory;
            totalObjectCount += s.count;
            totalCSharpCompanionCount += s.csharpCompanionCount;

            LOG_LINE("%-30s %-7u %-7s %-14s %-7.1f %-7d %-7d %-6u",
                     s.typeName.Data(),
                     s.count,
                     FormatBytes(s.instanceSize).Data(),
                     FormatBytes(s.totalMemory).Data(),
                     s.avgRefCount,
                     s.minRefCount,
                     s.maxRefCount,
                     s.csharpCompanionCount);
        }

        LOG_LINE("--------------------------------------------------------------------------------");
        LOG_LINE("%-30s %-7u %-7s %-14s",
                 "Total", totalObjectCount, "", FormatBytes(totalObjectMemory).Data());
        LOG_LINE("  Objects with C# companion: %u", totalCSharpCompanionCount);
        LOG_LINE("  Native-only objects: %u", totalObjectCount - totalCSharpCompanionCount);
    }

    void ReportCSharpBreakdown()
    {
#if defined(HYP_DOTNET) && HYP_DOTNET
        DotNETHost::GlobalFunctions& globalFns = DotNETHost::GetInstance().GetGlobalFunctions();

        if (!globalFns.queryManagedObjectCountsFptr)
        {
            HYP_LOG(Engine, Info, "C# query function not available.");

            if (globalFns.getTotalMemoryFptr)
            {
                int64 totalMemoryBytes = globalFns.getTotalMemoryFptr();
                LOG_LINE(".NET managed heap: %s", FormatBytes(size_t(totalMemoryBytes)).Data());
            }

            return;
        }

        constexpr int maxPairs = 256;
        void* pairs[maxPairs * 2] = {};
        int count = globalFns.queryManagedObjectCountsFptr(pairs, maxPairs);

        if (count == 0)
        {
            HYP_LOG(Engine, Info, "No managed C# objects tracked via interop.");

            if (globalFns.getTotalMemoryFptr)
            {
                int64 totalMemoryBytes = globalFns.getTotalMemoryFptr();
                LOG_LINE(".NET managed heap: %s", FormatBytes(size_t(totalMemoryBytes)).Data());
            }

            return;
        }

        LOG_LINE("");
        LOG_LINE("=== Managed C# Objects ===");
        LOG_LINE("%-32s %-7s %-16s",
                 "C# Type", "Count", "C++ Companion");

        int totalCount = 0;

        for (int i = 0; i < count; i++)
        {
            auto* mc = static_cast<dotnet::ManagedClass*>(pairs[i * 2 + 0]);
            int objCount = static_cast<int>(reinterpret_cast<intptr_t>(pairs[i * 2 + 1]));

            if (!mc || objCount <= 0)
                continue;

            totalCount += objCount;

            bool hasCppCompanion = mc->GetClass() != nullptr;

            LOG_LINE("%-32s %-7d %-16s",
                     mc->GetName().Data(),
                     objCount,
                     hasCppCompanion ? "yes" : "no");
        }

        LOG_LINE("--------------------------------------------------------------");
        LOG_LINE("Total managed objects: %d", totalCount);

        if (globalFns.getTotalMemoryFptr)
        {
            int64 totalMemoryBytes = globalFns.getTotalMemoryFptr();
            LOG_LINE(".NET managed heap: %s", FormatBytes(size_t(totalMemoryBytes)).Data());
        }

#else
        HYP_LOG(Engine, Info, "C# interop not enabled.");
#endif
    }

    virtual Result Run_Impl(const CommandLineArguments& args) override
    {
        HYP_LOG(Engine, Info, "=== Memory Report ===");

        ReportPools(args);

        if (args.Contains("objects") && args["objects"].ToBool())
            ReportObjectBreakdown();

        if (args.Contains("csharp") && args["csharp"].ToBool())
            ReportCSharpBreakdown();

        LOG_LINE("");
        LOG_LINE("=== End Memory Report ===");

        return {};
    }

#undef LOG_LINE
};

ENGINE_API const Class* g_clsMemoryReportCommandlet = nullptr;

const Class* MemoryReportCommandlet::StaticClass()
{
    return g_clsMemoryReportCommandlet;
}

HYP_BEGIN_CLASS(MemoryReportCommandlet, -1, 0, NAME("CommandletBase"),
                ClassAttribute("command", "memoryreport"))
Method(NAME("GetArgumentDefinitions"), &Type::GetArgumentDefinitions)
    HYP_END_CLASS

HYP_REGISTER_STATIC_CLASS(MemoryReportCommandlet);

} // namespace Hyperion
