#include <util/fs/FsUtil.hpp>
#include <util/Defines.hpp>
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
    return ::_mkdir(path.c_str());
#else
    return ::mkdir(path.c_str(), 0755);
#endif
}

std::string FileSystem::CurrentPath()
{
    return std::filesystem::current_path().string();
}

std::string FileSystem::RelativePath(const std::string &path, const std::string &base)
{
    return std::filesystem::proximate(path, base).string();
}

Int FilePath::MkDir() const
{
#if HYP_WINDOWS
    return ::_mkdir(Data());
#else
    return ::mkdir(Data(), 0755);
#endif
}

bool FilePath::Exists() const
{
    struct stat st;
    
    return stat(Data(), &st) == 0;
}

bool FilePath::IsDirectory() const
{
    struct stat st;
    
    if (stat(Data(), &st) == 0) {
        return st.st_mode & S_IFDIR;
    }

    return false;
}

UInt64 FilePath::LastModifiedTimestamp() const
{
    struct stat st;
    
    if (stat(Data(), &st) != 0) {
        return 0;
    }

#if HYP_MACOS
    return st.st_mtimespec.tv_sec;
#else
    return st.st_mtime;
#endif
}

bool FilePath::Open(BufferedReader<HYP_READER_DEFAULT_BUFFER_SIZE> &out) const
{
    if (!Exists()) {
        return false;
    }

    out = BufferedReader<HYP_READER_DEFAULT_BUFFER_SIZE>(*this);

    return true;
}

} // namespace hyperion