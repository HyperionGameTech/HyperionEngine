/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>

#include <Core/FileSystem/FilePath.hpp>

namespace Hyperion {

namespace CoreApi {
CORE_API extern const FilePath& GetExecutablePath();
CORE_API extern const FilePath& GetBaseDirectory();
} // namespace CoreApi

template <auto DirectoryStaticString, bool RelativeToExecutablePath = true>
struct DirectoryInitializer
{
    FilePath path;

    inline DirectoryInitializer()
    {
        // In non-debug modes, we always want resource directories to be relative to the executable path
        if (!RelativeToExecutablePath)
        {
            path = CoreApi::GetBaseDirectory() / String(DirectoryStaticString.Data());
        }
        else
        {
#if HYP_ANDROID
            path = FilePath(AndroidAssetPathPrefix) / String(DirectoryStaticString.Data());
#else

            path = CoreApi::GetExecutablePath() / String(DirectoryStaticString.Data());
#endif
        }

#ifndef HYP_SHIPPING
        if (!path.Exists())
        {
            if (!path.MkDir())
            {
                HYP_LOG(Core, Error, "Failed to create resource directory: {}", path.Data());
                return;
            }
        }

        AssertDebug(path.Exists() && path.IsDirectory(), "Resource directory does not exist or is not a directory: {}", path.Data());
        AssertDebug(path.CanRead(), "Resource directory is not readable: {}", path.Data());
        AssertDebug(path.CanWrite(), "Resource directory is not writable: {}", path.Data());
#endif
    }
};

} // namespace Hyperion
