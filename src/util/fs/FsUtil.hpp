#ifndef HYPERION_V2_FS_UTIL_H
#define HYPERION_V2_FS_UTIL_H

#include <core/Containers.hpp>
#include <util/Defines.hpp>
#include <util/StringUtil.hpp>

#include <string>
#include <cstring>
#include <array>
#include <mutex>

namespace hyperion {

class FilePath;

template <SizeType BufferSize>
class BufferedReader;

using Reader = BufferedReader<HYP_READER_DEFAULT_BUFFER_SIZE>;

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

class FilePath : public String
{
protected:
    using Base = String;

public:
    FilePath() : String() { }
    FilePath(const String::ValueType *str) : String(str) { }
    FilePath(const FilePath &other) : String(other) { }
    FilePath(const String &str) : String(str) { }
    FilePath(FilePath &&other) noexcept : String(std::move(other)) { }
    FilePath(String &&str) noexcept : String(std::move(str)) { }
    ~FilePath() = default;

    FilePath &operator=(const FilePath &other)
    {
        String::operator=(other);

        return *this;
    }

    FilePath &operator=(FilePath &&other) noexcept
    {
        String::operator=(std::move(other));

        return *this;
    }

    FilePath operator+(const FilePath &other) const
    {
        return FilePath(Base::operator+(other));
    }

    FilePath operator+(const String &other) const
    {
        return FilePath(Base::operator+(other));
    }

    FilePath operator+(const char *str) const
    {
        return FilePath(Base::operator+(str));
    }

    FilePath &operator+=(const FilePath &other)
    {
        return *this = Base::operator+=(other);
    }

    FilePath &operator+=(const String &other)
    {
        return *this = Base::operator+=(other);
    }

    FilePath &operator+=(const char *str)
    {
        return *this = Base::operator+=(str);
    }

    FilePath operator/(const FilePath &other) const
    {
        return Join(Data(), other.Data());
    }

    FilePath operator/(const String &other) const
    {
        return Join(Data(), other.Data());
    }

    FilePath operator/(const char *str) const
    {
        return Join(Data(), str);
    }

    FilePath &operator/=(const FilePath &other)
    {
        return *this = Join(Data(), other.Data());
    }

    FilePath &operator/=(const String &other)
    {
        return *this = Join(Data(), other.Data());
    }

    FilePath &operator/=(const char *str)
    {
        return *this = Join(Data(), str);
    }

    bool Exists() const;
    bool IsDirectory() const;
    Int MkDir() const;

    UInt64 LastModifiedTimestamp() const;

    String Basename() const
    {
        return String(StringUtil::Basename(Data()).c_str());
    }

    FilePath BasePath() const
    {
        return FilePath(StringUtil::BasePath(Data()).c_str());
    }

    bool Open(BufferedReader<HYP_READER_DEFAULT_BUFFER_SIZE> &out) const;

    static inline FilePath Current()
    {
        return FilePath(FileSystem::CurrentPath().c_str());
    }

    static inline FilePath Relative(const FilePath &path, const FilePath &base)
    {
        return FilePath(FileSystem::RelativePath(path.Data(), base.Data()).c_str());
    }

    template <class ... Paths>
    static inline FilePath Join(Paths &&... paths)
    {
        const auto str = FileSystem::Join(std::forward<Paths>(paths)...);

        return FilePath(str.c_str());
    }
};

} // namespace hyperion

#endif