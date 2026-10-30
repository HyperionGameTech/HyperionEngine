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

        return tests::EngineTestRunner::RunTests(tests::EngineTestHost::Engine, filter, listOnly, resultsPathString);
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
