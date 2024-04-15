/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <util/fs/FsUtil.hpp>
#include <core/Defines.hpp>
#include <system/Debug.hpp>
#include <asset/BufferedByteReader.hpp>

#include <filesystem>
#include <fstream>

#include <sys/stat.h>

#if defined(HYP_WINDOWS)
    #include <direct.h>
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN 1
    #endif
    #include <windows.h>
    #undef WIN32_LEAN_AND_MEAN
#elif defined(HYP_UNIX)
    #include <unistd.h>
#endif

namespace hyperion {

std::mutex FileSystem::mtx = std::mutex();
Array<FilePath> FileSystem::filepaths = { };

void FileSystem::PushDirectory(const FilePath &path)
{
    std::lock_guard guard(mtx);

    filepaths.PushBack(FilePath::Current());

#if defined(HYP_WINDOWS)
    _chdir(path.Data());
#elif defined(HYP_UNIX)
    chdir(path.Data());
#endif
}

FilePath FileSystem::PopDirectory()
{
    std::lock_guard guard(mtx);

    AssertThrow(filepaths.Any());

    auto current = FilePath::Current();

#if defined(HYP_WINDOWS)
    _chdir(filepaths.Back().Data());
#elif defined(HYP_UNIX)
    chdir(filepaths.Back().Data());
#endif

    filepaths.PopBack();

    return current;
}

bool FileSystem::DirExists(const std::string &path)
{
    struct stat st;
    
    if (stat(path.c_str(), &st) == 0) {
        return st.st_mode & S_IFDIR;
    }

    return false;
}

int FileSystem::MkDir(const std::string &path)
{
#if HYP_WINDOWS
    #define MKDIR_FUNCTION(path, mode) ::_mkdir(path)
#else
    #define MKDIR_FUNCTION(path, mode) ::mkdir(path, mode)
#endif

#ifdef HYP_UNIX
#define HYP_ISDIR(mode) S_ISDIR(mode)
#elif defined(HYP_WINDOWS)
#define HYP_ISDIR(mode) (((mode) & _S_IFDIR) > 0)
#endif

    struct stat st;

    int status = 0;
    for (SizeType pos = 0; pos < path.length(); ++pos) {
        if (path[pos] == '/' || pos == path.length() - 1) {
            std::string subdir = path.substr(0, pos + 1);
            if (stat(subdir.c_str(), &st) != 0) {
                // Directory doesn't exist
                if (MKDIR_FUNCTION(subdir.c_str(), 0755) != 0 && errno != EEXIST) {
                    status = -1;
                    break;
                }
            } else if (!HYP_ISDIR(st.st_mode)) {
                status = -1;
                break;
            }
        }
    }

    return status;

#undef MKDIR_FUNCTION
}

std::string FileSystem::CurrentPath()
{
    return std::filesystem::current_path().string();
}

std::string FileSystem::RelativePath(const std::string &path, const std::string &base)
{
    return std::filesystem::proximate(path, base).string();
}

} // namespace hyperion