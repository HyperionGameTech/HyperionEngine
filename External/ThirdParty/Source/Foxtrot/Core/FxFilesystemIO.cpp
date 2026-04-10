#include "FxFilesystemIO.hpp"

#include <filesystem>

namespace FxFilesystemIO {

uint64 FileGetLastModified(const char* path)
{
    return std::filesystem::last_write_time(path).time_since_epoch().count();
}

FxPagedArray<std::string> DirList(const char* path, const char* ends_with)
{
    const std::filesystem::directory_iterator& dir_iterator = std::filesystem::directory_iterator(path);

    FxPagedArray<std::string> file_paths(16);

    for (const auto& entry : dir_iterator) {
        const std::string& entry_str = entry.path().filename().string();

        if (ends_with) {
            if (entry_str.ends_with(ends_with)) {
                file_paths.Insert(entry_str);
            }

            continue;
        }

        file_paths.Insert(entry_str);
    }

    return file_paths;
}

std::string DirCurrent() { return std::filesystem::current_path().string(); }

FxPagedArray<std::string> DirListIfHasExtension(const char* path, const std::string& required_extension,
                                                bool return_filename_only)
{
    const std::filesystem::directory_iterator& dir_iterator = std::filesystem::directory_iterator(path);

    FxPagedArray<std::string> file_paths(16);

    for (const auto& entry : dir_iterator) {
        const auto& path_obj = entry.path();

        if (path_obj.has_extension() && path_obj.extension().string() == required_extension) {
            if (return_filename_only) {
                file_paths.Insert(path_obj.stem().string());
            }
            else {
                file_paths.Insert(path_obj.filename().string());
            }
        }
    }

    return file_paths;
}

}; // namespace FxFilesystemIO
