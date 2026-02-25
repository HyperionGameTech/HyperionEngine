/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

#include <Core/containers/String.hpp>
#include <Core/containers/Array.hpp>

#include <Core/filesystem/FilePath.hpp>

#include <Core/utilities/Result.hpp>

#include <Core/functional/Proc.hpp>

namespace Hyperion {

void ShowSelectFolderDialog(
    UTF8StringView title,
    const FilePath& baseDir,
    Proc<void(TResult<FilePath>&& result)>&& callback);

} // namespace Hyperion
