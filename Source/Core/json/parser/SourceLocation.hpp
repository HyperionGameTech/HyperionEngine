/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/containers/String.hpp>
#include <Core/HashCode.hpp>

namespace Hyperion::JSON {

class SourceLocation
{
public:
    static const SourceLocation eof;

    SourceLocation();
    SourceLocation(int line, int column, const String& filename);
    SourceLocation(const SourceLocation& other);

    int GetLine() const
    {
        return m_line;
    }

    int& GetLine()
    {
        return m_line;
    }

    int GetColumn() const
    {
        return m_column;
    }

    int& GetColumn()
    {
        return m_column;
    }

    const String& GetFileName() const
    {
        return m_filename;
    }

    void SetFileName(const String& filename)
    {
        m_filename = filename;
    }

    bool operator<(const SourceLocation& other) const;
    bool operator==(const SourceLocation& other) const;

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(m_line);
        hc.Add(m_column);
        hc.Add(m_filename);

        return hc;
    }

private:
    int m_line;
    int m_column;
    String m_filename;
};

} // namespace Hyperion::JSON
