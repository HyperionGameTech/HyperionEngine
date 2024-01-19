#ifndef HYPERION_TRACEMAP_HPP
#define HYPERION_TRACEMAP_HPP

#include <Types.hpp>

namespace hyperion {
namespace vm {

class Tracemap
{
public:
    // read stringmap into memory.
    struct StringmapEntry
    {
        enum : UInt8
        {
            ENTRY_TYPE_UNKNOWN,
            ENTRY_TYPE_FILENAME,
            ENTRY_TYPE_SYMBOL_NAME,
            ENTRY_TYPE_MODULE_NAME
        } entry_type;

        SChar data[255];
    };

    // a mapping from binary instruction location, to line number as well as optionally, stringmap index (-1 if not set). 
    struct LinemapEntry
    {
        UInt64  instruction_location;
        UInt64  line_num;
        Int64   stringmap_index;
    };

    Tracemap();
    Tracemap(const Tracemap &other) = delete;
    Tracemap &operator=(const Tracemap &other) = delete;
    ~Tracemap();

    void Set(StringmapEntry *stringmap, LinemapEntry *linemap);

private:
    StringmapEntry  *m_stringmap;
    LinemapEntry    *m_linemap;
};

} // namespace vm
} // namespace hyperion

#endif
