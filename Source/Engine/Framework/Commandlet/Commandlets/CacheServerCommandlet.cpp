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
#include <sys/inotify.h>
#include <poll.h>
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
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")

using SocketHandle = SOCKET;
static constexpr SocketHandle INVALID_SOCK = INVALID_SOCKET;
#define CLOSE_SOCKET closesocket

#endif

namespace Hyperion {

struct CacheServerContext {};

#if defined(HYP_WINDOWS)

class DirectoryWatcher
{
public:
    using Callback = ProcRef<void(const String& relativePath, bool)>;

    DirectoryWatcher(const FilePath& dirPath, const Callback& callback)
        : m_callback(callback),
          m_running(true)
    {
        m_dirHandle = CreateFileA(
            dirPath.Data(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            nullptr
        );

        if (m_dirHandle == INVALID_HANDLE_VALUE)
        {
            HYP_LOG(Assets, Warning, "DirectoryWatcher: failed to open '{}'", dirPath);
            m_running = false;
            return;
        }

        m_stopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        m_thread = std::thread(&DirectoryWatcher::Run, this);
    }

    ~DirectoryWatcher()
    {
        m_running = false;

        if (m_stopEvent)
        {
            SetEvent(m_stopEvent);
        }

        if (m_thread.joinable())
        {
            m_thread.join();
        }

        if (m_dirHandle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(m_dirHandle);
        }

        if (m_stopEvent)
        {
            CloseHandle(m_stopEvent);
        }
    }

private:
    void Run()
    {
        alignas(DWORD) BYTE buffer[8192];

        while (m_running.load())
        {
            OVERLAPPED overlapped = {};
            overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

            DWORD bytesReturned = 0;

            BOOL ok = ReadDirectoryChangesW(
                m_dirHandle,
                buffer,
                sizeof(buffer),
                TRUE,
                FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_CREATION,
                &bytesReturned,
                &overlapped,
                nullptr
            );

            if (!ok)
            {
                CloseHandle(overlapped.hEvent);

                break;
            }

            HANDLE waits[] = { overlapped.hEvent, m_stopEvent };
            DWORD result = WaitForMultipleObjects(2, waits, FALSE, INFINITE);

            if (result != WAIT_OBJECT_0)
            {
                CancelIo(m_dirHandle);
                CloseHandle(overlapped.hEvent);

                break;
            }

            if (!GetOverlappedResult(m_dirHandle, &overlapped, &bytesReturned, FALSE))
            {
                CloseHandle(overlapped.hEvent);

                continue;
            }

            CloseHandle(overlapped.hEvent);

            if (bytesReturned == 0)
            {
                continue;
            }

            DWORD offset = 0;

            while (offset < bytesReturned)
            {
                FILE_NOTIFY_INFORMATION* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer + offset);

                int wideLen = int(info->FileNameLength / sizeof(wchar_t));
                char utf8Buf[MAX_PATH];
                int utf8Len = WideCharToMultiByte(CP_UTF8, 0, info->FileName, wideLen, utf8Buf, sizeof(utf8Buf), nullptr, nullptr);

                if (utf8Len > 0)
                {
                    String relativePath { utf8Buf, utf8Buf + utf8Len };

                    bool wasDeleted = info->Action == FILE_ACTION_REMOVED || info->Action == FILE_ACTION_RENAMED_OLD_NAME;
                    m_callback(relativePath, wasDeleted);
                }

                if (info->NextEntryOffset == 0)
                {
                    break;
                }

                offset += info->NextEntryOffset;
            }
        }
    }

    HANDLE m_dirHandle = INVALID_HANDLE_VALUE;
    HANDLE m_stopEvent = nullptr;
    std::thread m_thread;
    std::atomic<bool> m_running;
    Callback m_callback;
};

#elif defined(HYP_UNIX) || defined(HYP_ANDROID)

class DirectoryWatcher
{
public:
    using Callback = ProcRef<void(const String& relativePath, bool)>;

