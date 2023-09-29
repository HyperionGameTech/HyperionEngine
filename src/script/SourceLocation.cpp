#include <script/SourceLocation.hpp>

namespace hyperion {

const SourceLocation SourceLocation::eof(-1, -1, "<eof>");

SourceLocation::SourceLocation() : SourceLocation(SourceLocation::eof)
{
}

SourceLocation::SourceLocation(int line, int column,
    const String &filename)
    : m_line(line),
      m_column(column),
      m_filename(filename)
{
}

SourceLocation::SourceLocation(const SourceLocation &other)
    : m_line(other.m_line),
      m_column(other.m_column),
      m_filename(other.m_filename)
{
}

bool SourceLocation::operator<(const SourceLocation &other) const
{
    if (m_filename == other.m_filename) {
        if (m_line == other.m_line) {
            return m_column < other.m_column;
        }
        return m_line < other.m_line;
    }
    return m_filename < other.m_filename;
}

bool SourceLocation::operator==(const SourceLocation &other) const
{
    return m_line == other.m_line &&
        m_column == other.m_column &&
        m_filename == other.m_filename &&
        m_line == other.m_line;
}

} // namespace hyperion
