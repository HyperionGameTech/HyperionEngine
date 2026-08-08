/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Framework/Commandlet/Commandlet.hpp>
#include <Framework/Commandlet/Commandlets/CacheServerCommandlet.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetBucket.hpp>
#include <Asset/AssetObject.hpp>
#include <Asset/BlobStorage.hpp>
#include <Asset/BlobStorageStructs.hpp>
#include <Asset/CookManifest.hpp>
#include <Asset/SerializationUtils.hpp>

#include <Core/CLI/CommandLine.hpp>
#include <Core/Core.hpp>

#include <Core/IO/ByteWriter.hpp>
#include <Core/IO/ByteReader.hpp>

#include <Core/Reflection/ClassUtils.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Containers/String.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Utilities/Time.hpp>
#include <Core/Utilities/GlobalContext.hpp>

#include <Core/Threading/ThreadPool.hpp>

#if defined(HYP_UNIX) || defined(HYP_ANDROID)

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

using SocketHandle = int;
static constexpr SocketHandle INVALID_SOCK = -1;
#define CLOSE_SOCKET close

#elif defined(HYP_WINDOWS)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

using SocketHandle = SOCKET;
static constexpr SocketHandle INVALID_SOCK = INVALID_SOCKET;
#define CLOSE_SOCKET closesocket

#endif

HYP_DISABLE_OPTIMIZATION;

namespace Hyperion {

struct CacheServerContext {};

class CacheServerCommandlet final : public CommandletBase
{
    HYP_OBJECT_BODY(CacheServerCommandlet);

    struct BlobLookupEntry
    {
        uint32 bucketIndex;
        Name assetName;
        const char* magic; // constant string
        uint64 size;
    };

    struct ServerState
    {
        CookManifest manifest;
        BlobStorage* blobStorage = nullptr;
        FilePath cacheDir;
        Map<uint64, BlobLookupEntry> blobLookup;
        Mutex mutex;

        bool devServer = false;
    };

    static void RebuildManifest(ServerState& state, Handle<AssetRegistry>& engineRegistry, Handle<AssetRegistry>& gameRegistry)
    {
        Mutex::Guard guard(state.mutex);

        state.manifest.assets.Clear();
        state.blobLookup.Clear();
        state.manifest.cookTimestamp = uint64(Time::Now());

        auto collectFromRegistry = [&state](AssetRegistry* registry, AssetRegistryId registryId)
        {
            if (!registry)
                return;

            for (uint32 bucketIndex = 1; bucketIndex < MaxAssetBuckets; bucketIndex++)
            {
                Array<AssetDesc> assetDescs;
                registry->GetBucketAssetDescs(bucketIndex, assetDescs);

                const AssetBucket& bucket = *AssetBuckets::AllBuckets[bucketIndex];

                for (const AssetDesc& assetDesc : assetDescs)
                {
                    Handle<AssetObject> assetObject = registry->GetAsset(bucket, assetDesc.name);

                    if (!assetObject.IsValid() || assetObject->IsTransient())
                        continue;

                    Array<Tuple<const char*, uint16, BlobDataReference*>> blobRefs;
                    assetObject->CollectBlobDataReferences(blobRefs);

                    if (blobRefs.Empty())
                        continue;

                    AssetEntry assetEntry;
                    assetEntry.registryId = registryId;
                    assetEntry.bucketIndex = bucketIndex;
                    assetEntry.name = assetObject->GetName();

                    {
                        FilePath hmfPath = registry->GetManifestPath(assetObject->GetPath());
                        if (hmfPath.Exists())
                            assetEntry.lastModifiedTimestamp = uint64(hmfPath.LastModifiedTimestamp());
                    }

                    for (auto& tup : blobRefs)
                    {
                        BlobDataReference* ref = tup.GetElement<2>();
                        if (!ref->key || ref->size == 0)
                            continue;

                        const uint64 key = ref->key.GetHashCode().Value();
                        const char* magic = tup.GetElement<0>();

                        BlobEntry blobEntry;
                        blobEntry.key = key;
                        blobEntry.size = ref->size;
                        blobEntry.magic = magic;
                        assetEntry.blobs.PushBack(std::move(blobEntry));

                        BlobLookupEntry& entry = state.blobLookup[key];
                        entry = {};
                        entry.bucketIndex = bucketIndex;
                        entry.assetName = assetEntry.name;
                        entry.size = ref->size;
                        entry.magic = magic;
                    }

                    state.manifest.assets.PushBack(std::move(assetEntry));
                }
            }
        };

        collectFromRegistry(engineRegistry.Get(), AssetRegistryId::Engine);
        collectFromRegistry(gameRegistry.Get(), AssetRegistryId::Game);

        if (state.manifest.assets.Any())
        {
            std::sort(state.manifest.assets.Data(),
                state.manifest.assets.Data() + state.manifest.assets.Size(),
                [](const AssetEntry& a, const AssetEntry& b)
                {
                    return a.lastModifiedTimestamp > b.lastModifiedTimestamp;
                });
        }

        HYP_LOG(Assets, Info, "CacheServer manifest rebuilt: {} assets, timestamp={}",
            state.manifest.assets.Size(), state.manifest.cookTimestamp);
    }

public:
    virtual ~CacheServerCommandlet() override = default;

