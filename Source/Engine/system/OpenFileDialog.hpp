/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

#include <Core/containers/String.hpp>
#include <Core/containers/Array.hpp>

#include <Core/filesystem/FilePath.hpp>

#include <Core/utilities/Span.hpp>
#include <Core/utilities/Result.hpp>

#include <Core/functional/Proc.hpp>

namespace Hyperion {

void ShowOpenFileDialog(
    UTF8StringView title, const FilePath& baseDir, Span<const ANSIStringView> extensions,
    bool allowMultiple, bool allowDirectories,
    Proc<void(TResult<Array<FilePath>>&& result)>&& callback);

} // namespace Hyperion
