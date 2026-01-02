/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/utilities/Span.hpp>
#include <core/utilities/Result.hpp>

#include <core/functional/Proc.hpp>

namespace Hyperion {

void ShowSaveFileDialog(
    UTF8StringView title,
    const FilePath& baseDir,
    Span<const ANSIStringView> extensions,
    Proc<void(TResult<FilePath>&& result)>&& callback);

} // namespace Hyperion
