/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Utilities/Result.hpp>

namespace Hyperion {

enum class AssetRegistryId : uint32;

namespace CacheClient {

struct Params
{
    ANSIString cacheServer;

    // 0 == Game
    AssetRegistryId registryId = static_cast<AssetRegistryId>(0);

    FilePath outputCacheDir;
    FilePath outputContentDir;

    int numAttempts = 1;
};

ENGINE_API Result SyncContent(const Params& params);
ENGINE_API void SyncFailed(const Error& error, bool& outClickedRetry, bool& outClickedExit);

} // namespace CacheClient
} // namespace Hyperion