#ifndef HYPERION_HTEST_H
#define HYPERION_HTEST_H

#include <util/Defines.hpp>

#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <type_traits>

namespace hyperion::test {

struct ExpectFunctor {
    std::string stringified;
    std::add_pointer_t<bool()> fn;
};

struct ExpectationResult {
    std::string stringified;
    bool success;
};

struct CaseResult {
    std::string case_name;
    std::vector<ExpectationResult> expectation_results;
};

struct UnitResult {
    std::string unit_name;
    std::vector<CaseResult> case_results;
};

struct TestReport {
    std::vector<UnitResult> unit_results;
};

class TestClassBase {
public:
    TestReport Run() const;

protected:
    struct Case {
        std::vector<ExpectFunctor> expectations;

        void operator()(ExpectFunctor &&fn)
        {
            expectations.push_back(std::move(fn));
        }
    };

    using ItLambda = std::add_pointer_t<void(Case &expect)>;

    struct Unit {
        void operator()(const std::string &case_name, ItLambda &&lambda)
        {
            Case _case;
            lambda(_case);

            cases[case_name] = std::move(_case);
        }
        
        std::unordered_map<std::string, Case> cases;
    };

    using DescribeLambda = std::add_pointer_t<void(Unit &it)>;
    
    void Describe(const std::string &unit_name, DescribeLambda &&lambda)
    {
        Unit unit;
        lambda(unit);

        m_test_units[unit_name] = std::move(unit);
    }

    std::unordered_map<std::string, Unit> m_test_units;
};

template <class T>
class TestClass : public TestClassBase {
};

class GlobalTestManager {
public:
    static GlobalTestManager *Instance();
    static bool PrintReport(const TestReport &report);

    TestReport RunAll() const;

private:
    template <class T, class ...Args>
    void AddTestClass(Args &&... args)
    {
        m_test_classes.push_back(std::make_unique<TestClass<T>>(std::move(args)...));
    }

    std::vector<std::unique_ptr<TestClassBase>> m_test_classes;
};

} // namespace hyperion::test

#define HYP_EXPECT(cond) expect({ #cond, [](void) -> bool { return cond; }})

#endif