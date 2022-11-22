#include "Profile.hpp"

#include <chrono>

namespace hyperion {

std::vector<double> Profile::RunInterleved(std::vector<Profile> &&profiles, SizeType runs_per, SizeType num_iterations, SizeType runs_per_iteration)
{
    std::vector<double> results;
    results.resize(profiles.size());

    SizeType run_index = 0;

    for (SizeType i = 0; i < runs_per; i++) {

        // size_t index = 0;
        SizeType index = run_index++ % profiles.size();
        SizeType counter = 0;

        while (counter < profiles.size()) {
            profiles[index].Run(num_iterations, runs_per_iteration);

            index = ++index % profiles.size();

            ++counter;
        }
    }

    for (SizeType i = 0; i < profiles.size(); i++) {
        results[i] = profiles[i].GetResult();
    }

    return results;
}

Profile &Profile::Run(SizeType num_iterations, SizeType runs_per_iteration)
{
    using namespace std;
    using namespace std::chrono;

    auto *times = new double[num_iterations];

    for (SizeType i = 0; i < num_iterations; i++) {
        auto start = high_resolution_clock::now();

        for (int j = 0; j < runs_per_iteration; j++) {
            m_lambda();
        }

        auto stop = high_resolution_clock::now();

        times[i] = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(stop - start).count();
    }

    double result = 0.0;

    for (SizeType i = 0; i < num_iterations; i++) {
        result += times[i];
    }

    result /= double(num_iterations);

    m_result += result;//= (m_result + result) * (1.0 / (m_iteration + 1));
    m_iteration++;

    delete[] times;

    return *this;
}

} // namespace hyperion