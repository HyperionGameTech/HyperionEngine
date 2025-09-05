#include <script/vm/Tracemap.hpp>
#include <core/debug/Debug.hpp>
#include <core/utilities/Format.hpp>

namespace hyperion {

Tracemap::Tracemap()
    : m_stringmap(nullptr),
      m_linemap(nullptr)
{
}

Tracemap::~Tracemap()
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

void Tracemap::Set(StringmapEntry* stringmap, LinemapEntry* linemap)
{
    Assert(m_stringmap == nullptr);
    Assert(m_linemap == nullptr);

    m_stringmap = stringmap;
    m_linemap = linemap;
}

} // namespace hyperion
