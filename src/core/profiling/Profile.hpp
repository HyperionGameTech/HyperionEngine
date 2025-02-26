/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_PROFILE_HPP
#define HYPERION_PROFILE_HPP

#include <core/containers/Array.hpp>

#include <Types.hpp>

#include <type_traits>

namespace hyperion {
namespace profiling {

class Profile
{
public:
    using ProfileFunction = void(*)(void);

    static Array<double> RunInterleved(Array<Profile *> &&, SizeType runs_per = 5, SizeType num_iterations = 100, SizeType runs_per_iteration = 100);
    
    Profile(ProfileFunction profile_function)
        : m_profile_function(profile_function),
          m_result(0.0),
          m_iteration(0)
    {
    }

    Profile(const Profile &other)                   = delete;
    Profile &operator=(const Profile &other)        = delete;
    Profile(Profile &&other) noexcept               = default;
    Profile &operator=(Profile &&other) noexcept    = default;

    ~Profile() = default;
    
    Profile &Run(SizeType num_iterations = 100, SizeType runs_per_iteration = 100);

    double GetResult() const { return m_iteration == 0 ? 0.0 : m_result / static_cast<double>(m_iteration); }

    Profile &Reset()
    {
        m_result = 0.0;
        m_iteration = 0;

        return *this;
    }

private:
    ProfileFunction m_profile_function;
    double          m_result;
    SizeType        m_iteration;
};

} // namespace profiling

using profiling::Profile;

} // namespace hyperion

#endif