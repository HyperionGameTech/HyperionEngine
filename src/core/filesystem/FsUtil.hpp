/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <string>
#include <cstring>

namespace hyperion {

class BufferedReader;

namespace filesystem {

class FilePath;

/// @TODO: Refactor FsUtil to use FilePath instead of std::string

class FileSystem
{
public:
    static bool DirExists(const std::string& path);
    static int MkDir(const std::string& path);
    static std::string CurrentPath();
    static std::string RelativePath(const std::string& path, const std::string& base);
};
} // namespace filesystem

using filesystem::FileSystem;

} // namespace hyperion
