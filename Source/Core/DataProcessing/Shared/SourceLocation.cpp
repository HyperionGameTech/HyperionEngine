#include <Core/DataProcessing/Shared/SourceLocation.hpp>

namespace Hyperion::DataProcessing {

const SourceLocation SourceLocation::eof(-1, -1);

bool SourceLocation::operator<(const SourceLocation& other) const
{
    if (GetFileName() == other.GetFileName())
    {
        if (m_line == other.m_line)
        {
            return m_column < other.m_column;
        }

        return m_line < other.m_line;
    }

    return GetFileName() < other.GetFileName();
}

bool SourceLocation::operator==(const SourceLocation& other) const
{
    return m_line == other.m_line
        && m_column == other.m_column
        && GetFileName() == other.GetFileName();
}

} // namespace Hyperion::DataProcessing
