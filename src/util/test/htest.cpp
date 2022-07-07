#include "Htest.hpp"

#include <core/lib/Range.hpp>

#include <thread>

namespace hyperion::test {

GlobalTestManager *GlobalTestManager::Instance()
{
    static GlobalTestManager instance;

    instance.AddTestClass<Range<int>>();

    return &instance;
}

bool GlobalTestManager::PrintReport(const TestReport &report)
{
    bool any_failures = false;

    for (auto &unit_result : report.unit_results) {
        DebugLog(LogType::Info, "%s\n", unit_result.unit_name.c_str());

        for (auto &case_result : unit_result.case_results) {
            DebugLog(LogType::Info, "%s\n", case_result.case_name.c_str());

            for (auto &expectation_result : case_result.expectation_results) {
                if (expectation_result.success) {
                    DebugLog(LogType::Info, "\tPASS:\t%s\n", expectation_result.stringified.c_str());
                } else {
                    DebugLog(LogType::Error, "\tFAIL:\t%s\n", expectation_result.stringified.c_str());

                    any_failures = true;
                }
            }
        }
    }

    return !any_failures;
}

TestReport GlobalTestManager::RunAll() const
{
    const auto &test_classes = m_test_classes;

    TestReport all_report{};

    std::vector<TestReport> test_reports;
    test_reports.resize(m_test_classes.size());

    std::vector<std::thread> threads;

    for (size_t i = 0; i < m_test_classes.size(); i++) {
        threads.emplace_back([&test_classes, &test_reports, index = i] {
            test_reports[index] = test_classes[index]->Run();
        });
    }

    for (size_t i = 0; i < threads.size(); i++) {
        threads[i].join();
        
        all_report.unit_results.insert(
            all_report.unit_results.end(),
            test_reports[i].unit_results.begin(),
            test_reports[i].unit_results.end()
        );
    }

    return all_report;
}

TestReport TestClassBase::Run() const
{
    const auto &test_units = m_test_units;

    std::vector<std::thread> threads;

    std::vector<UnitResult> unit_results;
    unit_results.resize(m_test_units.size());

    size_t test_unit_index = 0;

    for (auto &test_unit : m_test_units) {
        const size_t index = test_unit_index++;

        auto &unit_result = unit_results[index];
        unit_result.unit_name = test_unit.first;
        unit_result.case_results.resize(test_unit.second.cases.size());
        
        size_t case_index = 0;

        for (auto &it : test_unit.second.cases) {
            auto &expectation_results = unit_result.case_results[case_index].expectation_results;

            unit_result.case_results[case_index].case_name = it.first;
            expectation_results.resize(it.second.expectations.size());

            for (size_t i = 0; i < it.second.expectations.size(); i++) {
                const auto &expectation = it.second.expectations[i];

                threads.emplace_back([&expectation_results = expectation_results, expectation_index = i, &expectation] {
                    expectation_results[expectation_index] = {
                        .stringified = expectation.stringified,
                        .success     = expectation.fn()
                    };
                });
            }

            ++case_index;
        }
    }

    for (auto &thread : threads) {
        thread.join();
    }

    return TestReport{unit_results};
}

} // namespace hyperion::test