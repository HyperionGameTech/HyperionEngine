#include "SampleStreamer.hpp"

#include <editor/HyperionEditor.hpp>

#include <core/system/App.hpp>
#include <core/system/StackDump.hpp>
#include <core/system/ArgParse.hpp>

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

int main(int argc, char **argv)
{
    signal(SIGINT, HandleSignal);
    
    // handle fatal crashes
    signal(SIGSEGV, HandleSignal);

    // Bitset bs(0b1101011010001ull);
    // bs.Set(1000, true);

    // Bitset::BitIndex bit_index = Bitset::not_found;

    // do {
    //     bit_index = bs.NextSetBitIndex(bit_index + 1);

    //     DebugLog(LogType::Debug, "Bit index: %llu\n", bit_index);
    // } while (bit_index != Bitset::not_found);

    // HYP_BREAKPOINT;

    HyperionEditor editor;
    // SampleStreamer editor;
    App app;

    ArgParse arg_parse;
    arg_parse.Add("Headless", String::empty, ArgFlags::NONE, CommandLineArgumentType::BOOLEAN, false);    
    arg_parse.Add("Mode", "m", ArgFlags::NONE, Array<String> { "PrecompileShaders", "Streamer" }, String("Streamer"));

    if (auto parse_result = arg_parse.Parse(argc, argv)) {
        app.Launch(&editor, parse_result.result);
    } else {
        DebugLog(
            LogType::Error,
            "Failed to parse arguments!\n\t%s\n",
            parse_result.message.HasValue()
                ? parse_result.message->Data()
                : "<no message>"
        );

        return 1;
    }

    return 0;
}