    HYP_METHOD()
    static const CommandLineArgumentDefinitions& GetArgumentDefinitions()
    {
        static CommandLineArgumentDefinitions s_definitions;

        static bool s_initialized = false;
        if (!s_initialized)
        {
            s_initialized = true;

            s_definitions.Add(
                "project",
                "",
                "Project directory to serve assets from",
                CommandLineArgumentFlags::REQUIRED,
                {},
                "");

            s_definitions.Add(
                "port",
                "p",
                "Port to listen on",
                CommandLineArgumentFlags::NONE,
                {},
                JSON::Value(8080));

            s_definitions.Add(
                "dev",
                "",
                "Is devserver (enables serving of inline / non-cooked cache assets)",
                CommandLineArgumentFlags::NONE,
                {},
                false);
        }

        return s_definitions;
    }

protected:
    static FilePath MakeServeDir(const String& projectDir)
    {
        
        return FilePath(projectDir.StartsWith(".")
            // Relative path - starts with . (eg "../Foo" or "./Foo")
            ? (CoreApi::GetExecutablePath() / projectDir)
            // Just use provided path.
            : projectDir).ToCanonical();
    }

    virtual Result Run_Impl(const CommandLineArguments& args) override
    {
        GlobalContextScope scope { CacheServerContext() };

        const FilePath cacheDir = MakeServeDir(args["project"].ToString());

        int32 port = args["port"].ToInt32();

        if (!cacheDir.Exists() || !cacheDir.IsDirectory())
        {
            return HYP_MAKE_ERROR(Error, "Directory does not exist: {}", cacheDir);
        }

        ServerState state;
        state.cacheDir = cacheDir;
        state.devServer = args["dev"].ToBool();

        if (!state.devServer)
        {
            // Open BlobStorage in read-only mode to serve blob data by key
            state.blobStorage = new BlobStorage(cacheDir, /* readOnly */ true);
            state.blobStorage->Initialize();
        }

        HYP_DEFER({
            if (state.blobStorage != nullptr)
            {
                state.blobStorage->Shutdown();

                delete state.blobStorage;
                state.blobStorage = nullptr;
            }
        });

        HYP_LOG(Assets, Info, "{}CacheServer starting on port {}, serving '{}'", state.devServer ? "[DEV] " : "", port, cacheDir);

        Handle<AssetRegistry> engineRegistry = GetEngineAssetRegistry();
        if (engineRegistry.IsValid())
        {
            engineRegistry->LoadAssetDescs();
        }

        Handle<AssetRegistry> gameRegistry = MakeHandle<AssetRegistry>(AssetRegistryId::Game, cacheDir);

        GlobalContextScope assetRegistryScope { AssetRegistryContext { gameRegistry } };

        gameRegistry->LoadAssetDescs();

        // Build initial manifest and start a background poller to pick up
        // file changes while the server is running.
        RebuildManifest(state, engineRegistry, gameRegistry);

        // Start HTTP server
#if defined(HYP_WINDOWS)
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        {
            return HYP_MAKE_ERROR(Error, "WSAStartup failed");
        }
#endif

        SocketHandle listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#if defined(HYP_WINDOWS)
        HYP_DEFER({ WSACleanup(); });
#endif

        if (listenSock == INVALID_SOCK)
        {
            return HYP_MAKE_ERROR(Error, "Failed to create socket");
        }

        int reuse = 1;
        setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR,
#if defined(HYP_WINDOWS)
            (const char*)&reuse,
#else
            &reuse,
#endif
            sizeof(reuse));

