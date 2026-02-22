/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Core/containers/String.hpp>
#include <Core/containers/FixedArray.hpp>

#include <Core/utilities/Time.hpp>
#include <Core/utilities/FormatFwd.hpp>

#include <Core/Defines.hpp>

#include <Core/filesystem/FsUtil.hpp>

#include <Core/HashCode.hpp>

namespace Hyperion {

class BufferedReader;

namespace filesystem {

class FilePath : public String
{
public:
    using Base = String;

    FilePath()
        : String()
    {
    }

    FilePath(const String::ValueType* str)
        : String(str)
    {
    }

    template <int TStringType>
    FilePath(const utilities::StringView<TStringType>& stringView)
        : String(stringView)
    {
    }

    FilePath(const String& str)
        : String(str)
    {
    }

    FilePath(String&& str) noexcept
        : String(std::move(str))
    {
    }

    FilePath(const FilePath& other)
        : String(other)
    {
    }

    FilePath& operator=(const FilePath& other)
    {
        String::operator=(other);

        return *this;
    }

    FilePath(FilePath&& other) noexcept
        : String(std::move(other))
    {
    }

    FilePath& operator=(FilePath&& other) noexcept
    {
        String::operator=(std::move(other));

        return *this;
    }

    ~FilePath() = default;

    FilePath operator+(const FilePath& other) const
    {
        return FilePath(Base::operator+(other));
    }

    FilePath operator+(const String& other) const
    {
        return FilePath(Base::operator+(other));
    }

    FilePath operator+(const char* str) const
    {
        return FilePath(Base::operator+(str));
    }

    FilePath& operator+=(const FilePath& other)
    {
        return *this = Base::operator+=(other);
    }

    FilePath& operator+=(const String& other)
    {
        return *this = Base::operator+=(other);
    }

    FilePath& operator+=(const char* str)
    {
        return *this = Base::operator+=(str);
    }

    FilePath operator/(const FilePath& other) const
    {
        return Join(Data(), other.Data());
    }

    FilePath operator/(const String& other) const
    {
        return Join(Data(), other.Data());
    }

    FilePath operator/(const char* str) const
    {
        return Join(Data(), str);
    }

    FilePath& operator/=(const FilePath& other)
    {
        return *this = Join(Data(), other.Data());
    }

    FilePath& operator/=(const String& other)
    {
        return *this = Join(Data(), other.Data());
    }

    FilePath& operator/=(const char* str)
    {
        return *this = Join(Data(), str);
    }

    HYP_API bool Exists() const;
    HYP_API bool IsDirectory() const;
    HYP_API bool MkDir() const;

    HYP_API bool CanWrite() const;
    HYP_API bool CanRead() const;

    HYP_API String GetExtension() const;
    HYP_NODISCARD String StripExtension() const;

    HYP_API Time LastModifiedTimestamp() const;

    HYP_API SizeType FileSizeOnDisk() const;

    HYP_API String Basename() const;

    HYP_API FilePath BasePath() const;

    /*! \brief Remove the file or directory at the path.
     *
     * \return true if the file or directory was removed, false otherwise.
     */
    HYP_API bool Remove() const;

    HYP_API bool Rename(const FilePath& newPath) const;

    static inline FilePath Current()
    {
        return FilePath(FileSystem::CurrentPath().c_str());
    }

    static inline FilePath Relative(const FilePath& path, const FilePath& base)
    {
        return FilePath(FileSystem::RelativePath(path.Data(), base.Data()).c_str());
    }

    template <class... Strings>
    static inline FilePath Join(Strings&&... args)
    {
        FixedArray<String, sizeof...(args)> argsArray = { args... };

        enum
        {
            SEPARATOR_MODE_WINDOWS,
            SEPARATOR_MODE_UNIX
        } separatorMode;

        if (!std::strcmp(HYP_FILESYSTEM_SEPARATOR, "\\"))
        {
            separatorMode = SEPARATOR_MODE_WINDOWS;
        }
        else
        {
            separatorMode = SEPARATOR_MODE_UNIX;
        }

        for (auto& arg : argsArray)
        {
            if (separatorMode == SEPARATOR_MODE_WINDOWS)
            {
                arg = arg.ReplaceAll("/", "\\");
            }
            else
            {
                arg = arg.ReplaceAll("\\", "/");
            }
        }

        return FilePath(String::Join(argsArray, HYP_FILESYSTEM_SEPARATOR));
    }

    HYP_API Hyperion::containers::Array<FilePath, DynamicAllocator> GetAllFilesInDirectory() const;
    HYP_API Hyperion::containers::Array<FilePath, DynamicAllocator> GetSubdirectories() const;

    HYP_API SizeType DirectorySize() const;
    HYP_API SizeType FileSize() const;
};
} // namespace filesystem

using filesystem::FilePath;

namespace utilities {

template <class StringType>
struct Formatter<StringType, FilePath>
{
    auto operator()(const FilePath& value) const
    {
        if constexpr (std::is_base_of_v<StringType, FilePath>)
        {
            return static_cast<StringType>(value);
        }
        else
        {
            return StringType(value.Data());
        }
    }
};

} // namespace utilities

} // namespace Hyperion
