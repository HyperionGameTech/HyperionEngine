#pragma once

#include <Core/Types.hpp>

namespace Hyperion {

class Tracemap
{
public:
    // read stringmap into memory.
    struct StringmapEntry
    {
        enum : uint8
        {
            ENTRY_TYPE_UNKNOWN,
            ENTRY_TYPE_FILENAME,
            ENTRY_TYPE_SYMBOL_NAME,
            ENTRY_TYPE_MODULE_NAME
        } entryType;

        char data[256];
    };

    // a mapping from binary instruction location, to line number as well as optionally, stringmap index (-1 if not set).
    struct LinemapEntry
    {
        uint64 instructionLocation;
        uint64 lineNum;
        int64 stringmapIndex;
    };

    Tracemap();
    Tracemap(const Tracemap& other) = delete;
    Tracemap& operator=(const Tracemap& other) = delete;
    ~Tracemap();

    void Set(StringmapEntry* stringmap, LinemapEntry* linemap);

private:
    StringmapEntry* m_stringmap;
    LinemapEntry* m_linemap;
};

} // namespace Hyperion
