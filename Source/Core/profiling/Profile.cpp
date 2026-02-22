/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <Core/profiling/Profile.hpp>

#include <chrono>

namespace Hyperion {
namespace profiling {

Array<double> Profile::RunInterleved(Array<Profile*>&& profiles, SizeType runsPer, SizeType numIterations, SizeType runsPerIteration)
{
    Array<double> results;
    results.Resize(profiles.Size());

    SizeType runIndex = 0;

    for (SizeType i = 0; i < runsPer; i++)
    {

        // size_t index = 0;
        SizeType index = runIndex++ % profiles.Size();
        SizeType counter = 0;

        while (counter < profiles.Size())
        {
            profiles[index]->Run(numIterations, runsPerIteration);

            index = ++index % profiles.Size();

            ++counter;
        }
    }

    for (SizeType i = 0; i < profiles.Size(); i++)
    {
        results[i] = profiles[i]->GetResult();
    }

    return results;
}

Profile& Profile::Run(SizeType numIterations, SizeType runsPerIteration)
{
    double* times = new double[numIterations];

    for (SizeType i = 0; i < numIterations; i++)
    {
        auto start = std::chrono::high_resolution_clock::now();

        for (int j = 0; j < runsPerIteration; j++)
        {
            m_profileFunction();
        }

        auto stop = std::chrono::high_resolution_clock::now();

        times[i] = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(stop - start).count();
    }

    double result = 0.0;

    for (SizeType i = 0; i < numIterations; i++)
    {
        result += times[i];
    }

    result /= double(numIterations);

    m_result += result; //= (m_result + result) * (1.0 / (m_iteration + 1));
    m_iteration++;

    delete[] times;

    return *this;
}

} // namespace profiling
} // namespace Hyperion