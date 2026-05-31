/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Utilities/Time.hpp>

#include <Core/Utilities/StringView.hpp>

#include <Core/Containers/String.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Core/Defines.hpp>
#include <Core/Util.hpp> // For HYP_PRETTY_FUNCTION_NAME

#include <Core/Types.hpp>

namespace Hyperion {
namespace profiling {

class ProfilerConnection;
class ProfileScopeStack;
struct ProfileScopeEntry;

struct ProfilerConnectionParams
{
    String endpointUrl;
    bool enabled;
};

CORE_API void StartProfilerConnectionThread(const ProfilerConnectionParams& params);
CORE_API void StopProfilerConnectionThread();

/*! \brief Collect all hot functions across all registered profile scopes. Locks a global mutex so beware! */
CORE_API void CollectAllHotFunctions(Array<Pair<ANSIString, double>>& outHotFunctions);

struct CORE_API ProfileScope
{
    static ProfileScopeStack& GetProfileScopeStackForCurrentThread();
    static void ResetForCurrentThread();

    ProfileScopeEntry* entry;

    ProfileScope(ANSIStringView label = "", ANSIStringView location = "");

    ProfileScope(const ProfileScope& other) = delete;
    ProfileScope& operator=(const ProfileScope& other) = delete;

    ProfileScope(ProfileScope&& other) = delete;
    ProfileScope& operator=(ProfileScope&& other) = delete;

    ~ProfileScope();
};

#if HYP_ENABLE_PROFILE
#define HYP_NAMED_SCOPE(label)  \
    ProfileScope _profile_scope \
    {                           \
        (label), HYP_DEBUG_FUNC \
    }

#define HYP_NAMED_SCOPE_FMT(label, ...)                                         \
    const auto _profile_scope_format_string = HYP_FORMAT(label, ##__VA_ARGS__); \
    ProfileScope _profile_scope                                                 \
    {                                                                           \
        _profile_scope_format_string.Data(), HYP_DEBUG_FUNC                     \
    }

#define HYP_SCOPE                                                              \
    static const auto _profile_scope_function_name = HYP_PRETTY_FUNCTION_NAME; \
    ProfileScope _profile_scope                                                \
    {                                                                          \
        _profile_scope_function_name, HYP_DEBUG_FUNC                           \
    }

#define HYP_PROFILE_BEGIN                  \
    ProfileScope::ResetForCurrentThread(); \
    HYP_NAMED_SCOPE(*CurrentThreadId().GetName())

#else
#define HYP_NAMED_SCOPE(...)
#define HYP_NAMED_SCOPE_FMT(label, ...)
#define HYP_SCOPE
#define HYP_PROFILE_BEGIN
#endif

} // namespace profiling

using profiling::ProfilerConnectionParams;
using profiling::ProfileScope;
using profiling::StartProfilerConnectionThread;
using profiling::StopProfilerConnectionThread;
using profiling::CollectAllHotFunctions;

} // namespace Hyperion
