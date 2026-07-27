/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/
#include <Core/FileSystem/FilePath.hpp>
#include <Core/FileSystem/FsUtil.hpp>

#include <Core/Utilities/StringUtil.hpp>

#include <Core/IO/BufferedByteReader.hpp>

#include <Core/Logging/Logger.hpp>

#include <filesystem>
#include <fstream>

#include <sys/stat.h>

#if HYP_WINDOWS
#include <direct.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // !WIN32_LEAN_AND_MEAN

#include <windows.h>

#undef WIN32_LEAN_AND_MEAN

#elif HYP_ANDROID
#include <android/asset_manager.h>
#elif defined(HYP_UNIX)
#include <unistd.h>
#endif

namespace Hyperion {

CORE_API HYP_DECLARE_LOG_CHANNEL(IO);

#if HYP_ANDROID

CORE_API extern AAssetManager* g_androidAssetManager;

CORE_API bool IsAndroidAssetPath(const FilePath& filepath)
{
    return filepath.FindFirstIndex(AndroidAssetPathPrefix) == 0;
}

static UTF8StringView GetAndroidAssetPath(const FilePath& filepath)
{
    if (!IsAndroidAssetPath(filepath))
    {
        return {};
    }

    // we want the path, plus '/' so we use full size of the char array.
    return filepath.Substr(std::size(AndroidAssetPathPrefix));
}

#endif

namespace filesystem {

struct DirectoryIteratorImpl
{
    FilePath basePath;
    FilePath currentPath;
    bool currentIsDirectory = false;
    bool valid = false;

#if HYP_ANDROID
    // Android specific stuff.
    AAssetDir* assetDir = nullptr;
    bool isAssetPath = false;

    void OpenAssetDir(const FilePath& path)
    {
        isAssetPath = true;

        if (!g_androidAssetManager)
        {
            valid = false;
            return;
        }

        UTF8StringView assetRelative = GetAndroidAssetPath(path);

        // AAssetManager_openDir expects a relative path inside the assets/ tree.
        // An empty string means the root of the assets directory.
        assetDir = AAssetManager_openDir(g_androidAssetManager, String(assetRelative).Data());

        if (!assetDir)
        {
            valid = false;
            return;
        }

        // Position on the first entry
        AdvanceAsset();
    }

    bool AdvanceAsset()
    {
        if (!assetDir)
        {
            valid = false;
            return false;
        }

        const char* name = AAssetDir_getNextFileName(assetDir);

        if (!name)
        {
            valid = false;
            return false;
        }

        currentPath = basePath / FilePath(name);

        // try to open as a directory
        AAssetDir* subDir = AAssetManager_openDir(g_androidAssetManager, String(GetAndroidAssetPath(currentPath)).Data());
        const char* subEntry = subDir ? AAssetDir_getNextFileName(subDir) : nullptr;

        currentIsDirectory = (subEntry != nullptr);

        if (subDir)
        {
            AAssetDir_close(subDir);
        }

        valid = true;
        return true;
    }

    void CloseAsset()
    {
        if (assetDir)
        {
            AAssetDir_close(assetDir);
            assetDir = nullptr;
        }
    }
#endif // HYP_ANDROID

    // Filesystem iteration state
    std::filesystem::directory_iterator fsIter;
    std::filesystem::directory_iterator fsEnd;

    void OpenFs(const FilePath& path)
    {
        std::error_code ec;

        fsIter = std::filesystem::directory_iterator(path.Data(), ec);

        if (ec)
        {
            valid = false;
            return;
        }

        fsEnd = std::filesystem::directory_iterator();

        if (fsIter == fsEnd)
        {
            valid = false;
            return;
        }

        ReadFsEntry();
    }

    bool AdvanceFs()
    {
        std::error_code ec;

        fsIter.increment(ec);

        if (ec || fsIter == fsEnd)
        {
            valid = false;
            return false;
        }

        ReadFsEntry();
        return true;
    }

    void ReadFsEntry()
    {
#ifdef HYP_WINDOWS
        currentPath = FilePath(WideString(fsIter->path().c_str()).ToUtf8());
#else
        currentPath = FilePath(fsIter->path().c_str());
#endif
        currentIsDirectory = fsIter->is_directory();
        valid = true;
    }

