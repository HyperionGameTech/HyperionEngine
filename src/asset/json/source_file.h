#ifndef HYPERION_JSON_SOURCE_FILE_H
#define HYPERION_JSON_SOURCE_FILE_H

#include <string>
#include <cstddef>

namespace hyperion {
namespace json {

class SourceFile {
public:
    SourceFile(const std::string &filepath, size_t size);
    SourceFile(const SourceFile &other) = delete;
    ~SourceFile();

    SourceFile &operator=(const SourceFile &other) = delete;

    // input into buffer
    SourceFile &operator>>(const std::string &str);

    inline const std::string &GetFilePath() const { return m_filepath; }
    inline char *GetBuffer() const { return m_buffer; }
    inline size_t GetSize() const { return m_size; }
    inline void SetSize(size_t size) { m_size = size; }

    void ReadIntoBuffer(const char *data, size_t size);

private:
    std::string m_filepath;
    char *m_buffer;
    size_t m_position;
    size_t m_size;
};

} // namespace json
} // namespace hyperion

#endif