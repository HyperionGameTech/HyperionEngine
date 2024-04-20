#include "SampleStreamer.hpp"
#include <system/StackDump.hpp>
#include <util/ArgParse.hpp>

#include <HyperionEngine.hpp>

using namespace hyperion;

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

template <class T>
constexpr auto FuncName()
{
    return __FUNCSIG__;
}

template <auto Str>
constexpr auto StripNamespace()
{
    constexpr auto last_index = Str.template FindLast<containers::detail::IntegerSequenceFromString<StaticString("::")>>();

    if constexpr (last_index == SizeType(-1)) {
        return detail::StripClassOrStruct< Str >();
    } else {
        return detail::StripClassOrStruct< Str.template Substr<last_index + 2, Str.Size() - 1>() >();
    }
}

template <auto Str>
constexpr auto StripNestedNamespace()
{
    constexpr auto left_arrow_index = Str.template FindFirst< containers::detail::IntegerSequenceFromString< StaticString("<") > >();
    constexpr auto right_arrow_index = Str.template FindLast< containers::detail::IntegerSequenceFromString< StaticString(">") > >();

    if constexpr (left_arrow_index != SizeType(-1) && right_arrow_index != SizeType(-1)) {

        // constexpr auto after_right_arrow = Str.template Substr<right_arrow_index + 1, Str.Size() - 1>();
        return StripNestedNamespace< Str.template Substr<0, left_arrow_index>() >()
            .template Concat< StaticString("<")>()
            .template Concat< StripNestedNamespace< Str.template Substr<left_arrow_index + 1, right_arrow_index>() >() >()
            .template Concat<StaticString(">")>();

    } else if constexpr (left_arrow_index != -1 || right_arrow_index != -1) {
        static_assert(resolution_failure<containers::detail::IntegerSequenceFromString< Str > >, "Mismatched < and > in type name");
    } else {
        return StripNamespace< Str >();
    }
}

int main(int argc, char **argv)
{
    signal(SIGINT, HandleSignal);
    
    // handle fatal crashes
    signal(SIGSEGV, HandleSignal);

    WindowFlags window_flags = WINDOW_FLAGS_HIGH_DPI;

    ArgParse arg_parse;
    arg_parse.Add("headless", String::empty, ArgParse::ARG_FLAGS_NONE, ArgParse::ARGUMENT_TYPE_BOOL, false);    
    arg_parse.Add("mode", "m", ArgParse::ARG_FLAGS_NONE, Array<String> { "precompile_shaders", "streamer" }, String("streamer"));

    if (auto parse_result = arg_parse.Parse(argc, argv)) {
        if (const bool *headless_ptr = parse_result["headless"].TryGet<bool>()) {
            if (*headless_ptr) {
                window_flags |= WINDOW_FLAGS_HEADLESS;
            }
        }

        if (const String *mode_str = parse_result["mode"].TryGet<String>()) {
            if (*mode_str == "precompile_shaders") {
                window_flags |= WINDOW_FLAGS_NO_GFX;

                if (!Engine::GetInstance()->GetShaderCompiler().LoadShaderDefinitions(true)) {
                    DebugLog(LogType::Error, "Shader precompilation failed!\n");

                    std::exit(1);
                } else {
                    DebugLog(LogType::Info, "Precompiled shaders successfuly\n");

                    std::exit(0);
                }
            }
        }
    }
    
    RC<Application> application(new SDLApplication("My Application", argc, argv));

    if (!(window_flags & WINDOW_FLAGS_NO_GFX)) {
        DebugLog(
            LogType::Info,
            "Creating window with flags: %u\n",
            window_flags
        );

        application->SetCurrentWindow(application->CreateSystemWindow({
            "Hyperion Engine",
            { 1280, 720 },
            window_flags
        }));
    }

    hyperion::InitializeApplication(application);

    auto my_game = SampleStreamer(application);
    Engine::GetInstance()->InitializeGame(&my_game);

    SystemEvent event;

    uint num_frames = 0;
    float delta_time_accum = 0.0f;
    GameCounter counter;

    // AssertThrow(server.Start());

    while (Engine::GetInstance()->IsRenderLoopActive()) {
        // input manager stuff
        while (application->PollEvent(event)) {
            my_game.HandleEvent(std::move(event));
        }

        counter.NextTick();
        delta_time_accum += counter.delta;
        num_frames++;

        if (num_frames >= 250) {
            DebugLog(
                LogType::Debug,
                "Render FPS: %f\n",
                1.0f / (delta_time_accum / float(num_frames))
            );

            delta_time_accum = 0.0f;
            num_frames = 0;
        }

        Engine::GetInstance()->RenderNextFrame(&my_game);
    }

    hyperion::ShutdownApplication();

    return 0;
}
