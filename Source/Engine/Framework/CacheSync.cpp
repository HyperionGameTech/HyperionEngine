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

Result HttpGetBytes(
    const ANSIString& host, uint16 port,
    const char* path,
    ByteWriter& writer,
    bool* outShouldRetry = nullptr)
{
    if (outShouldRetry)
    {
        *outShouldRetry = false;
    }

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

        if (outShouldRetry)
        {
            *outShouldRetry = true;
        }

        return HYP_MAKE_ERROR(Error, "Failed to resolve host: {}", host);
    }

    SOCKET sock = INVALID_SOCKET;
    for (struct addrinfo* rp = result; rp != nullptr; rp = rp->ai_next)
    {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

        if (sock == INVALID_SOCKET)
        {
            continue;
        }

        if (connect(sock, rp->ai_addr, int(rp->ai_addrlen)) == 0)
        {
            break;
        }

        closesocket(sock);
        sock = INVALID_SOCKET;
    }

    freeaddrinfo(result);

    if (sock == INVALID_SOCKET)
    {
        WSACleanup();

        if (outShouldRetry)
        {
            *outShouldRetry = true;
        }

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
    char headerBuf[8192];
    size_t headerLen = 0;
    bool headersDone = false;
    size_t expectedBodySize = 0;
    size_t receivedBodySize = 0;

    char recvBuf[8192];
    int n;

    while ((n = recv(sock, recvBuf, sizeof(recvBuf), 0)) > 0)
    {
        if (!headersDone)
        {
            if (headerLen + size_t(n) > sizeof(headerBuf))
                return HYP_MAKE_ERROR(Error, "Response headers too large from {}:{}", host, uint32(port));

            Memory::Copy(headerBuf + headerLen, recvBuf, size_t(n));
            headerLen += size_t(n);

            const char* bodyEnd = strstr(headerBuf, "\r\n\r\n");
            if (bodyEnd != nullptr)
            {
                const char* statusStart = strchr(headerBuf, ' ');
                if (statusStart == nullptr)
                    return HYP_MAKE_ERROR(Error, "Invalid HTTP response from {}:{}", host, uint32(port));

                int statusCode = atoi(statusStart + 1);
                if (statusCode != 200)
                    return HYP_MAKE_ERROR(Error, "Server returned HTTP {} for {}:{}{}", statusCode, host, uint32(port), path);

                // Parse Content-Length
                const char* cl = strstr(headerBuf, "Content-Length:");
                if (cl != nullptr && cl < bodyEnd)
                    expectedBodySize = size_t(atoi(cl + 15));

                const char* bodyStart = bodyEnd + 4;
                size_t bodyBytesInHeader = size_t(headerBuf + headerLen - bodyStart);
                if (bodyBytesInHeader > 0)
                {
                    writer.Write(bodyStart, bodyBytesInHeader);
                    receivedBodySize += bodyBytesInHeader;
                }

                headersDone = true;
            }
        }
        else
        {
            writer.Write(recvBuf, size_t(n));
            receivedBodySize += size_t(n);
        }
    }

    closesocket(sock);
    WSACleanup();

    if (n < 0)
        return HYP_MAKE_ERROR(Error, "Failed to receive response from {}:{}", host, uint32(port));

    if (!headersDone)
        return HYP_MAKE_ERROR(Error, "Response from {}:{}{} ended before headers", host, uint32(port), path);

    if (expectedBodySize > 0 && receivedBodySize < expectedBodySize)
        return HYP_MAKE_ERROR(Error, "Truncated body from {}:{}{} — expected {} bytes, got {}",
            host, uint32(port), path, expectedBodySize, receivedBodySize);

    writer.Flush();

    return {};

#else

    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* result = nullptr;
    if (getaddrinfo(host.Data(), portStr, &hints, &result) != 0)
    {
        if (outShouldRetry)
        {
            *outShouldRetry = true;
        }

        return HYP_MAKE_ERROR(Error, "Failed to resolve host: {}", host);
    }

    int sock = -1;
    for (struct addrinfo* rp = result; rp != nullptr; rp = rp->ai_next)
    {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

        if (sock < 0)
        {
            continue;
        }

        if (connect(sock, rp->ai_addr, int(rp->ai_addrlen)) == 0)
        {
            break;
        }

        close(sock);
        sock = -1;
    }

    freeaddrinfo(result);

    if (sock < 0)
    {
        if (outShouldRetry)
        {
            *outShouldRetry = true;
        }

        return HYP_MAKE_ERROR(Error, "Failed to connect to {}:{}", host, uint32(port));
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
        close(sock);

        return HYP_MAKE_ERROR(Error, "Failed to send request to {}:{}", host, uint32(port));
    }

    char headerBuf[8192];
    size_t headerLen = 0;
    bool headersDone = false;
    size_t expectedBodySize = 0;
    size_t receivedBodySize = 0;

    char recvBuf[8192];
    ssize_t n;

    while ((n = recv(sock, recvBuf, sizeof(recvBuf), 0)) > 0)
    {
        if (!headersDone)
        {
            if (headerLen + size_t(n) > sizeof(headerBuf))
            {
                return HYP_MAKE_ERROR(Error, "Response headers too large from {}:{}", host, uint32(port));
            }

            Memory::Copy(headerBuf + headerLen, recvBuf, size_t(n));
            headerLen += size_t(n);

            const char* bodyEnd = strstr(headerBuf, "\r\n\r\n");

            if (bodyEnd != nullptr)
            {
                const char* statusStart = strchr(headerBuf, ' ');
                if (!statusStart)
                {
                    return HYP_MAKE_ERROR(Error, "Invalid HTTP response from {}:{}", host, uint32(port));
                }

                int statusCode = atoi(statusStart + 1);
                if (statusCode != 200)
                {
                    return HYP_MAKE_ERROR(Error, "Server returned HTTP {} for {}:{}{}", statusCode, host, uint32(port), path);
                }

                // Parse Content-Length
                const char* cl = strstr(headerBuf, "Content-Length:");
                if (cl != nullptr && cl < bodyEnd)
                    expectedBodySize = size_t(atoi(cl + 15));

                const char* bodyStart = bodyEnd + 4;

                size_t bodyBytesInHeader = size_t(headerBuf + headerLen - bodyStart);
                if (bodyBytesInHeader > 0)
                {
                    writer.Write(bodyStart, bodyBytesInHeader);
                    receivedBodySize += bodyBytesInHeader;
                }

                headersDone = true;
            }
        }
        else
        {
            writer.Write(recvBuf, size_t(n));
            receivedBodySize += size_t(n);
        }
    }

    close(sock);

    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
    {
        return HYP_MAKE_ERROR(Error, "Failed to receive response from {}:{}", host, uint32(port));
    }

    if (!headersDone)
    {
        return HYP_MAKE_ERROR(Error, "Response from {}:{}{} ended before headers", host, uint32(port), path);
    }

    if (expectedBodySize > 0 && receivedBodySize < expectedBodySize)
        return HYP_MAKE_ERROR(Error, "Truncated body from {}:{}{} — expected {} bytes, got {}",
            host, uint32(port), path, expectedBodySize, receivedBodySize);

    writer.Flush();

    return {};

#endif
}

