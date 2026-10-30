/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#ifdef HYP_TESTS

#include <Framework/Testing/EngineTest.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

#include <Core/Utilities/GlobalContext.hpp>

#include <Core/DataProcessing/JSON/JSON.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <chrono>

namespace Hyperion {
namespace tests {

bool EngineTestContext::Check(const char* checkName, bool condition, const String& detail)
{
    constexpr size_t MaxDetailLength = 200;

    ++m_numChecks;

    if (!condition)
    {
        ++m_numFailedChecks;

        if (m_failureMessage.Any())
        {
            m_failureMessage += "\n";
        }

        m_failureMessage += checkName;

        if (detail.Any())
        {
            String shortenedDetail;

            for (size_t index = 0; index < detail.Length() && index < MaxDetailLength; index++)
            {
                const char ch = char(detail[index]);

                shortenedDetail.Append(ch == '\n' || ch == '\r' || ch == '\t' ? ' ' : ch);
            }

            m_failureMessage += " (";
            m_failureMessage += shortenedDetail;
            m_failureMessage += detail.Length() > MaxDetailLength ? "...)" : ")";
        }
    }

    return condition;
}

EngineTestRegistry& EngineTestRegistry::GetInstance()
{
    static EngineTestRegistry s_instance;

    return s_instance;
}

void EngineTestRegistry::Register(const EngineTestDefinition& definition)
{
    m_tests.PushBack(definition);
}

String EngineTestRunner::GetFullName(const EngineTestDefinition& test)
{
    return String(test.suite) + "." + String(test.name);
}

bool EngineTestRunner::MatchesFilter(const EngineTestDefinition& test, const String& filter)
{
    if (filter.Empty())
    {
        return true;
    }

    return String(test.suite) == filter || GetFullName(test).Contains(filter);
}

EngineTestResult EngineTestRunner::Run(const EngineTestDefinition& test)
{
    GlobalContextScope scope(EngineTestContext {});
    EngineTestContext& context = *GetGlobalContext<EngineTestContext>();

    const auto startTime = std::chrono::steady_clock::now();

    test.function(context);

    const double durationMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - startTime).count();

    EngineTestResult result;
    result.name = GetFullName(test);
    result.passed = !context.HasFailed();
    result.message = context.GetFailureMessage();
    result.durationMs = durationMs;

    if (result.passed)
    {
        HYP_LOG(Engine, Info, "[PASS] {} ({} checks)", result.name, context.GetNumChecks());
    }
    else
    {
        HYP_LOG(Engine, Error, "[FAIL] {}: {} of {} checks failed\n{}", result.name, context.GetNumFailedChecks(), context.GetNumChecks(), result.message);
    }

    return result;
}

Result EngineTestRunner::RunTests(EngineTestHost host, const String& filter, bool listOnly, const String& resultsPath)
{
    JSON::JArray resultsArray;
    uint32 numFailed = 0;

    String phase = "engine";

    if (listOnly)
    {
        phase = "list";
    }
    else if (host == EngineTestHost::Editor)
    {
        phase = "editor";
    }

    const auto writeReport = [&]() -> Result
    {
        if (resultsPath.Empty())
        {
            return {};
        }

        JSON::Object report {
            { "phase", JSON::Value(phase) },
            { "tests", JSON::Value(resultsArray) }
        };

        const String reportText = JSON::Value(std::move(report)).ToString(true);

        FileByteWriter writer { FilePath(resultsPath) };

        if (!writer.IsOpen())
        {
            return HYP_MAKE_ERROR(Error, "Failed to open {} for writing", resultsPath);
        }

        writer.Write(reportText.Data(), reportText.Size());
        writer.Close();

        return {};
    };

    for (const EngineTestDefinition& test : EngineTestRegistry::GetInstance().GetTests())
    {
        if (test.host != host || !MatchesFilter(test, filter))
        {
            continue;
        }

        if (listOnly)
        {
            resultsArray.PushBack(JSON::Object { { "name", JSON::Value(GetFullName(test)) } });

            continue;
        }

        const EngineTestResult result = Run(test);

        if (!result.passed)
        {
            ++numFailed;
        }

        resultsArray.PushBack(JSON::Object {
            { "name", JSON::Value(result.name) },
            { "passed", JSON::Value(result.passed) },
            { "message", result.passed ? JSON::Value(JSON::JSNull()) : JSON::Value(result.message) },
            { "durationMs", JSON::Value(result.durationMs) }
        });

        if (Result writeResult = writeReport(); writeResult.HasError())
        {
            return writeResult;
        }
    }

    if (!listOnly)
    {
        HYP_LOG(Engine, Info, "{} tests: {} passed, {} failed", phase, resultsArray.Size() - numFailed, numFailed);
    }

    if (Result writeResult = writeReport(); writeResult.HasError())
    {
        return writeResult;
    }

    if (numFailed != 0)
    {
        return HYP_MAKE_ERROR(Error, "{} {} test(s) failed", numFailed, phase);
    }

    return {};
}

bool Check(const char* checkName, bool condition, const String& detail)
{
    EngineTestContext* context = GetGlobalContext<EngineTestContext>();

    if (context == nullptr)
    {
        if (!condition)
        {
            HYP_LOG(Engine, Error, "[FAIL] {} {} (outside of a test)", checkName, detail);
        }

        return condition;
    }

    return context->Check(checkName, condition, detail);
}

} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS
