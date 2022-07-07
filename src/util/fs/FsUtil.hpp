#ifndef HYPERION_V2_FS_UTIL_H
#define HYPERION_V2_FS_UTIL_H

#include <util/Defines.hpp>
#include <util/StringUtil.hpp>

#include <string>
#include <cstring>
#include <array>

namespace hyperion::v2 {

class FileSystem {
public:
    static bool DirExists(const std::string &path);
    static int Mkdir(const std::string &path);
    static std::string CurrentPath();
    static std::string RelativePath(const std::string &path, const std::string &base);

    template <class ...String>
    static inline std::string Join(String &&... args)
    {
        std::array<std::string, sizeof...(args)> args_array = {args...};

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

} // namespace hyperion::v2

#endif