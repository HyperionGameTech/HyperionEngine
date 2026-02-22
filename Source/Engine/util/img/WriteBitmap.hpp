#pragma once
#include <Core/Defines.hpp>

// just a quick BMP writer for testing
// https://stackoverflow.com/a/47785639/8320593

namespace Hyperion {

class ByteWriter;

class WriteBitmap
{
public:
    HYP_API static bool Write(
        ByteWriter* byteWriter,
        int width,
        int height,
        unsigned char* bytes);
};

} // namespace Hyperion
