#pragma once

#include <Core/Containers/String.hpp>
#include <Core/HashCode.hpp>

namespace Hyperion::DataProcessing {

class CORE_API SourceLocation
{
public:
    static const SourceLocation eof;

    constexpr SourceLocation()
        : SourceLocation(-1, -1)
    {
    }

    // filename is borrowed from the TokenStream being lexed, CompilerError keeps its own copy
    constexpr SourceLocation(int line, int column, const String* filename = nullptr)
        : m_line(line),
          m_column(column),
          m_filename(filename)
    {
    }

    constexpr SourceLocation(const SourceLocation& other) = default;
    constexpr SourceLocation& operator=(const SourceLocation& other) = default;

    int GetLine() const { return m_line; }
    int& GetLine() { return m_line; }

    int GetColumn() const { return m_column; }
    int& GetColumn() { return m_column; }

    const String& GetFileName() const
    {
        if (!m_filename)
        {
            return String::empty;
        }

        return *m_filename;
    }

    bool operator<(const SourceLocation& other) const;
    bool operator==(const SourceLocation& other) const;

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(m_line);
        hc.Add(m_column);
        hc.Add(GetFileName());
        return hc;
    }

private:
    int m_line;
    int m_column;
    const String* m_filename;
};

} // namespace Hyperion::DataProcessing
