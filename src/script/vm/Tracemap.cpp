#include <script/vm/Tracemap.hpp>
#include <core/debug/Debug.hpp>
#include <core/utilities/Format.hpp>

namespace Hyperion {

Script_Tracemap::Script_Tracemap()
    : m_stringmap(nullptr),
      m_linemap(nullptr)
{
}

Script_Tracemap::~Script_Tracemap()
{
    if (m_stringmap)
    {
        delete[] m_stringmap;
    }

    if (m_linemap)
    {
        delete[] m_linemap;
    }
}

void Script_Tracemap::Set(StringmapEntry* stringmap, LinemapEntry* linemap)
{
    Assert(m_stringmap == nullptr);
    Assert(m_linemap == nullptr);

    m_stringmap = stringmap;
    m_linemap = linemap;
}

} // namespace Hyperion
