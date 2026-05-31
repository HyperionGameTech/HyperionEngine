/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/filesystem/FilePath.hpp>

namespace Hyperion {

namespace CoreApi {
CORE_API extern FilePath GetExecutablePath();
} // namespace CoreApi

template <auto DirectoryStaticString, bool RelativeToExecutablePath = true>
struct DirectoryInitializer
{
    FilePath path;

    inline DirectoryInitializer()
    {
#ifdef HYP_ROOT_DIR
        // In non-debug modes, we always want resource directories to be relative to the executable path
        if (!RelativeToExecutablePath)
        {
            path = FilePath(HYP_ROOT_DIR) / String(DirectoryStaticString.Data());
        }
        else
#endif
        {
#if HYP_ANDROID
            path = FilePath(AndroidAssetPathPrefix) / String(DirectoryStaticString.Data());
#else

            path = CoreApi::GetExecutablePath() / String(DirectoryStaticString.Data());
#endif
        }

#if HYP_EDITOR
        if (!path.Exists())
        {
            if (!path.MkDir())
            {
                HYP_FAIL("Failed to create resource directory: {}", path.Data());
            }
        }

        AssertDebug(path.Exists() && path.IsDirectory(), "Resource directory does not exist or is not a directory: {}", path.Data());
        AssertDebug(path.CanRead(), "Resource directory is not readable: {}", path.Data());
        AssertDebug(path.CanWrite(), "Resource directory is not writable: {}", path.Data());
#endif
    }
};

} // namespace Hyperion
