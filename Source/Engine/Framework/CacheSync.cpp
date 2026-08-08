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
#include <Asset/BlobStorage.hpp>
#include <Asset/BlobStorageStructs.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Core/Threading/ThreadPool.hpp>
#include <Core/Threading/Task.hpp>

#include <Core/IO/ByteWriter.hpp>
#include <Core/IO/ByteReader.hpp>

#include <Core/DataProcessing/HMF/HMF.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

#include <Core/Utilities/ByteUtil.hpp>
#include <Core/Math/MathUtil.hpp>

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

#elif defined(HYP_WINDOWS)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#endif

namespace Hyperion {

namespace CacheSync {

namespace {

static Result HttpGetBytes(const ANSIString& host, uint16 port, const char* path, ByteBuffer& outBody)
{
    outBody = ByteBuffer();

    HYP_LOG(Assets, Info, "[CacheClient] GET {}:{}{}", host, uint32(port), path);

    char portStr[16];
    std::snprintf(portStr, sizeof(portStr), "%u", uint32(port));

#if defined(HYP_WINDOWS)

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        return HYP_MAKE_ERROR(Error, "WSAStartup failed");

    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo* result = nullptr;
    if (getaddrinfo(host.Data(), portStr, &hints, &result) != 0)
    {
        WSACleanup();
        return HYP_MAKE_ERROR(Error, "Failed to resolve host: {}", host);
    }

    SOCKET sock = INVALID_SOCKET;
    for (struct addrinfo* rp = result; rp != nullptr; rp = rp->ai_next)
    {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock == INVALID_SOCKET)
            continue;
        if (connect(sock, rp->ai_addr, int(rp->ai_addrlen)) == 0)
            break;
        closesocket(sock);
        sock = INVALID_SOCKET;
    }

    freeaddrinfo(result);

    if (sock == INVALID_SOCKET)
    {
        WSACleanup();
        return HYP_MAKE_ERROR(Error, "Failed to connect to {}:{}", host, uint32(port));
    }

    DWORD timeout = 10000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    char request[4096];
    int reqLen = std::snprintf(request, sizeof(request),
        "GET %s HTTP/1.0\r\n"
        "Host: %s:%u\r\n"
        "Connection: close\r\n"
        "\r\n",
         path, host.Data(), uint32(port));

    if (send(sock, request, reqLen, 0) == SOCKET_ERROR)
    {
        closesocket(sock);
        WSACleanup();
        return HYP_MAKE_ERROR(Error, "Failed to send request to {}:{}", host, uint32(port));
    }

    char recvBuf[8192];
    int n;
    ByteBuffer buffer;
    buffer.SetSize(0);

    while ((n = recv(sock, recvBuf, sizeof(recvBuf), 0)) > 0)
    {
        size_t oldSize = buffer.Size();
        buffer.SetSize(oldSize + size_t(n));
        Memory::Copy(buffer.Data() + oldSize, recvBuf, size_t(n));
    }

    closesocket(sock);
    WSACleanup();

    if (n < 0)
        return HYP_MAKE_ERROR(Error, "Failed to receive response from {}:{}", host, uint32(port));

#else

    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* result = nullptr;
    if (getaddrinfo(host.Data(), portStr, &hints, &result) != 0)
        return HYP_MAKE_ERROR(Error, "Failed to resolve host: {}", host);

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
        return HYP_MAKE_ERROR(Error, "Failed to connect to {}:{}", host, uint32(port));

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
        close(sock);
        return HYP_MAKE_ERROR(Error, "Failed to send request to {}:{}", host, uint32(port));
    }

    char recvBuf[8192];
    ssize_t n;
    ByteBuffer buffer;
    buffer.SetSize(0);

    while ((n = recv(sock, recvBuf, sizeof(recvBuf), 0)) > 0)
    {
        size_t oldSize = buffer.Size();
        buffer.SetSize(oldSize + size_t(n));
        Memory::Copy(buffer.Data() + oldSize, recvBuf, size_t(n));
    }

    close(sock);

    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        return HYP_MAKE_ERROR(Error, "Failed to receive response from {}:{}", host, uint32(port));

#endif

    const char* bodyStart = reinterpret_cast<const char*>(buffer.Data());
    const char* headerEnd = strstr(bodyStart, "\r\n\r\n");

    if (headerEnd == nullptr)
        return HYP_MAKE_ERROR(Error, "Invalid HTTP response from {}:{}{} — no headers", host, uint32(port), path);

    // Check HTTP status code, should only ever be 200 if succeeded.
    {
        const char* statusStart = strchr(bodyStart, ' ');
        if (statusStart != nullptr && statusStart < headerEnd)
        {
            int statusCode = atoi(statusStart + 1);

            if (statusCode != 200)
            {
                return HYP_MAKE_ERROR(Error, "Server returned HTTP {} for {}:{}{}", statusCode, host, uint32(port), path);
            }
        }
    }

    bodyStart = headerEnd + 4;
    const size_t bodySize = size_t(reinterpret_cast<const char*>(buffer.Data() + buffer.Size()) - bodyStart);

