/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Util/Img/WritePng.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <stb_image_write.h>

namespace Hyperion {

bool WritePng::Write(const FilePath& filepath, uint32 width, uint32 height, uint32 numComponents, const ubyte* bytes)
{
    return stbi_write_png(
               filepath.Data(),
               int(width),
               int(height),
               int(numComponents),
               bytes,
               int(width * numComponents))
        != 0;
}

} // namespace Hyperion
