#ifndef WRITE_BITMAP_HPP
#define WRITE_BITMAP_HPP

#include <core/Defines.hpp>

// just a quick BMP writer for testing
// https://stackoverflow.com/a/47785639/8320593

namespace hyperion {

class WriteBitmap
{
public:
    HYP_API static bool Write(
        const char *path,
        int width,
        int height,
        unsigned char *bytes
    );
};

} // namespace hyperion

#endif