    if (bodySize == 0)
        return HYP_MAKE_ERROR(Error, "Empty response body from {}:{}{}", host, uint32(port), path);

    outBody = ByteBuffer(bodySize, bodyStart);

    return {};
}

Result DownloadCacheFromHost(
    const ANSIStringView& host, uint16 port,
    const FilePath& cacheDir, const FilePath& contentDir)
{
    // 1. Download the manifest
    ByteBuffer manifestBytes;
    if (Result res = HttpGetBytes(host, port, "/manifest", manifestBytes); res.HasError())
    {
        return res;
    }

    // 2. Parse the server manifest via HMF
    String manifestStr = String(manifestBytes.ToByteView());

    HMF::ParseResult parseResult = HMF::Parse(manifestStr);
    if (!parseResult.HasValue())
    {
        return HYP_MAKE_ERROR(Error,"CacheSync failed to parse server manifest");
    }

    BoxedValue& boxedResult = parseResult.GetValue();
    if (!boxedResult.Is<CookManifest>())
    {
        return HYP_MAKE_ERROR(Error, "CacheSync server manifest is not a CookManifest");
    }

    const CookManifest& serverManifest = boxedResult.Get<CookManifest>();

    // @TODO we will save timestamp into the manifest file;
    // we'll still have a general check, so we can short circuit out of here in most cases,
    // but we'll want to only download assets that are missing/have newer versions.
    // so on the server side, we should sort the assets in the manifest descending by timestmap.
    // then we can stop downloading on the first one that has a timestamp indicating we have the latest ver.
    HYP_LOG(Assets, Info, "CacheSync server manifest: timestamp={}, {} assets",
        serverManifest.cook_timestamp_ms, serverManifest.assets.Size());

    // Compare with local manifest
    uint64 localTimestamp = 0;
    FilePath localManifestPath = cacheDir / "Manifest.hmf";

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
        HYP_LOG(Assets, Info, "CacheSync cache up to date (local={} >= server={})",
            localTimestamp, serverManifest.cook_timestamp_ms);
        return {};
    }

    HYP_LOG(Assets, Info, "CacheSync cache outdated (local={} < server={}), downloading",
        localTimestamp, serverManifest.cook_timestamp_ms);

    if (!cacheDir.Exists() && !cacheDir.MkDir())
    {
        return HYP_MAKE_ERROR(Error,"CacheSync failed to create cache directory '{}'", cacheDir);
    }

    // 5. Pre-compute block sizes from the manifest
    Array<uint64> blockSizes;
    blockSizes.Resize(MaxAssetBuckets);

    for (const AssetEntry& entry : serverManifest.assets)
    {
        if (entry.bucket_index >= MaxAssetBuckets)
            continue;

        for (const BlobEntry& blob : entry.blobs)
        {
            const size_t alignedSize = ByteUtil::AlignAs(blockSizes[entry.bucket_index], alignof(BlobHeader))
                + sizeof(BlobHeader) + blob.size;
            blockSizes[entry.bucket_index] = alignedSize;
        }
    }

    // 6. Build BlobBlockInfo array
    Array<BlobBlockInfo> blocks;
    for (uint32 bucketIndex = 1; bucketIndex < MaxAssetBuckets; bucketIndex++)
    {
        if (blockSizes[bucketIndex] > 0)
            blocks.PushBack(BlobBlockInfo { bucketIndex, blockSizes[bucketIndex] });
    }

    BlobStorage writeStorage(cacheDir, /* readOnly */ false);
    writeStorage.Initialize();

    if (Result result = writeStorage.BeginCook(blocks); result.HasError())
    {
        return HYP_MAKE_ERROR(Error,"CacheSync BeginCook failed: {}", result.GetError().GetMessage());
    }

    // 8. Download assets in parallel
    using threading::TaskThreadPool;
    TaskThreadPool downloadPool("CacheSyncWorker", 4);
    downloadPool.Start();

    Array<Task<void>> tasks;

    AtomicVar<int32> failureCount = 0;

