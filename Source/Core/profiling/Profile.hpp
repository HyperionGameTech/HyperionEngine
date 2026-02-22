/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

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

    static Array<double> RunInterleved(Array<Profile*>&&, SizeType runsPer = 5, SizeType numIterations = 100, SizeType runsPerIteration = 100);

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

    Profile& Run(SizeType numIterations = 100, SizeType runsPerIteration = 100);

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
    SizeType m_iteration;
};

} // namespace profiling

using profiling::Profile;

} // namespace Hyperion
