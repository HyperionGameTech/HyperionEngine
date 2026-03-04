/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/containers/Array.hpp>

#include <Core/Types.hpp>

#include <type_traits>

namespace Hyperion {
namespace profiling {

class Profile
{
public:
    using ProfileFunction = void (*)(void);

    static Array<double> RunInterleved(Array<Profile*>&&, size_t runsPer = 5, size_t numIterations = 100, size_t runsPerIteration = 100);

    Profile(ProfileFunction profileFunction)
        : m_profileFunction(profileFunction),
          m_result(0.0),
          m_iteration(0)
    {
    }

    Profile(const Profile& other) = delete;
    Profile& operator=(const Profile& other) = delete;
    Profile(Profile&& other) noexcept = default;
    Profile& operator=(Profile&& other) noexcept = default;

    ~Profile() = default;

    Profile& Run(size_t numIterations = 100, size_t runsPerIteration = 100);

    double GetResult() const
    {
        return m_iteration == 0 ? 0.0 : m_result / static_cast<double>(m_iteration);
    }

    Profile& Reset()
    {
        m_result = 0.0;
        m_iteration = 0;

        return *this;
    }

private:
    ProfileFunction m_profileFunction;
    double m_result;
    size_t m_iteration;
};

} // namespace profiling

using profiling::Profile;

} // namespace Hyperion
