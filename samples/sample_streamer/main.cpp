#include "SampleStreamer.hpp"

#include <editor/HyperionEditor.hpp>

#include <core/system/App.hpp>
#include <core/system/StackDump.hpp>
#include <core/system/ArgParse.hpp>

#include <core/containers/Bitset.hpp>

#include <core/logging/Logger.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/UUID.hpp>

#include <core/utilities/Tuple.hpp>
#include <core/utilities/Format.hpp>

#include <asset/serialization/fbom/FBOM.hpp>

#include <HyperionEngine.hpp>

#include "asset/font_loaders/FontAtlasLoader.hpp"
#include "asset/ui_loaders/UILoader.hpp"

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

struct TestStruct{ Name name; };

int main(int argc, char **argv)
{
    signal(SIGINT, HandleSignal);
    
    // handle fatal crashes
    signal(SIGSEGV, HandleSignal);

#if 0
    fbom::FBOMObject test_obj(fbom::FBOMObjectType("FooBarbazz"));
    test_obj.SetProperty("Blah", fbom::FBOMUnsignedInt(), 123u);
    // test_obj.SetProperty("Foo", fbom::FBOMString(), String("hello string test"));

    fbom::FBOMObject test_sub_obj(fbom::FBOMObjectType("BlehBleh"));
    test_sub_obj.SetProperty("PI", fbom::FBOMFloat(), 3.1415f);

    test_obj.AddChild(std::move(test_sub_obj));

    fbom::FBOMData test_data = fbom::FBOMData::FromObject(test_obj);

    DebugLog(LogType::Debug, "test_data type: %s\n", test_data.GetType().name.Data());

    fbom::FBOMObject test_data_loaded;
    fbom::FBOMResult read_object_result = test_data.ReadObject(test_data_loaded);
    AssertThrowMsg(read_object_result == fbom::FBOMResult::FBOM_OK, "Failed to read object : %s", read_object_result.message);

    DebugLog(LogType::Debug, "test_data total size: %u\n", test_data.TotalSize());

    uint32 test_data_loaded_blah1;
    test_data_loaded.GetProperty("Blah").ReadUnsignedInt(&test_data_loaded_blah1);
    DebugLog(LogType::Debug, "test_data_loaded Blah: %u\n", test_data_loaded_blah1);

    float nested_value;
    fbom::FBOMResult nested_read_result = test_data_loaded.nodes->Front().GetProperty("PI").ReadFloat(&nested_value);
    AssertThrowMsg(nested_read_result == fbom::FBOMResult::FBOM_OK, "Failed to read nested value : %s", nested_read_result.message);

    DebugLog(LogType::Debug, "test_data_loaded sub PI: %f\n", nested_value);

    HYP_BREAKPOINT;
#endif

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
