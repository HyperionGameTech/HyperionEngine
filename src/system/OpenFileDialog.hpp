/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/utilities/Span.hpp>
#include <core/utilities/Result.hpp>

#include <core/functional/Proc.hpp>

namespace Hyperion {

void ShowOpenFileDialog(
    UTF8StringView title, const FilePath& baseDir, Span<const ANSIStringView> extensions,
    bool allowMultiple, bool allowDirectories,
    Proc<void(TResult<Array<FilePath>>&& result)>&& callback);

} // namespace Hyperion
