#ifndef TRACEMAP_HPP
#define TRACEMAP_HPP

#include <cstdint>

namespace hyperion {
namespace vm {

class Tracemap {
public:
    // read stringmap into memory.
    struct StringmapEntry {
        enum : char {
            ENTRY_TYPE_UNKNOWN,
            ENTRY_TYPE_FILENAME,
            ENTRY_TYPE_SYMBOL_NAME,
            ENTRY_TYPE_MODULE_NAME
        } entry_type;
        char data[255];
    };

    // a mapping from binary instruction location, to line number as well as optionally, stringmap index (-1 if not set). 
    struct LinemapEntry {
        uint64_t instruction_location;
        uint64_t line_num;
        int64_t stringmap_index;
    };

    void Set(StringmapEntry *stringmap, LinemapEntry *linemap);

private:
    StringmapEntry *m_stringmap;
    LinemapEntry *m_linemap;
};

} // namespace vm
} // namespace hyperion

#endif
