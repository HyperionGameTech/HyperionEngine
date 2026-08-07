/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Framework/CacheSync.hpp>
#include <Framework/EngineGlobals.hpp>

#include <Asset/CookManifest.hpp>
#include <Asset/AssetBucket.hpp>

#include <Core/Threading/ThreadPool.hpp>
#include <Core/Threading/Task.hpp>

#include <Core/IO/ByteWriter.hpp>
#include <Core/IO/ByteReader.hpp>

#include <Core/DataProcessing/HMF/HMF.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

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

#endif // HYP_UNIX || HYP_ANDROID

namespace Hyperion {

#if defined(HYP_UNIX) || defined(HYP_ANDROID)

static bool HttpGet(const char* host, uint16 port, const char* path, ByteBuffer& outBody)
{
    outBody = ByteBuffer();

    char portStr[16];
    std::snprintf(portStr, sizeof(portStr), "%u", uint32(port));

    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* result = nullptr;
    if (getaddrinfo(host, portStr, &hints, &result) != 0)
    {
        HYP_LOG(Assets, Error, "CacheSync getaddrinfo failed for {}:{}", host, portStr);
        return false;
    }

    int sock = -1;

    for (struct addrinfo* rp = result; rp != nullptr; rp = rp->ai_next)
    {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock < 0)
            continue;

        if (connect(sock, rp->ai_addr, int(rp->ai_addrlen)) == 0)
            break;

        close(sock);
        sock = -1;
    }

    freeaddrinfo(result);

    if (sock < 0)
    {
        HYP_LOG(Assets, Error, "CacheSync failed to connect to {}:{}", host, portStr);
        return false;
    }

    struct timeval tv = { 10, 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    char request[4096];
    int reqLen = std::snprintf(request, sizeof(request),
        "GET %s HTTP/1.0\r\n"
        "Host: %s:%u\r\n"
        "Connection: close\r\n"
        "\r\n",
        path, host, uint32(port));

    if (send(sock, request, size_t(reqLen), 0) < 0)
    {
        HYP_LOG(Assets, Error, "CacheSync send failed for {}:{}{}", host, portStr, path);
        close(sock);
        return false;
    }

    ByteBuffer buffer;
    buffer.SetSize(0);

    char recvBuf[8192];
    ssize_t n;
    while ((n = recv(sock, recvBuf, sizeof(recvBuf), 0)) > 0)
    {
        size_t oldSize = buffer.Size();
        buffer.SetSize(oldSize + size_t(n));
        Memory::Copy(buffer.Data() + oldSize, recvBuf, size_t(n));
    }

    close(sock);

    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
    {
        HYP_LOG(Assets, Error, "CacheSync recv failed for {}:{}{}", host, portStr, path);
        return false;
    }

    const char* bodyStart = reinterpret_cast<const char*>(buffer.Data());
    const char* headerEnd = strstr(bodyStart, "\r\n\r\n");

    if (headerEnd == nullptr)
    {
        HYP_LOG(Assets, Error, "CacheSync invalid HTTP response for {}:{}{}", host, portStr, path);
        return false;
    }

    bodyStart = headerEnd + 4;
    const size_t bodySize = size_t(reinterpret_cast<const char*>(buffer.Data() + buffer.Size()) - bodyStart);

    outBody = ByteBuffer(bodySize, bodyStart);

