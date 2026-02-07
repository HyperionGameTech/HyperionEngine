#include <core/utilities/GlobalContext.hpp>

#include <core/threading/Thread.hpp>
#include <core/threading/Threads.hpp>
#include <core/threading/ThreadLocalStorage.hpp>

#include <core/functional/Proc.hpp>

namespace Hyperion {
namespace utilities {

#pragma region GlobalContextRegistry

static thread_local GlobalContextRegistry* s_globalContextRegistry;
static thread_local ValueStorage<GlobalContextRegistry> s_globalContextRegistryStorage;

static_assert(std::is_trivially_destructible_v<decltype(s_globalContextRegistryStorage)>);

HYP_API GlobalContextRegistry* GetGlobalContextRegistryForCurrentThread()
{
    if (!s_globalContextRegistry)
    {
        s_globalContextRegistry = new (&s_globalContextRegistryStorage.Get()) GlobalContextRegistry;
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
}

#pragma endregion GlobalContextRegistry

} // namespace utilities
} // namespace Hyperion