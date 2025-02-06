/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FILE_PATH_HPP
#define HYPERION_FILE_PATH_HPP

#include <core/containers/String.hpp>
#include <core/containers/ContainerBase.hpp>
#include <core/utilities/Time.hpp>
#include <core/utilities/FormatFwd.hpp>
#include <core/Defines.hpp>

#include <core/filesystem/FsUtil.hpp>

#include <HashCode.hpp>

namespace hyperion {

class BufferedReader;

namespace filesystem {

class FilePath : public String
{
protected:
    using Base = String;

public:
    FilePath()
        : String()
    {
    }

    FilePath(const String::ValueType *str)
        : String(str)
    {
    }

    FilePath(const FilePath &other)
        : String(other)
    {
    }

    FilePath(const String &str)
        : String(str)
    {
    }

    FilePath(FilePath &&other) noexcept
        : String(std::move(other))
    {
    }

    FilePath(String &&str) noexcept
        : String(std::move(str))
    {
    }

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

    HYP_API bool Exists() const;
    HYP_API bool IsDirectory() const;
    HYP_API bool MkDir() const;

    HYP_API String GetExtension() const;
    HYP_NODISCARD String StripExtension() const;

    HYP_API Time LastModifiedTimestamp() const;

    HYP_API String Basename() const;

    HYP_API FilePath BasePath() const;

    HYP_API bool Open(BufferedReader &out) const;

    /*! \brief Remove the file or directory at the path.
     *
     * \return true if the file or directory was removed, false otherwise.
     */
    HYP_API bool Remove() const;

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

    HYP_API hyperion::Array<FilePath> GetAllFilesInDirectory() const;
    HYP_API hyperion::Array<FilePath> GetSubdirectories() const;

    HYP_API SizeType DirectorySize() const;
    HYP_API SizeType FileSize() const;
};
} // namespace filesystem

using filesystem::FilePath;

namespace utilities {

template <class StringType>
struct Formatter<StringType, FilePath>
{
    auto operator()(const FilePath &value) const
    {
        if constexpr (std::is_base_of_v<StringType, FilePath>) {
            return static_cast<StringType>(value);
        } else {
            return StringType(value.Data());
        }
    }
};

} // namespace utilities

} // namespace hyperion

#endif