    DirectoryWatcher(const FilePath& dirPath, const Callback& callback)
        : m_callback(callback),
          m_running(true),
          m_inotifyFd(-1),
          m_watchFd(-1)
    {
        m_inotifyFd = inotify_init1(IN_NONBLOCK);
        if (m_inotifyFd < 0)
            return;

        m_watchFd = inotify_add_watch(m_inotifyFd, dirPath.Data(),
            IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVE);

        if (m_watchFd < 0)
        {
            close(m_inotifyFd);
            m_inotifyFd = -1;
            return;
        }

        m_stopFd[0] = -1;
        m_stopFd[1] = -1;
        pipe(m_stopFd);

        m_thread = std::thread(&DirectoryWatcher::Run, this);
    }

    ~DirectoryWatcher()
    {
        m_running = false;

        if (m_stopFd[1] >= 0)
        {
            char c = 'x';
            write(m_stopFd[1], &c, 1);
        }

        if (m_thread.joinable())
            m_thread.join();

        if (m_inotifyFd >= 0)
        {
            inotify_rm_watch(m_inotifyFd, m_watchFd);
            close(m_inotifyFd);
        }

        for (int fd : m_stopFd)
        {
            if (fd >= 0)
            {
                close(fd);
            }
        }
    }

private:
    void Run()
    {
        char buffer[8192];

        while (m_running.load())
        {
            struct pollfd fds[2];
            fds[0].fd = m_inotifyFd;
            fds[0].events = POLLIN;
            fds[1].fd = m_stopFd[0];
            fds[1].events = POLLIN;

            int ret = poll(fds, 2, 2000);

            if (ret <= 0)
                continue;

            if (fds[1].revents & POLLIN)
                break;

            if (!(fds[0].revents & POLLIN))
                continue;

            int len = read(m_inotifyFd, buffer, sizeof(buffer));
            if (len <= 0)
                continue;

            int offset = 0;
            while (offset < len)
            {
                struct inotify_event** event = reinterpret_cast<struct inotify_event*>(buffer + offset);

                if (event->len > 0)
                {
                    String relativePath { event->name };

                    bool wasDeleted = (event->mask & IN_DELETE) || (event->mask & IN_MOVE);
                    m_callback(relativePath, wasDeleted);
                }

                offset += sizeof(struct inotify_event) + event->len;
            }
        }
    }

    int m_inotifyFd;
    int m_watchFd;
    int m_stopFd[2];
    std::thread m_thread;
    std::atomic<bool> m_running;
    Callback m_callback;
};

#endif

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
                        {
                            continue;
                        }

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

    static bool ParseAssetFilePath(const String& relativePath, String& outBucketName, String& outAssetName)
    {
        size_t sep = relativePath.FindFirstIndex("/");
        if (sep == String::NotFound)
            sep = relativePath.FindFirstIndex("\\");
        if (sep == String::NotFound)
            return false;

        outBucketName = relativePath.Substr(0, sep);
        String fileName = relativePath.Substr(sep + 1);

        if (fileName.EndsWith(".hmf"))
        {
            outAssetName = fileName.Substr(0, fileName.Size() - 4);
            return true;
        }

        size_t dot = fileName.FindFirstIndex(".");
        if (dot != String::NotFound)
        {
            outAssetName = fileName.Substr(0, dot);
            return true;
        }

        return false;
    }

