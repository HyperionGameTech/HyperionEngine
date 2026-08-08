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

namespace CacheSync {

struct CacheSyncParams
{
    Name sceneName;
    FilePath outputCacheDir;
    FilePath outputContentDir;
};

ENGINE_API void SyncCacheBlocking(const CacheSyncParams& params, bool shouldRetry);

} // namespace CacheSync

} // namespace Hyperion