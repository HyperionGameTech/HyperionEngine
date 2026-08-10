/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Framework/CacheClient.hpp>
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

#include <System/MessageBox.hpp>

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

namespace CacheClient {

HYP_DEFINE_LOG_SUBCHANNEL(CacheClient, Engine);

namespace {

Result HttpGetBytes(
    const ANSIString& host, uint16 port,
    const ANSIString& path,
    ByteWriter& writer,
    bool* outShouldRetry = nullptr)
{
    if (outShouldRetry)
    {
        *outShouldRetry = false;
    }

    HYP_LOG(CacheClient, Verbose, "GET {}:{}{}", host, uint32(port), path);

    char portStr[16];
    std::snprintf(portStr, sizeof(portStr), "%u", uint32(port));

#if defined(HYP_WINDOWS)

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        return HYP_MAKE_ERROR(Error, "WSAStartup failed");
    }

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

        return HYP_MAKE_ERROR(Error, "Failed to resolve host");
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

        return HYP_MAKE_ERROR(Error, "Failed to connect");
    }

    DWORD timeout = 10000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    char request[4096];
    int reqLen = std::snprintf(request, sizeof(request),
        "GET %s HTTP/1.0\r\n"
        "Host: %s:%u\r\n"
        "Connection: close\r\n"
        "\r\n",
         path.Data(), host.Data(), uint32(port));