    static void UpdateAssetInManifest(
        ServerState& state,
        AssetRegistry* registry,
        AssetRegistryId registryId,
        const String& bucketName,
        const String& assetNameStr,
        bool wasDeleted)
    {
        Mutex::Guard guard(state.mutex);

        AssetBucket bucket = GetAssetBucketByName(StringHash(bucketName));
        uint32 bucketIndex = bucket.GetIndex();

        if (bucketIndex == 0 || bucketIndex >= MaxAssetBuckets)
        {
            return;
        }

        Name name = CreateNameFromDynamicString(assetNameStr);

        size_t existingIndex = size_t(-1);
        for (size_t i = 0; i < state.manifest.assets.Size(); i++)
        {
            const AssetEntry& entry = state.manifest.assets[i];

            if (entry.registryId == registryId
                && entry.bucketIndex == bucketIndex
                && entry.name == name)
            {
                existingIndex = i;

                break;
            }
        }

        if (wasDeleted)
        {
            if (existingIndex != size_t(-1))
            {
                for (const BlobEntry& blob : state.manifest.assets[existingIndex].blobs)
                    state.blobLookup.Erase(blob.key);

                state.manifest.assets.EraseAt(existingIndex);

                HYP_LOG(Assets, Info, "CacheServer: removed asset {}/{}", bucketName, assetNameStr);
            }

            state.manifest.cookTimestamp = uint64(Time::Now());

            return;
        }

        if (!registry)
        {
            return;
        }

        Handle<AssetObject> assetObject = registry->GetAsset(bucket, name);
        if (!assetObject.IsValid() || assetObject->IsTransient())
        {
            return;
        }

        Array<Tuple<const char*, uint16, BlobDataReference*>> blobRefs;
        assetObject->CollectBlobDataReferences(blobRefs);

        if (existingIndex != size_t(-1))
        {
            for (const BlobEntry& blob : state.manifest.assets[existingIndex].blobs)
                state.blobLookup.Erase(blob.key);
        }

        AssetEntry newEntry;
        newEntry.registryId = registryId;
        newEntry.bucketIndex = bucketIndex;
        newEntry.name = name;

        FilePath hmfPath = registry->GetManifestPath(assetObject->GetPath());
        if (hmfPath.Exists())
        {
            newEntry.lastModifiedTimestamp = uint64(hmfPath.LastModifiedTimestamp());
        }

        for (auto& tup : blobRefs)
        {
            BlobDataReference* ref = tup.GetElement<2>();

            if (!ref->key || ref->size == 0)
            {
                continue;
            }

            uint64 key = ref->key.GetHashCode().Value();

            BlobEntry blobEntry;
            blobEntry.key = key;
            blobEntry.size = ref->size;
            blobEntry.magic = tup.GetElement<0>();
            newEntry.blobs.PushBack(std::move(blobEntry));

            BlobLookupEntry& lookup = state.blobLookup[key];
            lookup = {};
            lookup.bucketIndex = bucketIndex;
            lookup.assetName = name;
            lookup.size = ref->size;
            lookup.magic = tup.GetElement<0>();
        }

        if (existingIndex != size_t(-1))
        {
            // Replace in-place — sorted position may have shifted, so
            // remove and re-insert at the correct position.
            state.manifest.assets.EraseAt(existingIndex);
        }

        // Insert at the sorted position (descending by timestamp)
        const AssetEntry* insertPos = std::lower_bound(
            state.manifest.assets.Data(),
            state.manifest.assets.Data() + state.manifest.assets.Size(),
            newEntry.lastModifiedTimestamp,
            [](const AssetEntry& entry, uint64 ts)
            {
                return entry.lastModifiedTimestamp > ts;
            });

        state.manifest.assets.Insert(insertPos, std::move(newEntry));
        state.manifest.cookTimestamp = uint64(Time::Now());

        HYP_LOG(Assets, Info, "CacheServer: updated asset {}/{}", bucketName, assetNameStr);
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

        const FilePath gameContentDir = gameRegistry->GetRootPath();
        const FilePath& engineContentDir = EngineGlobals::GetContentDirectory<HYP_STATIC_STRING("Engine")>();

        auto makeCallback = [&state](AssetRegistry* registry)
        {
            return [&state, registry](const String& relativePath, bool wasDeleted)
            {
                String bucketName, assetName;
                if (!ParseAssetFilePath(relativePath, bucketName, assetName))
                {
                    return;
                }

                UpdateAssetInManifest(state, registry, AssetRegistryId::Game,
                    bucketName, assetName, wasDeleted);
            };
        };

        auto gameCallback = makeCallback(gameRegistry);
        DirectoryWatcher gameWatcher(gameContentDir, gameCallback);

        auto engineCallback = makeCallback(engineRegistry);
        DirectoryWatcher engineWatcher(engineContentDir, engineCallback);

        HYP_LOG(Assets, Info, "CacheServer file watchers started");

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