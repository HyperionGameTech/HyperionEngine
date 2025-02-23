#include "SampleStreamer.hpp"

#include <editor/HyperionEditor.hpp>

#include <system/App.hpp>

#include <core/debug/StackDump.hpp>

#include <core/cli/CommandLine.hpp>

#include <core/containers/Bitset.hpp>

#include <core/logging/Logger.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/UUID.hpp>

#include <core/utilities/Tuple.hpp>
#include <core/utilities/Format.hpp>

#include <HyperionEngine.hpp>

using namespace hyperion;

namespace hyperion {
HYP_DEFINE_LOG_CHANNEL(Core);
}

void HandleSignal(int signum)
{
    // Dump stack trace
    DebugLog(
        LogType::Warn,
        "Received signal %d\n",
        signum
    );

    DebugLog(LogType::Debug, "%s\n", StackDump().ToString().Data());

    if (Engine::GetInstance()->m_stop_requested.Get(MemoryOrder::RELAXED)) {
        DebugLog(
            LogType::Warn,
            "Forcing stop\n"
        );

        fflush(stdout);

        exit(signum);

        return;
    }

    Engine::GetInstance()->RequestStop();

    // Wait for the render loop to stop
    while (Engine::GetInstance()->IsRenderLoopActive());

    exit(signum);
}

int main(int argc, char **argv)
{
    signal(SIGINT, HandleSignal);
    
    // handle fatal crashes
    signal(SIGSEGV, HandleSignal);

    HyperionEditor editor;
    // SampleStreamer editor;
    App app;

    CommandLineParser arg_parse { DefaultCommandLineArgumentDefinitions() };
    auto parse_result = arg_parse.Parse(argc, argv);

    if (parse_result.HasValue()) {
        app.Launch(&editor, parse_result.GetValue());
    } else {
        const Error &error = parse_result.GetError();

        DebugLog(
            LogType::Error,
            "Failed to parse arguments!\n\t%s\n",
            error.GetMessage().Any()
                ? error.GetMessage().Data()
                : "<no message>"
        );

        return 1;
    }

    return 0;
}