static Map<AssetPath, uint64> BuildLocalAssetTimestampMap(const FilePath& cacheDir)
{
    Map<AssetPath, uint64> result;
    FilePath localManifest = cacheDir / "Manifest.hmf";

    if (!localManifest.Exists())
    {
        return result;
    }

    FileByteReader reader { localManifest };
    if (reader.Eof())
    {
        return result;
    }

    String localStr = String(reader.Read().ToByteView());

    HMF::ParseResult parseResult = HMF::Parse(localStr);
    if (!parseResult.HasValue())
    {
        return result;
    }

    BoxedValue& boxed = parseResult.GetValue();
    if (!boxed.Is<CookManifest>())
    {
        return result;
    }

    const CookManifest& localManifestData = boxed.Get<CookManifest>();
    for (const AssetEntry& localEntry : localManifestData.assets)
    {
        AssetPath key(localEntry.registryId,
            *AssetBuckets::AllBuckets[localEntry.bucketIndex],
            localEntry.name);

        result[key] = localEntry.lastModifiedTimestamp;
    }

    return result;
}

Result DownloadCacheFromHost(
    const ANSIStringView& host, uint16 port,
    const FilePath& cacheDir, const FilePath& contentDir,
    bool* outShouldRetry = nullptr)
{
    if (outShouldRetry)
    {
        *outShouldRetry = false;
    }

    MemoryByteWriter<DynamicAllocator> manifestWriter;
    if (Result res = HttpGetBytes(host, port, "/manifest", manifestWriter, outShouldRetry); res.HasError())
    {
        return res;
    }

    ByteBuffer& manifestBytes = manifestWriter.GetBuffer();

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
        serverManifest.cookTimestamp, serverManifest.assets.Size());

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
                    localTimestamp = localBoxed.Get<CookManifest>().cookTimestamp;
                }
            }
        }
    }

    if (serverManifest.cookTimestamp <= localTimestamp)
    {
        HYP_LOG(Assets, Info, "CacheSync cache up to date (local={} >= server={})",
            localTimestamp, serverManifest.cookTimestamp);

        return {};
    }

    HYP_LOG(Assets, Info, "CacheSync cache outdated (local={} < server={}), downloading",
        localTimestamp, serverManifest.cookTimestamp);

    if (!cacheDir.Exists() && !cacheDir.MkDir())
    {
        return HYP_MAKE_ERROR(Error,"CacheSync failed to create cache directory '{}'", cacheDir);
    }

    Array<uint64> blockSizes;
    blockSizes.Resize(MaxAssetBuckets);

    for (const AssetEntry& entry : serverManifest.assets)
    {
        if (!entry.bucketIndex || entry.bucketIndex >= MaxAssetBuckets)
        {
            continue;
        }

        for (const BlobEntry& blob : entry.blobs)
        {
            const size_t alignedSize = ByteUtil::AlignAs(blockSizes[entry.bucketIndex], alignof(BlobHeader))
                + sizeof(BlobHeader) + blob.size;

            blockSizes[entry.bucketIndex] = alignedSize;
        }
    }

    Array<BlobBlockInfo> blocks;
    for (uint32 bucketIndex = 1; bucketIndex < MaxAssetBuckets; bucketIndex++)
    {
        if (blockSizes[bucketIndex] > 0)
        {
            blocks.PushBack(BlobBlockInfo { bucketIndex, blockSizes[bucketIndex] });
        }
    }

    BlobStorage writeStorage(cacheDir, /* readOnly */ false);
    writeStorage.Initialize();

    if (Result result = writeStorage.BeginCook(blocks); result.HasError())
    {
        return HYP_MAKE_ERROR(Error,"CacheSync BeginCook failed: {}", result.GetError().GetMessage());
    }

    using threading::TaskThreadPool;

    TaskThreadPool downloadPool("CacheSyncWorker", 4);
    downloadPool.Start();

    List<Task<void>> tasks;

    AtomicVar<int32> failureCount = 0;

    // Build a map of local asset timestamps so we can skip up-to-date assets.
    // Assets are sorted descending by timestamp, so we can break on first match.
    Map<AssetPath, uint64> localAssetTimestamps = BuildLocalAssetTimestampMap(cacheDir);

    for (const AssetEntry& entry : serverManifest.assets)
    {
        if (!entry.bucketIndex || entry.bucketIndex >= MaxAssetBuckets)
        {
            continue;
        }

        // Skip if we already have this asset at an equal-or-newer timestamp.
        // Assets are sorted descending, so all remaining are also up-to-date.
        {
            AssetPath key(entry.registryId, *AssetBuckets::AllBuckets[entry.bucketIndex], entry.name);

            auto it = localAssetTimestamps.Find(key);

            if (it != localAssetTimestamps.End() && it->second >= entry.lastModifiedTimestamp)
            {
                break;
            }
        }

        tasks.EmplaceBack(downloadPool.Enqueue(HYP_STATIC_MESSAGE("CacheSyncAsset"), [&]() -> void
        {
            bool entryFailed = false;

            // Download the HMF manifest
            {
                char hmfPathBuf[512];
                std::snprintf(hmfPathBuf, sizeof(hmfPathBuf), "/hmf/%u/%s",
                    entry.bucketIndex, entry.name.LookupString());

                FilePath bucketContentDir = contentDir / String(GetAssetBucketName(entry.bucketIndex));
                bucketContentDir.MkDir();

                FilePath hmfFilePath = bucketContentDir / (entry.name.ToString() + ".hmf");
                FileByteWriter hmfWriter { hmfFilePath };

                if (hmfWriter.IsOpen())
                {
                    if (Result res = HttpGetBytes(host, port, hmfPathBuf, hmfWriter); res.HasError())
                    {
                        HYP_LOG(Assets, Warning, "Failed to download HMF for {}: {}", entry.name, res.GetError().GetMessage());
                    }
                    hmfWriter.Close();
                }
            }

            // Download each blob and write via PutData
            for (const BlobEntry& blob : entry.blobs)
            {
                char blobPathBuf[512];
                std::snprintf(blobPathBuf, sizeof(blobPathBuf), "/blob?key=%llx&size=%llu",
                    (unsigned long long)blob.key, (unsigned long long)blob.size);

                MemoryByteWriter<DynamicAllocator> blobWriter;
                if (Result res = HttpGetBytes(host, port, blobPathBuf, blobWriter); res.HasError())
                {
                    HYP_LOG(Assets, Error, "CacheSync failed to download blob key={} for {}: {}",
                            blob.key, entry.name, res.GetError().GetMessage());

                    entryFailed = true;
                    continue;
                }

                ByteBuffer& blobData = blobWriter.GetBuffer();

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

                if (!writeStorage.PutData(entry.bucketIndex, StringHash(blob.key), header, blobData.Data()))
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

    {
        localManifestPath.Remove();

        FileByteWriter localManifestWriter { localManifestPath };
        localManifestWriter.WriteString(manifestStr.ToUtf8());
        localManifestWriter.Close();
    }

    HYP_LOG(Assets, Info, "CacheSync complete. {} assets, {} blocks",
        serverManifest.assets.Size(), blocks.Size());

    return {};
}

} // anonymous

HYP_EXPORT void SyncCacheBlocking(const FilePath& cacheDir, const FilePath& contentDir, bool shouldRetry)
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

        do
        {
            Result res;
            if ((res = DownloadCacheFromHost(host, port, cacheDir, contentDir, shouldRetry ? &shouldRetry : nullptr)); !res.HasError())
            {
                break;
            }

            ++numAttempts;

            if (shouldRetry && numAttempts < MaxAttempts)
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
        while (shouldRetry);
    }
}

} // namespace CacheSync

} // namespace Hyperion