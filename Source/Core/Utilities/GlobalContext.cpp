#include <Core/Utilities/GlobalContext.hpp>

#include <Core/Threading/Thread.hpp>
#include <Core/Threading/Threads.hpp>
#include <Core/Threading/ThreadLocalStorage.hpp>

#include <Core/Functional/Proc.hpp>

namespace Hyperion {
namespace utilities {

#pragma region GlobalContextRegistry

thread_local GlobalContextRegistry* t_globalContextRegistry;
thread_local ValueStorage<GlobalContextRegistry> t_globalContextRegistryStorage;

static_assert(std::is_trivially_destructible_v<decltype(t_globalContextRegistryStorage)>);

CORE_API GlobalContextRegistry* GetGlobalContextRegistryForCurrentThread()
{
    if (!t_globalContextRegistry)
    {
        t_globalContextRegistry = new (&t_globalContextRegistryStorage.Get()) GlobalContextRegistry;
    }

    return t_globalContextRegistry;
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

    if (t_globalContextRegistry == this)
    {
        t_globalContextRegistry = nullptr;
    }
}

#pragma endregion GlobalContextRegistry

} // namespace utilities
} // namespace Hyperion
