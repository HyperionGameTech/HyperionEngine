/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/FileSystem/FilePath.hpp>

namespace Hyperion {

enum class AssetRegistryId : uint32;

namespace CacheClient {

struct Params
{
    AssetRegistryId registryId;
    FilePath outputCacheDir;
    FilePath outputContentDir;
};

ENGINE_API void SyncContent(const Params& params, bool shouldRetry);

} // namespace CacheClient
} // namespace Hyperion