    if (send(sock, request, reqLen, 0) == SOCKET_ERROR)
    {
        closesocket(sock);
        WSACleanup();

        return HYP_MAKE_ERROR(Error, "Failed to send request");
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
            {
                return HYP_MAKE_ERROR(Error, "Response headers too large");
            }

            Memory::Copy(headerBuf + headerLen, recvBuf, size_t(n));
            headerLen += size_t(n);

            const char* bodyEnd = strstr(headerBuf, "\r\n\r\n");
            if (bodyEnd != nullptr)
            {
                const char* statusStart = strchr(headerBuf, ' ');
                if (!statusStart)
                {
                    return HYP_MAKE_ERROR(Error, "Invalid HTTP response");
                }

                int statusCode = atoi(statusStart + 1);
                if (statusCode != 200)
                {
                    return HYP_MAKE_ERROR(Error, "Server returned HTTP {}", statusCode);
                }

                // Parse Content-Length
                const char* cl = strstr(headerBuf, "Content-Length:");
                
                if (cl != nullptr && cl < bodyEnd)
                {
                    expectedBodySize = size_t(atoi(cl + 15));
                }

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
    {
        return HYP_MAKE_ERROR(Error, "Failed to receive response");
    }

    if (!headersDone)
    {
        return HYP_MAKE_ERROR(Error, "Response ended before headers");
    }

    if (expectedBodySize > 0 && receivedBodySize < expectedBodySize)
    {
        return HYP_MAKE_ERROR(Error, "Expected {} bytes, got {}", expectedBodySize, receivedBodySize);
    }

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

        return HYP_MAKE_ERROR(Error, "Failed to resolve host");
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

        return HYP_MAKE_ERROR(Error, "Failed to connect");
    }

    struct timeval tv = { 10, 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    char request[4096];
    int reqLen = std::snprintf(request, sizeof(request),
        "GET %s HTTP/1.0\r\n"
        "Host: %s:%u\r\n"
        "Connection: close\r\n"
        "\r\n",
         path.Data(), host.Data(), uint32(port));

    if (send(sock, request, size_t(reqLen), 0) < 0)
    {
        close(sock);

        return HYP_MAKE_ERROR(Error, "Failed to send request");
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
                return HYP_MAKE_ERROR(Error, "Response headers too large");
            }

            Memory::Copy(headerBuf + headerLen, recvBuf, size_t(n));
            headerLen += size_t(n);

            const char* bodyEnd = strstr(headerBuf, "\r\n\r\n");

            if (bodyEnd != nullptr)
            {
                const char* statusStart = strchr(headerBuf, ' ');
                if (!statusStart)
                {
                    return HYP_MAKE_ERROR(Error, "Invalid HTTP response");
                }

                int statusCode = atoi(statusStart + 1);
                if (statusCode != 200)
                {
                    return HYP_MAKE_ERROR(Error, "Server returned HTTP {}", statusCode);
                }

                // Parse Content-Length
                const char* cl = strstr(headerBuf, "Content-Length:");
                if (cl != nullptr && cl < bodyEnd)
                {
                    expectedBodySize = size_t(atoi(cl + 15));
                }

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
        return HYP_MAKE_ERROR(Error, "Failed to receive response");
    }

    if (!headersDone)
    {
        return HYP_MAKE_ERROR(Error, "Response ended before headers");
    }

    if (expectedBodySize > 0 && receivedBodySize < expectedBodySize)
        return HYP_MAKE_ERROR(Error, "Expected {} bytes, got {}", expectedBodySize, receivedBodySize);

    writer.Flush();

    return {};

#endif
}

inline String GetManifestFileName(const Params& params)
{
    return GetAssetRegistryName(params.registryId);
}

Map<AssetPath, uint64> BuildLocalAssetTimestampMap(const Params& params)
{
    Map<AssetPath, uint64> result;
    FilePath localManifest = params.outputCacheDir / GetManifestFileName(params) + ".hmf";

    if (!localManifest.Exists())
    {
        return result;
    }

    FileByteReader reader { localManifest };
    if (reader.Eof())
    {
        return result;
    }

    HMF::ParseResult parseResult = HMF::Parse(localManifest, reader);
    if (!parseResult.HasValue())
    {
        return result;
    }

    BoxedValue& boxed = parseResult.GetValue();
    if (!boxed.Is<ServerManifest>())
    {
        return result;
    }

    const ServerManifest& localManifestData = boxed.Get<ServerManifest>();
    for (const AssetEntry& localEntry : localManifestData.assets)
    {
        result[localEntry.path] = localEntry.lastModifiedTimestamp;
    }

    return result;
}

Result DownloadCacheFromHost(
    const ANSIStringView& host, uint16 port,
    const Params& params,
    bool* outShouldRetry = nullptr)
{
    if (outShouldRetry)
    {
        *outShouldRetry = false;
    }

    MemoryByteWriter<DynamicAllocator> manifestWriter;
    if (Result res = HttpGetBytes(host, port, String("/manifest?id=") + String::ToString(static_cast<uint32>(params.registryId)), manifestWriter, outShouldRetry); res.HasError())
    {
        return res;
    }

    ByteBuffer& manifestBytes = manifestWriter.GetBuffer();
    MemoryByteReader manifestReader(manifestBytes.ToByteView());

    HMF::ParseResult parseResult = HMF::Parse(manifestReader);
    if (!parseResult.HasValue())
    {
        return HYP_MAKE_ERROR(Error,"CacheSync failed to parse server manifest");
    }

    BoxedValue& boxedResult = parseResult.GetValue();
    if (!boxedResult.Is<ServerManifest>())
    {
        return HYP_MAKE_ERROR(Error, "CacheSync server manifest is not a CookManifest");
    }

    const ServerManifest& serverManifest = boxedResult.Get<ServerManifest>();

    HYP_LOG(CacheClient, Verbose, "CacheSync timestamp={}, {} assets",
        serverManifest.timestamp, serverManifest.assets.Size());

    // Compare with local manifest
    uint64 localTimestamp = 0;
    FilePath localManifestPath = params.outputCacheDir / GetManifestFileName(params) + ".hmf";

    if (localManifestPath.Exists())
    {
        FileByteReader localReader { localManifestPath };
        if (!localReader.Eof())
        {
            HMF::ParseResult localResult = HMF::Parse(localManifestPath, localReader);

            if (localResult.HasValue())
            {
                BoxedValue& localBoxed = localResult.GetValue();

                if (localBoxed.Is<ServerManifest>())
                {
                    localTimestamp = localBoxed.Get<ServerManifest>().timestamp;
                }
            }
        }
    }

    //if (serverManifest.timestamp <= localTimestamp)
    //{
    //    HYP_LOG(CacheClient, Verbose, "CacheSync cache up to date  (local={} >= server={})",
    //        localTimestamp, serverManifest.timestamp);

    //    return {};
    //}

    HYP_LOG(CacheClient, Verbose, "CacheSync cache outdated (local={} < server={})",
        localTimestamp, serverManifest.timestamp);

    if (!params.outputCacheDir.Exists() && !params.outputCacheDir.MkDir())
    {
        return HYP_MAKE_ERROR(Error,"CacheSync failed to create cache directory '{}'", params.outputCacheDir);
    }

    BlobStorage& storage = *EngineGlobals::GetBlobStorage();
    storage.Lock(params.outputCacheDir, /* readOnly */ false);

    bool locked = true;

    HYP_DEFER({
        if (locked)
        {
            storage.Unlock();
        }
    });

    Map<AssetPath, uint64> localAssetTimestamps = BuildLocalAssetTimestampMap(params);

    // Work out which blobs actually have to be written before touching the blocks, so each one can
    // be grown by exactly the amount the new data needs.
    //
    // A blob is left alone when its asset is already current locally *and* its bytes are still
    // present and in range in the block file. Those keep their existing offsets, so nothing is
    // re-downloaded, nothing is rewritten, and nothing has to be held in memory to survive the
    // cook. Anything else -- stale asset, missing blob, or a table of contents entry that no longer
    // matches the block file -- gets fetched and appended.
    //
    // Note that a replaced blob is appended at the tail rather than written back over its old
    // bytes. Blobs are addressed by an offset in the table of contents rather than by their position
    // within the block, so one growing past its old extent doesn't disturb its neighbours and there's
    // no need to shuffle (or re-fetch) whatever follows it. The cost is that the bytes it leaves
    // behind are dead space until the block is next rebuilt from scratch.
    Set<uint64> blobKeysToWrite;

    Array<uint64> additionalBlockSizes;
    additionalBlockSizes.Resize(MaxAssetBuckets);

    for (const AssetEntry& entry : serverManifest.assets)
    {
        const bool isUpToDateLocally = [&]() -> bool
        {
            auto it = localAssetTimestamps.Find(entry.path);

            return it != localAssetTimestamps.End() && it->second >= entry.lastModifiedTimestamp;
        }();

        for (const BlobEntry& blob : entry.blobs)
        {
            if (isUpToDateLocally && storage.HasData(StringHash(blob.key), blob.size))
            {
                continue;
            }

            if (!blobKeysToWrite.Insert(blob.key).second)
            {
                // Already accounted for
                continue;
            }

            // Pad by one alignment per blob so the reservation still covers the padding PutData
            // inserts between blobs, wherever in the block the first one happens to land.
            additionalBlockSizes[entry.path.bucketIndex] += sizeof(BlobHeader) + blob.size + alignof(BlobHeader);
        }
    }

    Array<BlobBlockInfo> blocks;
    for (uint32 bucketIndex = 1; bucketIndex < MaxAssetBuckets; bucketIndex++)
    {
        if (additionalBlockSizes[bucketIndex] > 0)
        {
            blocks.PushBack(BlobBlockInfo { bucketIndex, additionalBlockSizes[bucketIndex] });
        }
    }

    Array<uint32> resetBuckets;

    if (Result result = storage.BeginCook(blocks, /* zeroize */ false, &resetBuckets); result.HasError())
    {
        return HYP_MAKE_ERROR(Error,"CacheSync BeginCook failed: {}", result.GetError().GetMessage());
    }

    // A bucket that comes back reset had nothing to preserve, so any blob we decided to keep in it
    // is gone. HasData()'s bounds check should already have caught that above, so this only guards
    // against the two disagreeing -- bail and let the retry re-sync rather than go on to record a
    // manifest claiming we hold data we don't.
    for (uint32 bucketIndex : resetBuckets)
    {
        for (const AssetEntry& entry : serverManifest.assets)
        {
            if (entry.path.bucketIndex != bucketIndex)
            {
                continue;
            }

            for (const BlobEntry& blob : entry.blobs)
            {
                if (!blobKeysToWrite.Contains(blob.key))
                {
                    return HYP_MAKE_ERROR(Error, "CacheSync kept blob key={} in bucket '{}' but the block had no data to preserve",
                        blob.key, GetAssetBucketName(bucketIndex));
                }
            }
        }
    }

    using threading::TaskThreadPool;

    TaskThreadPool downloadPool("CacheSyncWorker", 4);
    downloadPool.Start();

    List<Task<void>> tasks;

    AtomicVar<int32> failureCount = 0;

    for (const AssetEntry& entry : serverManifest.assets)
    {
        tasks.EmplaceBack(downloadPool.Enqueue(HYP_STATIC_MESSAGE("CacheSyncAsset"), [&]() -> void
        {
            bool entryFailed = false;

            // Download the HMF manifest
            {
                char hmfPathBuf[512];
                std::snprintf(hmfPathBuf, sizeof(hmfPathBuf), "/hmf/%u/%s?id=%u",
                    entry.path.bucketIndex, *entry.path.GetName(),
                    params.registryId);

                FilePath bucketContentDir = params.outputContentDir / String(entry.path.GetBucket().GetName());
                bucketContentDir.MkDir();

                FilePath hmfFilePath = bucketContentDir / (entry.path.assetName.ToString() + ".hmf");

                // Check if the file exists already and has a timestamp >= than the timestamp we know;
                // if we're downloading for multiple scenes, then it may have been already downloaded.
                // Only the HMF is skipped here -- the blobs below are tracked separately and can
                // still be missing from the cache even when the HMF on disk is current.
                const bool hmfUpToDate = hmfFilePath.Exists()
                    && hmfFilePath.LastModifiedTimestamp() >= entry.lastModifiedTimestamp;

                if (!hmfUpToDate)
                {
                    FileByteWriter hmfWriter { hmfFilePath };

                    if (hmfWriter.IsOpen())
                    {
                        if (Result res = HttpGetBytes(host, port, hmfPathBuf, hmfWriter); res.HasError())
                        {
                            HYP_LOG(CacheClient, Warning, "Failed to download HMF for {}: {}", entry.path.ToString(), res.GetError().GetMessage());
                        }

                        hmfWriter.Close();
                    }
                }
            }

            // Download each blob we don't already hold and append it via PutData. Only the blobs
            // in flight right now are resident; anything already cached stays where it lies.
            for (const BlobEntry& blob : entry.blobs)
            {
                if (!blobKeysToWrite.Contains(blob.key))
                {
                    continue;
                }

                char blobPathBuf[512];
                std::snprintf(blobPathBuf, sizeof(blobPathBuf), "/blob?id=%u&key=%llx&size=%llu",
                    params.registryId,
                    blob.key, blob.size);

                MemoryByteWriter<DynamicAllocator> blobWriter;
                if (Result res = HttpGetBytes(host, port, blobPathBuf, blobWriter); res.HasError())
                {
                    HYP_LOG(CacheClient, Error, "CacheSync failed to download blob key={} for {}: {}",
                            blob.key, entry.path.assetName, res.GetError().GetMessage());

                    entryFailed = true;
                    continue;
                }

                ByteBuffer& blobData = blobWriter.GetBuffer();

                if (blobData.Size() != blob.size)
                {
                    HYP_LOG(CacheClient, Warning, "Blob size mismatch! ({} != {}) entry: {}, blob key: {}",
                            blobData.Size(), blob.size,
                            entry.path.assetName, blob.key);

                    entryFailed = true;
                    continue;
                }

                BlobHeader header {};

                const size_t magicLen = blob.magic.Size();
                Memory::Copy((char*)header.magic, blob.magic.Data(), MathUtil::Min(magicLen, sizeof(header.magic)));
                
                header.version = 1;
                header.payloadOffset = 0;
                header.payloadSize = blob.size;

                if (!storage.PutData(entry.path.bucketIndex, StringHash(blob.key), header, blobData.Data()))
                {
                    HYP_LOG(CacheClient, Error, "CacheSync PutData failed for blob key={}", blob.key);
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

    if (Result result = storage.FinishCook(); result.HasError())
    {
        return HYP_MAKE_ERROR(Error, "FinishCook failed: {}", result.GetError().GetMessage());
    }

    storage.Unlock();
    locked = false;

    // Only record the manifest once everything it claims is really in the cache -- otherwise the
    // next run reads it back, treats the assets that failed as up to date, and never retries them.
    if (const int32 numFailures = failureCount.Get(MemoryOrder::RELAXED); numFailures != 0)
    {
        HYP_LOG(CacheClient, Warning, "CacheSync failed to fetch {} of {} assets",
            numFailures, serverManifest.assets.Size());
    }

    {
        localManifestPath.Remove();

        FileByteWriter localManifestWriter { localManifestPath };
        localManifestWriter.Write(manifestBytes.ToByteView());
        localManifestWriter.Close();
    }

    HYP_LOG(CacheClient, Verbose, "Cache download complete. {} assets, {} blobs fetched, {} blocks grown",
        serverManifest.assets.Size(), blobKeysToWrite.Size(), blocks.Size());

    return {};
}

} // anonymous

HYP_EXPORT Result SyncContent(const Params& params)
{
    const int maxAttempts = MathUtil::Max(params.numAttempts, 1);
    const bool shouldRetry = maxAttempts > 1;

    int numAttempts = 0;

    if (params.cacheServer.Empty())
    {
        return HYP_MAKE_ERROR(Error, "No cache server set.");
    }

    const size_t colonPos = params.cacheServer.FindLastIndex(":");

    if (colonPos == String::NotFound)
    {
        return HYP_MAKE_ERROR(Error, "Invalid cache server address: {}", params.cacheServer);
    }

    ANSIStringView host = params.cacheServer.Substr(0, colonPos);

    // Chomp off the protocol.
    if (host.Size() > 7 && Memory::Compare(host.Data(), "http://", 7) == 0)
    {
        host = host.Substr(7, SIZE_MAX);
    }
    else if (host.Size() > 8 && Memory::Compare(host.Data(), "https://", 8) == 0)
    {
        host = host.Substr(8, SIZE_MAX);
    }

    ANSIStringView portStr = params.cacheServer.Substr(colonPos + 1, SIZE_MAX);
    uint16 port = static_cast<uint16>(std::atoi(portStr.Data()));

    do
    {
        Result res;

        bool retryThisType = shouldRetry;

        if ((res = DownloadCacheFromHost(host, port, params, shouldRetry ? &retryThisType : nullptr)); !res.HasError())
        {
            // OK
            return res;
        }

        ++numAttempts;

        if (shouldRetry && numAttempts < maxAttempts)
        {
            HYP_LOG(CacheClient, Error, "Failed to download cache from server! Error message was: {}\nRetrying in 5s...", res.GetError().GetMessage());

            ThreadSleep(5000);
        }
        else
        {
            return res.GetError();
        }
    }
    while (shouldRetry);
        
    return HYP_MAKE_ERROR(Error, "Failed to connect due to unknown reasons");
}

HYP_EXPORT void SyncFailed(const Error& error, bool& outClickedRetry, bool& outClickedExit)
{
    outClickedRetry = false;
    outClickedExit = false;

    // clang-format off
    SystemMessageBox(MessageBoxType::CRITICAL)
        .Title("Sync Content Failed")
        .Text(String("Failed to download core content required to start the game.\n"
            "Ensure a proper internet connection and try again.\n\n"
            "Message details: ") + error.GetMessage())
        .Button("Retry", [&outClickedRetry] { outClickedRetry = true; })
        .Button("Exit", [&outClickedExit] { outClickedExit = true; })
        .Show();
    // clang-format on
}

} // namespace CacheClient

} // namespace Hyperion