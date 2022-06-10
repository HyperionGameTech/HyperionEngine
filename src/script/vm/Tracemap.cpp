#include <script/vm/Tracemap.hpp>

namespace hyperion {
namespace vm {

void Tracemap::Set(StringmapEntry *stringmap, LinemapEntry *linemap)
{
    m_stringmap = stringmap;
    m_linemap = linemap;
}

} // namespace vm
} // namespace hyperion