        struct sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(uint16(port));

        if (bind(listenSock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        {
            CLOSE_SOCKET(listenSock);

            return HYP_MAKE_ERROR(Error, "Failed to bind to port {}", port);
        }

        if (listen(listenSock, 8) < 0)
        {
            CLOSE_SOCKET(listenSock);

            return HYP_MAKE_ERROR(Error, "Failed to listen on port {}", port);
        }

        HYP_LOG(Assets, Info, "CacheServer listening on port {}", port);

        using threading::TaskThreadPool;

        TaskThreadPool serverPool("CacheServerWorker", 8);
        serverPool.Start();

        HYP_DEFER({ serverPool.Stop(); });

        // Background poller thread
        AtomicVar<bool> pollerShouldStop = false;
        std::thread pollerThread([&]()
        {
            while (!pollerShouldStop.Get(MemoryOrder::RELAXED))
            {
                ThreadSleep(5000);

                if (pollerShouldStop.Get(MemoryOrder::RELAXED))
                    break;

                RebuildManifest(state, engineRegistry, gameRegistry);
            }
        });

        HYP_DEFER({
            pollerShouldStop.Set(true, MemoryOrder::RELAXED);
            if (pollerThread.joinable())
                pollerThread.join();
        });

        List<Task<void>> tasks;

        for (;;)
        {
            // Remove completed tasks.
            for (auto it = tasks.Begin(); it != tasks.End();)
            {
                if (it->IsCompleted())
                {
                    it->Await();

                    it = tasks.Erase(it);
                    
                    continue;
                }

                ++it;
            }

            struct sockaddr_in clientAddr = {};
            socklen_t clientAddrLen = sizeof(clientAddr);

            SocketHandle clientSock = accept(listenSock, (struct sockaddr*)&clientAddr, &clientAddrLen);
            if (clientSock == INVALID_SOCK)
            {
                continue;
            }

            tasks.EmplaceBack(serverPool.Enqueue(HYP_STATIC_MESSAGE("CacheServerRequest"), [&state, clientSock, &gameRegistry]()
            {
                char recvBuf[8192];

                int bytesRead = recv(clientSock, recvBuf, sizeof(recvBuf) - 1, 0);

                if (bytesRead <= 0)
                {
                    CLOSE_SOCKET(clientSock);
                    return;
                }

                recvBuf[bytesRead] = '\0';

                const char* reqLineEnd = strstr(recvBuf, "\r\n");
                if (reqLineEnd == nullptr)
                {
                    CLOSE_SOCKET(clientSock);
                    return;
                }

                const char* pathStart = strchr(recvBuf, ' ');
                if (pathStart == nullptr)
                {
                    CLOSE_SOCKET(clientSock);
                    return;
                }
                pathStart++;

                const char* pathEnd = strchr(pathStart, ' ');
                if (!pathEnd)
                {
                    pathEnd = reqLineEnd;
                }

                String path(pathStart, pathStart + size_t(pathEnd - pathStart));

                HYP_LOG(Assets, Info, "[CacheServer] GET {}", path);

                if (path == "/manifest")
                {
                    String hmfText;
                    {
                        Mutex::Guard guard(state.mutex);
                        ObjectToHMF(GetClass<CookManifest>(), BoxedValue(state.manifest), hmfText);
                    }

                    char header[256];
                    int headerLen = std::snprintf(header, sizeof(header),
                        "HTTP/1.0 200 OK\r\n"
                        "Content-Type: application/octet-stream\r\n"
                        "Content-Length: %zu\r\n"
                        "\r\n",
                        hmfText.Size());

                    send(clientSock, header, headerLen, 0);
                    send(clientSock, hmfText.Data(), int(hmfText.Size()), 0);
                }
                else if (path.StartsWith("/hmf/"))
                {
                    String subPath = path.Substr(5);
                    size_t slashPos = subPath.FindFirstIndex("/");

                    if (slashPos == String::NotFound)
                    {
                        const char* bad = "HTTP/1.0 400 Bad Request\r\n"
                            "Content-Length: 0\r\n\r\n";
                        
                        send(clientSock, bad, int(strlen(bad)), 0);
                        CLOSE_SOCKET(clientSock);

                        return;
                    }

                    uint32 bucketIndex = uint32(std::atoi(subPath.Substr(0, slashPos).Data()));
                    String assetName = subPath.Substr(slashPos + 1);

                    bool served = false;

                    if (bucketIndex >= 1 && bucketIndex < MaxAssetBuckets)
                    {
                        String assetBucketStr = String(GetAssetBucketName(bucketIndex));

                        FilePath hmfPath = gameRegistry->GetRootPath() / assetBucketStr / (assetName + ".hmf");

                        if (!hmfPath.Exists())
                        {
                            hmfPath = EngineGlobals::GetContentDirectory<HYP_STATIC_STRING("Engine")>() / assetBucketStr / (assetName + ".hmf");
                        }

                        if (hmfPath.Exists())
                        {
                            FileByteReader reader { hmfPath };

                            ByteBuffer data = reader.Read();

                            char header[256];
                            int headerLen = std::snprintf(header, sizeof(header),
                                "HTTP/1.0 200 OK\r\n"
                                "Content-Type: application/octet-stream\r\n"
                                "Content-Length: %zu\r\n"
                                "\r\n",
                                data.Size());

                            send(clientSock, header, headerLen, 0);
                            send(clientSock, (const char*)data.Data(), int(data.Size()), 0);

                            served = true;
                        }
                    }

                    if (!served)
                    {
                        const char* notFound = "HTTP/1.0 404 Not Found\r\n"
                                               "Content-Length: 0\r\n\r\n";

                        send(clientSock, notFound, int(strlen(notFound)), 0);
                    }
                }
                else if (path.StartsWith("/blob?"))
                {
                    String query = path.Substr(6);
                    size_t keyStart = query.FindFirstIndex("key=");
                    size_t sizeStart = query.FindFirstIndex("&size=");

                    if (keyStart == String::NotFound || sizeStart == String::NotFound)
                    {
                        const char* bad = "HTTP/1.0 400 Bad Request\r\n"
                                          "Content-Length: 0\r\n\r\n";

                        send(clientSock, bad, int(strlen(bad)), 0);
                        CLOSE_SOCKET(clientSock);
                        return;
                    }

                    String keyHex = query.Substr(keyStart + 4, sizeStart);
                    String sizeStr = query.Substr(sizeStart + 6);

                    uint64 keyValue = 0;
                    size_t hexCount = 0;

                    for (size_t i = 0; i < keyHex.Size() && keyHex[i] != '&'; i++)
                    {
                        char c = keyHex[i];
                        uint64 digit;

                        if (c >= '0' && c <= '9')
                        {
                            digit = c - '0';
                        }
                        else if (c >= 'a' && c <= 'f')
                        {
                            digit = c - 'a' + 10;
                        }
                        else if (c >= 'A' && c <= 'F')
                        {
                            digit = c - 'A' + 10;
                        }
                        else
                        {
                            break;
                        }

                        if (++hexCount > 16)
                        {
                            break;
                        }

                        keyValue = (keyValue << 4) | digit;
                    }

                    uint64 sizeValue = 0;

                    for (size_t i = 0; i < sizeStr.Size() && sizeStr[i] >= '0' && sizeStr[i] <= '9'; i++)
                    {
                        sizeValue = sizeValue * 10 + uint64(sizeStr[i] - '0');
                    }

                    bool served = false;

                    if (state.devServer)
                    {
                        auto lookupIt = state.blobLookup.Find(keyValue);
                        if (lookupIt != state.blobLookup.End())
                        {
                            const BlobLookupEntry& entry = lookupIt->second;

                            static const auto s_getBlobPath = [](const BlobLookupEntry& entry, const FilePath& contentDir)
                            {
                                return contentDir
                                    / String(GetAssetBucketName(entry.bucketIndex))
                                    / (entry.assetName.ToString() + "." + entry.magic + ".raw.blob");
                            };
                            
                            FilePath rawBlobPath = s_getBlobPath(entry, gameRegistry->GetRootPath());

                            if (!rawBlobPath.Exists())
                            {
                                rawBlobPath = s_getBlobPath(entry, EngineGlobals::GetContentDirectory<HYP_STATIC_STRING("Engine")>());
                            }

                            if (rawBlobPath.Exists())
                            {
                                FileByteReader reader { rawBlobPath };

                                if (!reader.Eof())
                                {
                                    ByteBuffer data = reader.Read();

                                    char header[256];
                                    int headerLen = std::snprintf(header, sizeof(header),
                                        "HTTP/1.0 200 OK\r\n"
                                        "Content-Type: application/octet-stream\r\n"
                                        "Content-Length: %zu\r\n"
                                        "\r\n",
                                        data.Size());

                                    send(clientSock, header, headerLen, 0);
                                    send(clientSock, (const char*)data.Data(), int(data.Size()), 0);

                                    served = true;
                                }
                            }
                        }
                    }
                    else
                    {
                        void* blobRaw = nullptr;
                        if (state.blobStorage->GetData(StringHash(keyValue), size_t(sizeValue), blobRaw))
                        {
                            char header[256];
                            int headerLen = std::snprintf(header, sizeof(header),
                                "HTTP/1.0 200 OK\r\n"
                                "Content-Type: application/octet-stream\r\n"
                                "Content-Length: %llu\r\n"
                                "\r\n",
                                (unsigned long long)sizeValue);
                            send(clientSock, header, headerLen, 0);
                            send(clientSock, (const char*)blobRaw, int(sizeValue), 0);

                            served = true;
                        }
                    }

                    if (!served)
                    {
                        const char* notFound = "HTTP/1.0 404 Not Found\r\n"
                            "Content-Length: 0\r\n\r\n";
                        send(clientSock, notFound, int(strlen(notFound)), 0);
                    }
                }
                else
                {
                    const char* notFound = "HTTP/1.0 404 Not Found\r\n"
                        "Content-Length: 0\r\n\r\n";
                    send(clientSock, notFound, int(strlen(notFound)), 0);
                }

                CLOSE_SOCKET(clientSock);
            }));
        }

        for (Task<void>& task : tasks)
        {
            task.Await();
        }

        CLOSE_SOCKET(listenSock);

        return {};
    }
};

HYP_EXPORT const Class* g_clsCacheServerCommandlet = nullptr;

const Class* CacheServerCommandlet::StaticClass()
{
    return g_clsCacheServerCommandlet;
}

// clang-format off

HYP_BEGIN_CLASS(CacheServerCommandlet, -1, 0, NAME("CommandletBase"), ClassAttribute("command", "cacheserver"))
    Method(NAME("GetArgumentDefinitions"), &Type::GetArgumentDefinitions)
HYP_END_CLASS

// clang-format on

HYP_REGISTER_STATIC_CLASS(CacheServerCommandlet);

} // namespace Hyperion