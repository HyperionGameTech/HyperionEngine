/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Containers/String.hpp>
#include <Core/Containers/Array.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Utilities/Result.hpp>

#include <Core/Functional/Proc.hpp>

namespace Hyperion {

void ShowSelectFolderDialog(
    UTF8StringView title,
    const FilePath& baseDir,
    Proc<void(TResult<FilePath>&& result)>&& callback);

} // namespace Hyperion
