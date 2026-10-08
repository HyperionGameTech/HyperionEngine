#include <Core/Logging/Logger.hpp>

#include <Asset/BlobStorage.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Framework/Game.hpp>

#include <HyperionEngine.hpp>

#include <emscripten.h>
#include <emscripten/wasmfs.h>

#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>

using namespace Hyperion;

// Everything the engine reads lives under /hyperion: Config, and the cooked package in Cache and Content.
static void MountPackage()
{
#ifdef HYP_WEB_NODE_PACKAGE_DIR
    // node test runs read the package straight from disk
    backend_t backend = wasmfs_create_node_backend(HYP_WEB_NODE_PACKAGE_DIR);
    wasmfs_create_directory("/hyperion", 0777, backend);
#else
    // Config and Content are preloaded. The cache block files are fetched on first use,
    // and linked into a normal directory so the engine can still write beside them.
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

int main(int argc, char** argv)
{
    MountPackage();

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

    if (argc <= 1)
    {
        argc = int(sizeof(defaultArguments) / sizeof(defaultArguments[0]));
        argv = const_cast<char**>(defaultArguments);
    }

    if (!Hyp_Initialize(argc, argv))
    {
        return 1;
    }

    // Mapping a file copies it into the heap here, and the blob storage unmaps its block files whenever its last reader
    // lets go. Holding one reader for the life of the app keeps a single copy instead of re-reading them over and over.
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
