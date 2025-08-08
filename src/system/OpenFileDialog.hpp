/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/utilities/Span.hpp>

#include <core/utilities/Result.hpp>

namespace hyperion {

void ShowOpenFileDialog(
    UTF8StringView title, const FilePath& baseDir, Span<const ANSIStringView> extensions,
    bool allowMultiple, bool allowDirectories,
    void (*callback)(TResult<Array<FilePath>>&& result));

} // namespace hyperion