    for (const AssetEntry& entry : serverManifest.assets)
    {
        if (entry.bucket_index >= MaxAssetBuckets)
        {
            continue;
        }

        tasks.EmplaceBack(downloadPool.Enqueue(HYP_STATIC_MESSAGE("CacheSyncAsset"), [&]() -> void
        {
            bool entryFailed = false;

            // Download the HMF manifest
            {
                char hmfPathBuf[512];
                std::snprintf(hmfPathBuf, sizeof(hmfPathBuf), "/hmf/%u/%s",
                    entry.bucket_index, entry.name.Data());

                ByteBuffer hmfBytes;
                if (Result res = HttpGetBytes(host, port, hmfPathBuf, hmfBytes); res.HasError())
                {
                    HYP_LOG(Assets, Warning, "Failed to download HMF for {}: {}", entry.name, res.GetError().GetMessage());
                }
                else
                {
                    FilePath bucketContentDir = contentDir / String(GetAssetBucketName(entry.bucket_index));
                    bucketContentDir.MkDir();

                    FilePath manifestPath = bucketContentDir / (entry.name + ".hmf");

                    FileByteWriter writer { manifestPath };
                    if (writer.IsOpen())
                    {
                        writer.Write(hmfBytes);
                        writer.Close();
                    }
                }
            }

            // Download each blob and write via PutData
            for (const BlobEntry& blob : entry.blobs)
            {
                char blobPathBuf[512];
                std::snprintf(blobPathBuf, sizeof(blobPathBuf), "/blob?key=%llx&size=%llu",
                    (unsigned long long)blob.key, (unsigned long long)blob.size);

                ByteBuffer blobData;
                if (Result res = HttpGetBytes(host, port, blobPathBuf, blobData); res.HasError())
                {
                    HYP_LOG(Assets, Error, "CacheSync failed to download blob key={} for {}: {}",
                            blob.key, entry.name, res.GetError().GetMessage());

                    entryFailed = true;
                    continue;
                }

                if (blobData.Size() != blob.size)
                {
                    HYP_LOG(Assets, Warning, "Blob size mismatch! ({} != {}) entry: {}, blob key: {}",
                            blobData.Size(), blob.size,
                            entry.name, blob.key);

                    entryFailed = true;
                    continue;
                }

                BlobHeader header {};

                const size_t magicLen = blob.magic.Size();
                Memory::Copy((char*)header.magic, blob.magic.Data(), MathUtil::Min(magicLen, sizeof(header.magic)));
                
                header.version = 1;
                header.payloadOffset = 0;
                header.payloadSize = blob.size;

                if (!writeStorage.PutData(entry.bucket_index, StringHash(blob.key), header, blobData.Data()))
                {
                    HYP_LOG(Assets, Error, "CacheSync PutData failed for blob key={}", blob.key);
                    entryFailed = true;
                }
            }

            if (entryFailed)
            {
                failureCount.Increment(1, MemoryOrder::RELAXED);
            }
        }));
    }

    // Wait for download task to finish
    for (auto& task : tasks)
    {
        task.Await();
    }

    downloadPool.Stop();

    if (Result result = writeStorage.FinishCook(); result.HasError())
    {
        return HYP_MAKE_ERROR(Error,"CacheSync FinishCook failed: {}", result.GetError().GetMessage());
    }

    writeStorage.Shutdown();

    if (failureCount.Get(MemoryOrder::RELAXED) > 0)
    {
        return HYP_MAKE_ERROR(Error, "Download incomplete — {} asset(s) had failures", failureCount.Get(MemoryOrder::RELAXED));
    }

    localManifestPath.Remove();

    FileByteWriter manifestWriter { localManifestPath };
    if (!manifestWriter.IsOpen())
    {
        return HYP_MAKE_ERROR(Error,"CacheSync failed to write local manifest");
    }

    manifestWriter.WriteString(manifestStr.ToUtf8());
    manifestWriter.Close();

    HYP_LOG(Assets, Info, "CacheSync complete — {} assets, {} blocks",
        serverManifest.assets.Size(), blocks.Size());

    return {};
}

} // anonymous

HYP_EXPORT void SyncCacheBlocking(const FilePath& cacheDir, const FilePath& contentDir)
{
    static constexpr int MaxAttempts = 5;
    int numAttempts = 0;

    // If cache server is set, download cache from there to build out ours.
    if (const ANSIStringView cacheServer = ANSIStringView(EngineGlobals::GetCacheServerAddress()); cacheServer)
    {
        size_t colonPos = cacheServer.FindLastIndex(":");
        if (colonPos == String::NotFound)
        {
            HYP_LOG(Assets, Error, "Invalid cache server address: {}", cacheServer);
            return;
        }

        ANSIStringView host = cacheServer.Substr(0, colonPos);

        // Chomp off the protocol.
        if (host.Size() > 7 && Memory::Compare(host.Data(), "http://", 7) == 0)
        {
            host = ANSIStringView(host.Data() + 7, host.Data() + host.Size());
        }
        else if (host.Size() > 8 && Memory::Compare(host.Data(), "https://", 8) == 0)
        {
            host = ANSIStringView(host.Data() + 8, host.Data() + host.Size());
        }
        ANSIStringView portStr = cacheServer.Substr(colonPos + 1, SIZE_MAX);

        uint16 port = static_cast<uint16>(std::atoi(portStr.Data()));

        while (true)
        {
            Result res;
            if ((res = DownloadCacheFromHost(host, port, cacheDir, contentDir)); !res.HasError())
            {
                break;
            }

            ++numAttempts;

            if (numAttempts < MaxAttempts)
            {
                HYP_LOG(Assets, Error, "Failed to download cache from server! Error message was: {}\nRetrying in 5s...", res.GetError().GetMessage());

                ThreadSleep(5000);
            }
            else
            {
                HYP_LOG(Assets, Error, "Failed to download cache from server! Error message was: {}\nDone trying.", res.GetError().GetMessage());
                return;
            }
        }
    }
}

} // namespace CacheSync

} // namespace Hyperion