#include <core/utilities/GlobalContext.hpp>

namespace hyperion {
namespace utilities {

#pragma region GlobalContextRegistry

static constexpr SizeType PoolBlockSize = 4096;

static thread_local GlobalContextRegistry* s_globalContextRegistry = nullptr;
static thread_local Pool* s_globalContextPool = nullptr;

HYP_API Pool* GetGlobalContextPoolForCurrentThread()
{
    if (!s_globalContextPool)
    {
        s_globalContextPool = new Pool(/* blockSize */ PoolBlockSize);
    }

    return s_globalContextPool;
}

HYP_API GlobalContextRegistry* GetGlobalContextRegistryForCurrentThread()
{
    if (!s_globalContextRegistry)
    {
        s_globalContextRegistry = new GlobalContextRegistry();
    }

    return s_globalContextRegistry;
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

    if (s_globalContextRegistry == this)
    {
        s_globalContextRegistry = nullptr;
    }

    if (s_globalContextPool)
    {
        delete s_globalContextPool;
        s_globalContextPool = nullptr;
    }
}

#pragma endregion GlobalContextRegistry

} // namespace utilities
} // namespace hyperion