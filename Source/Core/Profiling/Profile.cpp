/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Profiling/Profile.hpp>

#include <chrono>

namespace Hyperion {
namespace profiling {

Array<double> Profile::RunInterleved(Array<Profile*>&& profiles, size_t runsPer, size_t numIterations, size_t runsPerIteration)
{
    Array<double> results;
    results.Resize(profiles.Size());

    size_t runIndex = 0;

    for (size_t i = 0; i < runsPer; i++)
    {

        // size_t index = 0;
        size_t index = runIndex++ % profiles.Size();
        size_t counter = 0;

        while (counter < profiles.Size())
        {
            profiles[index]->Run(numIterations, runsPerIteration);

            index = ++index % profiles.Size();

            ++counter;
        }
    }

    for (size_t i = 0; i < profiles.Size(); i++)
    {
        results[i] = profiles[i]->GetResult();
    }

    return results;
}

Profile& Profile::Run(size_t numIterations, size_t runsPerIteration)
{
    double* times = new double[numIterations];

    for (size_t i = 0; i < numIterations; i++)
    {
        auto start = std::chrono::high_resolution_clock::now();

        for (int j = 0; j < runsPerIteration; j++)
        {
            m_profileFunction(false);
        }

        auto stop = std::chrono::high_resolution_clock::now();

        times[i] = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(stop - start).count();
    }

    double result = 0.0;

    for (size_t i = 0; i < numIterations; i++)
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
