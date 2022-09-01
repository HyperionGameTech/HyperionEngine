#ifndef SOURCE_LOCATION_HPP
#define SOURCE_LOCATION_HPP

#include <string>

namespace hyperion {

class SourceLocation {
public:
    static const SourceLocation eof;

public:
    SourceLocation();
    SourceLocation(int line, int column, const std::string &filename);
    SourceLocation(const SourceLocation &other);

    int GetLine() const { return m_line; }
    int &GetLine() { return m_line; }
    int GetColumn() const { return m_column; }
    int &GetColumn() { return m_column; }
    const std::string &GetFileName() const { return m_filename; }
    void SetFileName(const std::string &filename) { m_filename = filename; }

    bool operator<(const SourceLocation &other) const;
    bool operator==(const SourceLocation &other) const;

private:
    int m_line;
    int m_column;
    std::string m_filename;
};

} // namespace hyperion

#endif
