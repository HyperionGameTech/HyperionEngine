/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <string>
#include <cstring>

namespace Hyperion {

class BufferedReader;

namespace filesystem {

class FilePath;

//// \todo : Refactor FsUtil to use FilePath instead of std::string

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

} // namespace Hyperion
