#include <Core/Functional/Proc.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Utilities/Result.hpp>
#include <Core/Utilities/Span.hpp>

#include <Core/Debug/Debug.hpp>

#include <System/OpenFileDialog.hpp>
#include <System/SaveFileDialog.hpp>

namespace Hyperion {

void ShowOpenFileDialog(
    UTF8StringView title,
    const FilePath& baseDir,
    Span<const ANSIStringView> extensions,
    bool allowMultiple,
    bool allowDirectories,
    Proc<void(TResult<Array<FilePath>>&& result)>&& callback)
{
    HYP_NOT_IMPLEMENTED();

    if (callback)
    {
        callback(HYP_MAKE_ERROR(Error, "File dialog not available on iOS"));
    }
}

void ShowSaveFileDialog(
    UTF8StringView title,
    const FilePath& baseDir,
    Span<const ANSIStringView> extensions,
    Proc<void(TResult<FilePath>&& result)>&& callback)
{
    HYP_NOT_IMPLEMENTED();

    if (callback)
    {
        callback(HYP_MAKE_ERROR(Error, "File dialog not available on iOS"));
    }
}

} // namespace Hyperion
