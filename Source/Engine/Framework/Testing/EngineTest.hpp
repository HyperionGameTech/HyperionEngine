/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#ifdef HYP_TESTS

#include <Core/Defines.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Utilities/Format.hpp>

namespace Hyperion {
namespace tests {

class EngineTestContext;

using EngineTestFunction = void (*)(EngineTestContext& context);

struct EngineTestDefinition
{
    const char* suite;
    const char* name;
    EngineTestFunction function;
};

/// Collects the failed checks of the running test
class ENGINE_API EngineTestContext
{
public:
    /// The context of the test being run, or null outside of one
    static EngineTestContext* Current();

    /// Records a failure if the condition is false; returns the condition so a test can bail out of what depends on it
    bool Check(const char* checkName, bool condition, const String& detail = String::empty);

    bool HasFailed() const
    {
        return m_numFailedChecks != 0;
    }

    uint32 GetNumChecks() const
    {
        return m_numChecks;
    }

    uint32 GetNumFailedChecks() const
    {
        return m_numFailedChecks;
    }

    /// Every failed check on its own line
    const String& GetFailureMessage() const
    {
        return m_failureMessage;
    }

private:
    friend class EngineTestRunner;

    uint32 m_numChecks = 0;
    uint32 m_numFailedChecks = 0;
    String m_failureMessage;
};

/// Tests register at static initialization; the EngineTestCommandlet runs them
class ENGINE_API EngineTestRegistry
{
public:
    static EngineTestRegistry& GetInstance();

    void Register(const EngineTestDefinition& definition);

    const Array<EngineTestDefinition>& GetTests() const
    {
        return m_tests;
    }

private:
    Array<EngineTestDefinition> m_tests;
};

struct EngineTestRegistration
{
    EngineTestRegistration(const char* suite, const char* name, EngineTestFunction function)
    {
        EngineTestRegistry::GetInstance().Register({ suite, name, function });
    }
};

struct EngineTestResult
{
    String name;
    bool passed = false;
    String message;
    double durationMs = 0.0;
};

class ENGINE_API EngineTestRunner
{
public:
    /// "Suite.Name" of a test
    static String GetFullName(const EngineTestDefinition& test);

    /// Whether the filter (empty for everything) names the test's suite or is part of its full name
    static bool MatchesFilter(const EngineTestDefinition& test, const String& filter);

    static EngineTestResult Run(const EngineTestDefinition& test);
};

/// Reports a check against the running test; for tests that don't take the context
ENGINE_API bool Check(const char* checkName, bool condition, const String& detail = String::empty);

} // namespace tests
} // namespace Hyperion

#define HYP_ENGINE_TEST(suite, name)                                                                                                         \
    static void HYP_CONCAT(HypEngineTest_##suite##_, name)(::Hyperion::tests::EngineTestContext & context);                                  \
    static const ::Hyperion::tests::EngineTestRegistration HYP_CONCAT(g_hypEngineTestRegistration_##suite##_, name)(                          \
        #suite, #name, &HYP_CONCAT(HypEngineTest_##suite##_, name));                                                                           \
    static void HYP_CONCAT(HypEngineTest_##suite##_, name)(::Hyperion::tests::EngineTestContext & context)

/// Fails the test if the condition is false and carries on, naming the condition and the line
#define HYP_TEST_CHECK(condition) \
    context.Check(#condition, bool(condition), HYP_FORMAT("{}:{}", __FILE__, __LINE__))

/// As HYP_TEST_CHECK, with a message built the way HYP_FORMAT does
#define HYP_TEST_CHECK_MSG(condition, fmt, ...) \
    context.Check(#condition, bool(condition), HYP_FORMAT(fmt, ##__VA_ARGS__))

/// Fails the test if the condition is false and returns from it, for when what follows can't run without it
#define HYP_TEST_REQUIRE(condition)                \
    do                                             \
    {                                              \
        if (!HYP_TEST_CHECK(condition))            \
        {                                          \
            return;                                \
        }                                          \
    }                                              \
    while (false)

#endif // HYP_TESTS
