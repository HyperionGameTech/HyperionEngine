/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FS_UTIL_HPP
#define HYPERION_FS_UTIL_HPP

#include <core/Defines.hpp>
#include <core/utilities/StringUtil.hpp>

#include <string>
#include <cstring>
#include <array>
#include <mutex>

namespace hyperion {

class BufferedReader;

namespace filesystem {

class FilePath;

class FileSystem
{
    static std::mutex mtx;
    static Array<FilePath> filepaths;

public:
    static void PushDirectory(const FilePath &path);
    static FilePath PopDirectory();

    static bool DirExists(const std::string &path);
    static int MkDir(const std::string &path);
    static std::string CurrentPath();
    static std::string RelativePath(const std::string &path, const std::string &base);

    template <class ...String>
    static inline std::string Join(String &&... args)
    {
        std::array<std::string, sizeof...(args)> args_array = { args... };

        enum {
            SEPARATOR_MODE_WINDOWS,
            SEPARATOR_MODE_UNIX
        } separator_mode;

        if (!std::strcmp(HYP_FILESYSTEM_SEPARATOR, "\\")) {
            separator_mode = SEPARATOR_MODE_WINDOWS;
        } else {
            separator_mode = SEPARATOR_MODE_UNIX;
        }

        for (auto &arg : args_array) {
            if (separator_mode == SEPARATOR_MODE_WINDOWS) {
                arg = StringUtil::ReplaceAll(arg, "/", "\\");
            } else {
                arg = StringUtil::ReplaceAll(arg, "\\", "/");
            }
        }

        return StringUtil::Join(args_array, HYP_FILESYSTEM_SEPARATOR);
    }
};
} // namespace filesystem

using filesystem::FileSystem;

} // namespace hyperion

#endif