/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#ifdef HYP_TESTS

#include <HyperionPch.hpp>

#include <Framework/Testing/EngineTest.hpp>

#include <Framework/Commandlet/Commandlet.hpp>

#include <Core/CLI/CommandLine.hpp>

#include <Core/DataProcessing/JSON/JSON.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

#include <Core/Reflection/ClassRegistry.hpp>
#include <Core/Reflection/Method.hpp>

#include <Plugins/PluginManager.hpp>

namespace Hyperion {

class EngineTestCommandlet final : public CommandletBase
{
    HYP_OBJECT_BODY(EngineTestCommandlet);

public:
    virtual ~EngineTestCommandlet() override = default;

    static const CommandLineArgumentDefinitions& GetArgumentDefinitions()
    {
        static const CommandLineArgumentDefinitions s_definitions = []()
        {
            CommandLineArgumentDefinitions definitions;
            definitions.Add("filter", "", "Only run these tests by name", CommandLineArgumentFlags::NONE, CommandLineArgumentType::STRING, JSON::Value(""));
            definitions.Add("list", "", "List the tests in the report", CommandLineArgumentFlags::NONE, CommandLineArgumentType::BOOLEAN, JSON::Value(false));
            definitions.Add("results", "", "File to write the report to", CommandLineArgumentFlags::NONE, CommandLineArgumentType::STRING, JSON::Value(""));

            return definitions;
        }();

        return s_definitions;
    }

protected:
    virtual Result Run(const CommandLineArguments& args) override
    {
        const String filter = args["filter"].ToString();
        const bool listOnly = args["list"].ToBool();
        const String resultsPathString = args["results"].ToString();

        JSON::JArray resultsArray;
        uint32 numFailed = 0;

        // as a game would: the default plugins load, so plugin tests run with what their modules register
        if (!listOnly && !PluginManager::GetInstance().IsInitialized())
        {
            PluginManager::GetInstance().Initialize(PluginHostMode::Game);
        }

        // plugin tests register as their libraries load, so a listing loads them too
        if (listOnly)
        {
            PluginManager::GetInstance().LoadAllPluginLibraries();
        }

        // rewritten after each test, so a test that crashes the process still leaves the results before it
        const auto writeReport = [&]() -> Result
        {
            if (resultsPathString.Empty())
            {
                return {};
            }

            JSON::Object report {
                { "phase", JSON::Value(String(listOnly ? "list" : "engine")) },
                { "tests", JSON::Value(resultsArray) }
            };

            const String reportText = JSON::Value(std::move(report)).ToString(true);

            FileByteWriter writer { FilePath(resultsPathString) };

            if (!writer.IsOpen())
            {
                return HYP_MAKE_ERROR(Error, "Failed to open {} for writing", resultsPathString);
            }

            writer.Write(reportText.Data(), reportText.Size());
            writer.Close();

            return {};
        };

        for (const tests::EngineTestDefinition& test : tests::EngineTestRegistry::GetInstance().GetTests())
        {
            if (!tests::EngineTestRunner::MatchesFilter(test, filter))
            {
                continue;
            }

            if (listOnly)
            {
                resultsArray.PushBack(JSON::Object { { "name", JSON::Value(tests::EngineTestRunner::GetFullName(test)) } });

                continue;
            }

            const tests::EngineTestResult result = tests::EngineTestRunner::Run(test);

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
            HYP_LOG(Engine, Info, "Engine tests: {} passed, {} failed", resultsArray.Size() - numFailed, numFailed);
        }

        if (Result writeResult = writeReport(); writeResult.HasError())
        {
            return writeResult;
        }

        if (numFailed != 0)
        {
            return HYP_MAKE_ERROR(Error, "{} engine test(s) failed", numFailed);
        }

        return {};
    }
};

HYP_EXPORT const Class* g_clsEngineTestCommandlet = nullptr;

const Class* EngineTestCommandlet::StaticClass()
{
    return g_clsEngineTestCommandlet;
}

HYP_BEGIN_CLASS(EngineTestCommandlet, -1, 0, NAME("CommandletBase"), ClassAttribute("command", "enginetest"))
    Method(NAME("GetArgumentDefinitions"), &Type::GetArgumentDefinitions)
HYP_END_CLASS

HYP_REGISTER_STATIC_CLASS(EngineTestCommandlet);

} // namespace Hyperion

#endif // HYP_TESTS
