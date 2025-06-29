/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_PROFILE_SCOPE_HPP
#define HYPERION_PROFILE_SCOPE_HPP

#include <core/utilities/Time.hpp>

#include <core/utilities/StringView.hpp>

#include <core/containers/String.hpp>

#include <core/threading/Mutex.hpp>

#include <core/Defines.hpp>
#include <core/Util.hpp> // For HYP_PRETTY_FUNCTION_NAME

#include <Types.hpp>

namespace hyperion {
namespace profiling {

class ProfilerConnection;
class ProfileScopeStack;
struct ProfileScopeEntry;

struct ProfilerConnectionParams
{
    String endpoint_url;
    bool enabled;
};

extern HYP_API void StartProfilerConnectionThread(const ProfilerConnectionParams& params);
extern HYP_API void StopProfilerConnectionThread();

struct HYP_API ProfileScope
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

#ifdef HYP_ENABLE_PROFILE
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
    HYP_NAMED_SCOPE(*Threads::CurrentThreadId().GetName())

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

} // namespace hyperion

#endif