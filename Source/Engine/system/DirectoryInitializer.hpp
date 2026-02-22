/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

#include <Core/filesystem/FilePath.hpp>

namespace Hyperion {

namespace CoreApi {
extern FilePath GetExecutablePath();
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
            path = FilePath(HYP_ROOT_DIR) / DirectoryStaticString.Data();
        }
        else
#endif
        {
            path = CoreApi::GetExecutablePath() / DirectoryStaticString.Data();
        }

        if (!path.Exists())
        {
            if (!path.MkDir())
            {
                HYP_FAIL("Failed to create resource directory: {}", path.Data());
            }
        }

        Assert(path.Exists() && path.IsDirectory(), "Resource directory does not exist or is not a directory: {}", path.Data());
        Assert(path.CanRead(), "Resource directory is not readable: {}", path.Data());
        Assert(path.CanWrite(), "Resource directory is not writable: {}", path.Data());
    }
};

} // namespace Hyperion