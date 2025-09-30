#include <core/utilities/GlobalContext.hpp>

namespace hyperion {
namespace utilities {

#pragma region GlobalContextRegistry

static constexpr SizeType g_poolBlockSize = 4096;

thread_local GlobalContextRegistry* g_globalContextRegistry = nullptr;
thread_local Pool* g_globalContextPool = nullptr;

HYP_API Pool* GetGlobalContextPoolForCurrentThread()
{
    if (!g_globalContextPool)
    {
        g_globalContextPool = new Pool(/* blockSize */ g_poolBlockSize);
    }

    return g_globalContextPool;
}

HYP_API GlobalContextRegistry* GetGlobalContextRegistryForCurrentThread()
{
    if (!g_globalContextRegistry)
    {
        g_globalContextRegistry = new GlobalContextRegistry();
    }

    return g_globalContextRegistry;
}

GlobalContextRegistry::GlobalContextRegistry()
    : m_ownerThreadId(ThreadId::Current())
{
}

GlobalContextRegistry::~GlobalContextRegistry()
{
    for (auto& it : m_contextHolders)
    {
        delete it.second;
    }

    if (g_globalContextRegistry == this)
    {
        g_globalContextRegistry = nullptr;
    }

    if (g_globalContextPool)
    {
        delete g_globalContextPool;
        g_globalContextPool = nullptr;
    }
}

#pragma endregion GlobalContextRegistry

} // namespace utilities
} // namespace hyperion