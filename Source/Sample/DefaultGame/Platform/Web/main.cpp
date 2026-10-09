#include <Core/Logging/Logger.hpp>

#include <Asset/BlobStorage.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Framework/Game.hpp>
#include <Framework/Threads/SimThread.hpp>

#include <HyperionEngine.hpp>

#include <emscripten.h>
#include <emscripten/wasmfs.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <thread>

using namespace Hyperion;

static void MountPackage()
{
#ifdef HYP_WEB_NODE_PACKAGE_DIR
    backend_t backend = wasmfs_create_node_backend(HYP_WEB_NODE_PACKAGE_DIR);
    wasmfs_create_directory("/hyperion", 0777, backend);
#else

    // Config and Content are preloaded
    chmod("/hyperion", 0777); // preloaded directories come out read-only
    mkdir("/hyperion/Cache", 0777);

    backend_t backend = wasmfs_create_fetch_backend("Cache", 16 * 1024 * 1024);
    wasmfs_create_directory("/package", 0777, backend);

    close(wasmfs_create_file("/package/index.txt", 0444, backend));

    FILE* indexFile = fopen("/package/index.txt", "r");

    if (indexFile == nullptr)
    {
        fprintf(stderr, "No Cache/index.txt, the cooked package is missing\n");

        return;
    }

    char name[256];

    while (fgets(name, sizeof(name), indexFile) != nullptr)
    {
        name[strcspn(name, "\r\n")] = '\0';

        if (name[0] == '\0')
        {
            continue;
        }

        char sourcePath[512];
        char linkPath[512];
        snprintf(sourcePath, sizeof(sourcePath), "/package/%s", name);
        snprintf(linkPath, sizeof(linkPath), "/hyperion/Cache/%s", name);

        close(wasmfs_create_file(sourcePath, 0444, backend));
        symlink(sourcePath, linkPath);
    }

    fclose(indexFile);
#endif
}

#ifndef HYP_WEB_NODE_PACKAGE_DIR

static void PrefetchCacheFiles()
{
    std::thread([]()
        {
            static constexpr size_t readSize = 4u * 1024u * 1024u;

            void* buffer = std::malloc(readSize);

            for (const char* path : { "/hyperion/Cache/Meshes.bin", "/hyperion/Cache/Textures.bin" })
            {
                const int file = open(path, O_RDONLY);

                if (file < 0)
                {
                    continue;
                }

                while (read(file, buffer, readSize) > 0)
                {
                }

                close(file);
            }

            std::free(buffer);
        })
        .detach();
}
#endif

extern "C" int Hyp_ExecuteConsoleCommand(int argc, const char** argv);
extern "C" void Hyp_GetAllCVarNames(void* callback, void* userData);
extern "C" void Hyp_GetAllCommandletNames(void* callback, void* userData);

extern "C" {

// The page's console calls this, on the page's own thread, with each line typed into it.
EMSCRIPTEN_KEEPALIVE void HypWeb_ExecuteConsoleLine(const char* line)
{
    if (g_simThreadInstance == nullptr)
    {
        return;
    }

    g_simThreadInstance->GetScheduler().Enqueue(
        [text = String(line).Trimmed()]()
        {
            Array<String> args = text.Split(' ');

            if (args.Empty())
            {
                return;
            }

            Array<const char*> argsCharV = MapToArray(args, [](const String& str)
                {
                    return str.Data();
                });

            if (Hyp_ExecuteConsoleCommand(int(args.Size()), argsCharV.Data()) != 0)
            {
                fprintf(stderr, "`%s` is not a console variable or command, or its value is not valid\n", text.Data());
            }
            else
            {
                printf("ok\n");
            }
        },
        TaskEnqueueFlags::FIRE_AND_FORGET);
}

EMSCRIPTEN_KEEPALIVE const char* HypWeb_GetConsoleNames()
{
    static String s_names;

    if (s_names.Empty())
    {
        void (*appendName)(const char*, void*) = [](const char* name, void* userData)
        {
            String& names = *static_cast<String*>(userData);
            names.Append(name);
            names.Append("\n");
        };

        Hyp_GetAllCVarNames(reinterpret_cast<void*>(appendName), &s_names);
        Hyp_GetAllCommandletNames(reinterpret_cast<void*>(appendName), &s_names);
    }

    return s_names.Data();
}

} // extern "C"

int main(int argc, char** argv)
{
    MountPackage();

#ifndef HYP_WEB_NODE_PACKAGE_DIR
    PrefetchCacheFiles();
#endif

    const char* defaultArguments[] = {
        "hyperion-sample",
        "--basedir=/hyperion",
        "--cachedir=/hyperion/Cache",
        "--contentdir=/hyperion/Content",
        "--singleplayer=true",
#ifndef HYP_WEB_NODE_PACKAGE_DIR
        // the thread running main() owns the canvas
        "--RenderOnMainThread=true"
#endif
    };

    // The page passes each ?set=Name=Value as console variables to set before the game starts.
    Array<String> startupVariables;

    for (int index = 1; index < argc; index++)
    {
        startupVariables.PushBack(argv[index]);
    }

    argc = int(sizeof(defaultArguments) / sizeof(defaultArguments[0]));
    argv = const_cast<char**>(defaultArguments);

    if (!Hyp_Initialize(argc, argv))
    {
        return 1;
    }

    for (const String& startupVariable : startupVariables)
    {
        Array<String> parts = startupVariable.Split('=');

        if (parts.Size() != 2)
        {
            fprintf(stderr, "`%s` is not Name=Value\n", startupVariable.Data());

            continue;
        }

        const char* arguments[] = { parts[0].Data(), parts[1].Data() };

        if (Hyp_ExecuteConsoleCommand(2, arguments) != 0)
        {
            fprintf(stderr, "`%s` is not a console variable, or its value is not valid\n", startupVariable.Data());
        }
    }

    EngineGlobals::GetBlobStorage()->Lock(EngineGlobals::GetCacheDirectory(), /* readOnly */ true);

    Handle<Game> defaultGame = Game::CreateGame("DefaultGame"_sh);

    Hyp_SetGame(defaultGame.Get());

    if (!Hyp_LaunchThreads())
    {
        HYP_FAIL("Failed to launch threads!");
    }

    Hyp_Shutdown();

    return 0;
}
