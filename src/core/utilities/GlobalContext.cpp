#include <core/utilities/GlobalContext.hpp>

namespace hyperion {
namespace utilities {

#pragma region GlobalContextRegistry

thread_local GlobalContextRegistry g_global_context_registry { };

HYP_API GlobalContextRegistry *GetGlobalContextRegistryForCurrentThread()
{
    return &g_global_context_registry;
}

GlobalContextRegistry::GlobalContextRegistry()
    : m_owner_thread_id(ThreadID::Current())
{
}

GlobalContextRegistry::~GlobalContextRegistry() = default;

#pragma endregion GlobalContextRegistry

} // namespace utilities
} // namespace hyperion