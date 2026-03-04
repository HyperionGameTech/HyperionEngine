/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */
#include <Core/filesystem/FilePath.hpp>
#include <Core/filesystem/FsUtil.hpp>

#include <Core/utilities/StringUtil.hpp>

#include <Core/io/BufferedByteReader.hpp>

#include <filesystem>
#include <fstream>

#include <sys/stat.h>

#if defined(HYP_WINDOWS)
#include <direct.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#elif defined(HYP_UNIX)
#include <unistd.h>
#endif

namespace Hyperion {
namespace filesystem {

bool FilePath::MkDir() const
{
    std::error_code ec;

    if (Exists())
        return true;

    if (std::filesystem::create_directories(Data(), ec))
        return true;

    return false;
}

bool FilePath::CanWrite() const
{
    struct stat st;

    if (stat(Data(), &st) != 0)
    {
        return false;
    }

#ifdef HYP_WINDOWS
    return (st.st_mode & _S_IWRITE) != 0;
#else
    return (st.st_mode & S_IWUSR) != 0 || (st.st_mode & S_IWGRP) != 0 || (st.st_mode & S_IWOTH) != 0;
#endif
}

bool FilePath::CanRead() const
{
    struct stat st;
    if (stat(Data(), &st) != 0)
    {
        return false;
    }

#ifdef HYP_WINDOWS
    return (st.st_mode & _S_IREAD) != 0;
#else
    return (st.st_mode & S_IRUSR) != 0 || (st.st_mode & S_IRGRP) != 0 || (st.st_mode & S_IROTH) != 0;
#endif
}

HYP_API String FilePath::GetExtension() const
{
    return StringUtil::GetExtension(*this);
}

HYP_NODISCARD String FilePath::StripExtension() const
{
    return StringUtil::StripExtension(*this);
}

bool FilePath::Remove() const
{
    return std::filesystem::remove(Data());
}

bool FilePath::Rename(const FilePath& newPath) const
{
    std::error_code ec;
    std::filesystem::rename(Data(), newPath.Data(), ec);

    return !ec;
}

bool FilePath::Exists() const
{
    if (Empty())
    {
        return false;
    }

    struct stat st;

    return stat(Data(), &st) == 0;
}

bool FilePath::IsDirectory() const
{
    if (Empty())
    {
        return false;
    }

    struct stat st;

    if (stat(Data(), &st) == 0)
    {
        return st.st_mode & S_IFDIR;
    }

    return false;
}

Time FilePath::LastModifiedTimestamp() const
{
    struct stat st;

    if (stat(Data(), &st) != 0)
    {
        return Time(0);
    }

#ifdef HYP_MACOS
    return Time(st.st_mtimespec.tv_sec);
#else
    return Time(st.st_mtime);
#endif
}

size_t FilePath::FileSizeOnDisk() const
{
    struct stat st;

    if (stat(Data(), &st) != 0)
    {
        return 0;
    }

    return st.st_size;
}

String FilePath::Basename() const
{
    return StringUtil::Basename(*this);
}

FilePath FilePath::BasePath() const
{
    return FilePath(StringUtil::BasePath(*this));
}

Hyperion::containers::Array<FilePath, DynamicAllocator> FilePath::GetAllFilesInDirectory() const
{
    Hyperion::containers::Array<FilePath, DynamicAllocator> files;

    for (const auto& entry : std::filesystem::directory_iterator(Data()))
    {
        if (entry.is_regular_file())
        {
#ifdef HYP_WINDOWS
            files.PushBack(WideString(entry.path().c_str()).ToUtf8());
#else
            files.PushBack(entry.path().c_str());
#endif
        }
    }

    return files;
}

Hyperion::containers::Array<FilePath, DynamicAllocator> FilePath::GetSubdirectories() const
{
    Hyperion::containers::Array<FilePath, DynamicAllocator> files;

    for (const auto& entry : std::filesystem::directory_iterator(Data()))
    {
        if (entry.is_directory())
        {
#ifdef HYP_WINDOWS
            files.PushBack(WideString(entry.path().c_str()).ToUtf8());
#else
            files.PushBack(entry.path().c_str());
#endif
        }
    }

    return files;
}

size_t FilePath::DirectorySize() const
{
    size_t size = 0;

    for (const auto& entry : std::filesystem::directory_iterator(Data()))
    {
        if (entry.is_regular_file())
        {
            size += entry.file_size();
        }
    }

    return size;
}

size_t FilePath::FileSize() const
{
    struct stat st;

    if (stat(Data(), &st) != 0)
    {
        return 0;
    }

    return st.st_size;
}

} // namespace filesystem
} // namespace Hyperion