    ~DirectoryIteratorImpl()
    {
#if HYP_ANDROID
        CloseAsset();
#endif
    }
};


DirectoryIterator::DirectoryIterator() = default;

DirectoryIterator::DirectoryIterator(Pimpl<DirectoryIteratorImpl>&& impl)
    : m_impl(std::move(impl))
{
}

DirectoryIterator::DirectoryIterator(DirectoryIterator&& other) noexcept = default;
DirectoryIterator& DirectoryIterator::operator=(DirectoryIterator&& other) noexcept = default;
DirectoryIterator::~DirectoryIterator() = default;

bool DirectoryIterator::HasNext() const
{
    return m_impl && m_impl->valid;
}

DirectoryIterator::operator bool() const
{
    return HasNext();
}

FilePath DirectoryIterator::Current() const
{
    if (!HasNext())
    {
        return FilePath();
    }

    return m_impl->currentPath;
}

bool DirectoryIterator::CurrentIsDirectory() const
{
    if (!HasNext())
    {
        return false;
    }

    return m_impl->currentIsDirectory;
}

bool DirectoryIterator::Advance()
{
    if (!m_impl)
    {
        return false;
    }

#if HYP_ANDROID
    if (m_impl->isAssetPath)
    {
        return m_impl->AdvanceAsset();
    }
#endif

    return m_impl->AdvanceFs();
}


DirectoryIterator FilePath::OpenDirectory() const
{
    if (Empty())
    {
        return DirectoryIterator();
    }

    Pimpl<DirectoryIteratorImpl> impl = MakePimpl<DirectoryIteratorImpl>();
    impl->basePath = *this;

#if HYP_ANDROID
    if (IsAndroidAssetPath(*this))
    {
        impl->OpenAssetDir(*this);
        return DirectoryIterator(std::move(impl));
    }
#endif

    impl->OpenFs(*this);
    return DirectoryIterator(std::move(impl));
}

bool FilePath::MkDir() const
{
#if HYP_ANDROID
    if (IsAndroidAssetPath(*this))
        return false;
#endif

    std::error_code ec;

    if (Exists() || std::filesystem::create_directories(Data(), ec))
        return true;

    return false;
}

bool FilePath::CanWrite() const
{
#if HYP_ANDROID
    if (IsAndroidAssetPath(*this))
        return false;
#endif

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
#if HYP_ANDROID
    if (IsAndroidAssetPath(*this))
        return true;
#endif

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

CORE_API String FilePath::GetExtension() const
{
    return StringUtil::GetExtension(*this);
}

HYP_NODISCARD String FilePath::StripExtension() const
{
    return StringUtil::StripExtension(*this);
}

bool FilePath::Remove() const
{
#if HYP_ANDROID
    if (IsAndroidAssetPath(*this))
        return false;
#endif

    std::error_code ec;
    if (!std::filesystem::remove(Data(), ec))
    {
        HYP_LOG(IO, Warning, "Failed to remove file, got error code: {}", ec.value());
        return false;
    }

    return true;
}

bool FilePath::Rename(const FilePath& newPath) const
{
#if HYP_ANDROID
    if (IsAndroidAssetPath(*this))
        return false;
#endif

    std::error_code ec;
    std::filesystem::rename(Data(), newPath.Data(), ec);

    return !ec;
}

bool FilePath::Exists() const
{
#if HYP_ANDROID
    if (IsAndroidAssetPath(*this))
    {
        return true; // assume true; will fail upon read if file doesn't exist
    }
#endif

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
#if HYP_WINDOWS
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;

    if (!GetFileAttributesExA(Data(), GetFileExInfoStandard, &fileInfo))
    {
        return Time(0);
    }

    const FILETIME& ft = fileInfo.ftLastWriteTime;

    // Same conversion as Time::Now(): FILETIME is in 100-nanosecond intervals, divide by 10000 to get milliseconds
    return Time((uint64(ft.dwHighDateTime) << 32 | ft.dwLowDateTime) / 10000);
#else
    struct stat st;

    if (stat(Data(), &st) != 0)
    {
        return Time(0);
    }

    // Time is in ms

#if defined(HYP_MACOS) || defined(HYP_IOS)
    return Time(st.st_mtimespec.tv_sec * 1000 + st.st_mtimespec.tv_nsec / 1000000);
#else
    return Time(st.st_mtime * 1000);
#endif
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

containers::Array<FilePath, DynamicAllocator> FilePath::GetAllFilesInDirectory() const
{
    containers::Array<FilePath, DynamicAllocator> files;

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

containers::Array<FilePath, DynamicAllocator> FilePath::GetSubdirectories() const
{
    containers::Array<FilePath, DynamicAllocator> files;

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
