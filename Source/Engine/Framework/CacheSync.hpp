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

ENGINE_API void SyncCacheBlocking(const FilePath& cacheDir, const FilePath& contentDir);

} // namespace CacheSync

} // namespace Hyperion