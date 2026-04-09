/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/containers/String.hpp>
#include <Core/containers/Array.hpp>

#include <Core/filesystem/FilePath.hpp>

#include <Core/utilities/Span.hpp>
#include <Core/utilities/Result.hpp>

#include <Core/functional/Proc.hpp>

namespace Hyperion {

void ShowSaveFileDialog(
    UTF8StringView title,
    const FilePath& baseDir,
    Span<const ANSIStringView> extensions,
    Proc<void(TResult<FilePath>&& result)>&& callback);

} // namespace Hyperion
