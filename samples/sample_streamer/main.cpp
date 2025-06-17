#include <editor/HyperionEditor.hpp>

#include <system/App.hpp>

#include <core/debug/StackDump.hpp>

#include <core/containers/Bitset.hpp>

#include <core/logging/Logger.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/UUID.hpp>

#include <core/utilities/Tuple.hpp>
#include <core/utilities/Format.hpp>

#include <HyperionEngine.hpp>
#include <Engine.hpp>

// temp
#include <SDL2/SDL.h>

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
        signum);

    DebugLog(LogType::Debug, "%s\n", StackDump().ToString().Data());

    if (g_engine->m_stop_requested.Get(MemoryOrder::RELAXED))
    {
        DebugLog(
            LogType::Warn,
            "Forcing stop\n");

        fflush(stdout);

        exit(signum);

        return;
    }

    g_engine->RequestStop();

    // Wait for the render loop to stop
    while (g_engine->IsRenderLoopActive())
        ;

    exit(signum);
}

int main(int argc, char** argv)
{
    signal(SIGINT, HandleSignal);

    // handle fatal crashes
    signal(SIGSEGV, HandleSignal);

    if (!hyperion::InitializeEngine(argc, argv))
    {
        return 1;
    }

    Handle<Game> game = CreateObject<HyperionEditor>();

    App::GetInstance().LaunchGame(game);

    return 0;
}
