/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

namespace Hyperion {

namespace filesystem {
class FilePath;
} // namespace filesystem

using filesystem::FilePath;

class WritePng
{
public:
    ENGINE_API static bool Write(
        const FilePath& filepath,
        uint32 width,
        uint32 height,
        uint32 numComponents,
        const ubyte* bytes);
};

} // namespace Hyperion
