/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/String.hpp>
#include <Core/Containers/FixedArray.hpp>

#include <Core/Memory/Pimpl.hpp>

#include <Core/Utilities/Time.hpp>
#include <Core/Utilities/FormatFwd.hpp>

#include <Core/Defines.hpp>

#include <Core/FileSystem/FsUtil.hpp>

#include <Core/HashCode.hpp>

namespace Hyperion {

class BufferedReader;

namespace filesystem {

struct DirectoryIteratorImpl;

class CORE_API DirectoryIterator
{
public:
    DirectoryIterator();
    DirectoryIterator(Pimpl<DirectoryIteratorImpl>&& impl);

    DirectoryIterator(const DirectoryIterator&) = delete;
    DirectoryIterator& operator=(const DirectoryIterator&) = delete;

    DirectoryIterator(DirectoryIterator&& other) noexcept;
    DirectoryIterator& operator=(DirectoryIterator&& other) noexcept;

    ~DirectoryIterator();

    /*! \brief Returns true if the iterator was successfully opened and has not been exhausted. */
    bool HasNext() const;

    /*! \brief Returns true if the iterator was successfully opened and has not been exhausted. */
    explicit operator bool() const;

    /*! \brief Get the FilePath of the current entry. Only valid when IsValid() is true. */
    FilePath Current() const;

    /*! \brief Returns true if the current entry is a directory. Only valid when IsValid() is true. */
    bool CurrentIsDirectory() const;

    /*! \brief Move to the next entry. Returns true if there is another entry, false if exhausted. */
    bool Advance();

private:
    Pimpl<DirectoryIteratorImpl> m_impl;
};

class FilePath : public String
{
public:
    using Base = String;

    FilePath()
        : Base()
    {
    }

    FilePath(const typename Base::CharType* str)
        : Base(str)
    {
    }

    template <int TStringType>
    FilePath(const utilities::StringView<TStringType>& stringView)
        : Base(stringView)
    {
    }

    FilePath(const String& str)
        : Base(str)
    {
    }

    FilePath(String&& str) noexcept
        : Base(std::move(str))
    {
    }

    FilePath(const FilePath& other)
        : Base(other)
    {
    }

    FilePath& operator=(const FilePath& other)
    {
        Base::operator=(other);

        return *this;
    }

    FilePath(FilePath&& other) noexcept
        : Base(std::move(other))
    {
    }

    FilePath& operator=(FilePath&& other) noexcept
    {
        Base::operator=(std::move(other));

        return *this;
    }

    ~FilePath() = default;

    FilePath operator+(const FilePath& other) const
    {
        return FilePath(Base::operator+(other));
    }

    FilePath operator+(const Base& other) const
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

    FilePath& operator+=(const Base& other)
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

    FilePath operator/(const Base& other) const
    {
        return Join(Data(), other.Data());
    }

    FilePath operator/(const typename Base::CharType* str) const
    {
        return Join(Data(), str);
    }

    FilePath& operator/=(const FilePath& other)
    {
        return *this = Join(Data(), other.Data());
    }

    FilePath& operator/=(const Base& other)
    {
        return *this = Join(Data(), other.Data());
    }

    FilePath& operator/=(const typename Base::CharType* str)
    {
        return *this = Join(Data(), str);
    }

    CORE_API bool Exists() const;
    CORE_API bool IsDirectory() const;
    CORE_API bool MkDir() const;

    CORE_API bool CanWrite() const;
    CORE_API bool CanRead() const;

    CORE_API String GetExtension() const;
    HYP_NODISCARD CORE_API String StripExtension() const;

    CORE_API Time LastModifiedTimestamp() const;

    CORE_API size_t FileSizeOnDisk() const;

    CORE_API String Basename() const;

    CORE_API FilePath BasePath() const;

    /*! \brief Remove the file or directory at the path.
     *
     * \return true if the file or directory was removed, false otherwise.
     */
    CORE_API bool Remove() const;

    /*! \brief Remove the file or directory at the path, including any nested files and subdirectories.
     *
     * \return true if the path was removed (or did not exist), false otherwise.
     */
    CORE_API bool RemoveRecursively() const;

    CORE_API bool Rename(const FilePath& newPath) const;

    bool IsAbsolute() const
    {
#ifdef HYP_WINDOWS
        if (Size() >= 3 && (Data()[0] >= 'A' && Data()[0] <= 'Z' && Data()[1] == ':' && (Data()[2] == '/' || Data()[2] == '\\')))
        {
            return true;
        }
#endif // HYP_WINDOWS

        if (Empty())
        {
            return false;
        }

        return (Data()[0] == '/' || Data()[0] == '\\');
    }

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

        return FilePath(Base::Join(argsArray, HYP_FILESYSTEM_SEPARATOR));
    }
    
    HYP_NODISCARD FilePath ToRelative(const FilePath& other) const
    {
        return Relative(*this, other);
    }
    
    HYP_NODISCARD FilePath ToCanonical() const
    {
        containers::FatArray<String, InlineAllocator<8>> res;

        for (const auto& str : Split('/', '\\'))
        {
            if (str == ".." && !res.Empty())
            {
                res.PopBack();
            }
            else if (str != ".")
            {
                res.PushBack(str);
            }
        }

        return FilePath(Base::Join(res, HYP_FILESYSTEM_SEPARATOR));
    }

    CORE_API containers::Array<FilePath, DynamicAllocator> GetAllFilesInDirectory() const;
    CORE_API containers::Array<FilePath, DynamicAllocator> GetSubdirectories() const;

    /*! \brief Open the directory for iteration. Returns a DirectoryIterator positioned at the first entry.
     *  On Android, asset paths (prefixed with AndroidAssetPathPrefix) are iterated via the AssetManager.
     *  On failure (path doesn't exist, not a directory, etc.) returns an invalid iterator. */
    CORE_API DirectoryIterator OpenDirectory() const;

    CORE_API size_t DirectorySize() const;
    CORE_API size_t FileSize() const;
};
} // namespace filesystem

using filesystem::FilePath;
using filesystem::DirectoryIterator;

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