    return true;
}

#else

static bool HttpGet(const char*, uint16, const char*, ByteBuffer&)
{
    return false;
}

#endif // HYP_UNIX || HYP_ANDROID

bool CacheSync_Download(const char* host, uint16 port, const char* cacheDir)
{
    // 1. Download the manifest
    ByteBuffer manifestBytes;
    if (!HttpGet(host, port, "/manifest", manifestBytes))
    {
        HYP_LOG(Assets, Error, "CacheSync failed to download manifest from {}:{}", host, uint32(port));
        return false;
    }

    // 2. Parse the manifest via HMF
    String manifestStr = String(manifestBytes.ToByteView());

    HMF::ParseResult parseResult = HMF::Parse(manifestStr);
    if (!parseResult.HasValue())
    {
        HYP_LOG(Assets, Error, "CacheSync failed to parse server manifest");
        return false;
    }

    BoxedValue& boxedResult = parseResult.GetValue();

    if (!boxedResult.Is<CookManifest>())
    {
        HYP_LOG(Assets, Error, "CacheSync server manifest is not a CookManifest");
        return false;
    }

    const CookManifest& serverManifest = boxedResult.Get<CookManifest>();

    HYP_LOG(Assets, Info, "CacheSync server manifest: timestamp={}, {} files",
        serverManifest.cook_timestamp_ms, serverManifest.files.Size());

    // 3. Compare with the locally cached manifest timestamp
    uint64 localTimestamp = 0;
    FilePath localManifestPath = FilePath(cacheDir) / "cook_manifest.hmf";

    if (localManifestPath.Exists())
    {
        FileByteReader localReader { localManifestPath };
        if (!localReader.Eof())
        {
            String localStr = String(localReader.Read().ToByteView());
            HMF::ParseResult localResult = HMF::Parse(localStr);

            if (localResult.HasValue())
            {
                BoxedValue& localBoxed = localResult.GetValue();
                if (localBoxed.Is<CookManifest>())
                {
                    localTimestamp = localBoxed.Get<CookManifest>().cook_timestamp_ms;
                }
            }
        }
    }

    if (serverManifest.cook_timestamp_ms <= localTimestamp)
    {
        HYP_LOG(Assets, Info, "CacheSync cache is up to date (local={} >= server={})",
            localTimestamp, serverManifest.cook_timestamp_ms);
        return true;
    }

    HYP_LOG(Assets, Info, "CacheSync cache outdated (local={} < server={}), downloading {} files",
        localTimestamp, serverManifest.cook_timestamp_ms, serverManifest.files.Size());

    // 4. Download all files in parallel
    FilePath cacheDirPath { cacheDir };

    if (!cacheDirPath.Exists())
    {
        if (!cacheDirPath.MkDir())
        {
            HYP_LOG(Assets, Error, "CacheSync failed to create cache directory '{}'", cacheDir);
            return false;
        }
    }

    using threading::TaskThreadPool;
    TaskThreadPool downloadPool("CacheSyncWorker", 4);
    downloadPool.Start();

    Array<Task<void>> tasks;

    for (const String& file : serverManifest.files)
    {
        tasks.EmplaceBack(downloadPool.Enqueue(HYP_STATIC_MESSAGE("CacheSyncDownload"), [&]() -> void
        {
            ByteBuffer body;
            String reqPath = String("/file/") + file;

            if (!HttpGet(host, port, reqPath.Data(), body))
            {
                HYP_LOG(Assets, Error, "CacheSync failed to download '{}' from {}:{}",
                    file, host, uint32(port));
                return;
            }

            FilePath filePath = cacheDirPath / file;

            FileByteWriter fileWriter { filePath };
            if (!fileWriter.IsOpen())
            {
                HYP_LOG(Assets, Error, "CacheSync failed to open '{}' for writing", filePath.Data());
                return;
            }

            fileWriter.Write(body);
            fileWriter.Close();

            HYP_LOG(Assets, Info, "CacheSync downloaded '{}' ({} bytes)", file, body.Size());
        }));
    }

    // 5. Wait for all downloads to complete
    for (auto& task : tasks)
    {
        task.Await();
    }

    downloadPool.Stop();

    // 6. Write the server manifest locally
    localManifestPath.Remove();

    FileByteWriter manifestWriter { localManifestPath };
    if (!manifestWriter.IsOpen())
    {
        HYP_LOG(Assets, Error, "CacheSync failed to write local manifest");
        return false;
    }

    manifestWriter.Write(manifestBytes);
    manifestWriter.Close();

    HYP_LOG(Assets, Info, "CacheSync complete");

    return true;
}

} // namespace Hyperion