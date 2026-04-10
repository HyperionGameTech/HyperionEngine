#pragma once

#include <Core/FxPagedArray.hpp>
//
#include "FxTypes.hpp"

namespace FxFilesystemIO {


uint64 FileGetLastModified(const char* path);

//////////////////////////////
// Directory functions
//////////////////////////////

FxPagedArray<std::string> DirList(const char* path, const char* ends_with = nullptr);
std::string DirCurrent();
FxPagedArray<std::string> DirListIfHasExtension(const char* path, const std::string& required_extension,
                                                bool return_filename_only);


}; // namespace FxFilesystemIO
