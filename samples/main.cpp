#include "SampleStreamer.hpp"

#include <HyperionEngine.hpp>
#include <core/lib/AtomicVar.hpp>

using namespace hyperion;

void HandleSignal(int signum)
{
    if (g_engine->m_stop_requested.Get(MemoryOrder::RELAXED)) {
        DebugLog(
            LogType::Warn,
            "Forcing stop\n"
        );

        fflush(stdout);

        exit(signum);

        return;
    }

    g_engine->RequestStop();

    while (g_engine->IsRenderLoopActive());

    exit(signum);
}

int main(int argc, char **argv)
{
    signal(SIGINT, HandleSignal);
    
    RC<Application> application(new SDLApplication("My Application", argc, argv));
    application->SetCurrentWindow(application->CreateSystemWindow({
        "Hyperion Engine",
        1920, 1080,
        true
    }));
    
    hyperion::InitializeApplication(application);

    auto my_game = SampleStreamer(application);
    g_engine->InitializeGame(&my_game);

    UInt num_frames = 0;
    float delta_time_accum = 0.0f;
    GameCounter counter;

    SystemEvent event;

    // AssertThrow(server.Start());

     while (g_engine->IsRenderLoopActive()) {
        // input manager stuff
        while (application->PollEvent(event)) {
            my_game.HandleEvent(std::move(event));
        }

        g_engine->RenderNextFrame(&my_game);
    }

    return 0;
}
