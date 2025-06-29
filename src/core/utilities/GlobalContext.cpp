#include <core/utilities/GlobalContext.hpp>

namespace hyperion {
namespace utilities {

#pragma region GlobalContextRegistry

thread_local GlobalContextRegistry* g_global_context_registry = nullptr;

HYP_API GlobalContextRegistry* GetGlobalContextRegistryForCurrentThread()
{
    if (!g_global_context_registry)
    {
        g_global_context_registry = new GlobalContextRegistry();
    }

    return g_global_context_registry;
}

GlobalContextRegistry::GlobalContextRegistry()
    : m_owner_thread_id(ThreadId::Current())
{
}

GlobalContextRegistry::~GlobalContextRegistry() = default;

#pragma endregion GlobalContextRegistry

} // namespace utilities
} // namespace hyperion