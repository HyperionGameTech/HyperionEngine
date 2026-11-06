#pragma once

#include <Core/Types.hpp>
#include <Core/Unicode.hpp>

namespace Hyperion::DataProcessing {

class SourceStream
{
public:
    SourceStream(const char* data, size_t size);
    SourceStream(const SourceStream& other) = default;

    size_t GetPosition() const { return m_position; }
    bool HasNext() const { return m_position < m_size; }

    utf::Char32 Peek() const;
    utf::Char32 Next();
    utf::Char32 Next(int& posChange);
    void GoBack(int n = 1);

private:
    int DecodeAt(size_t position, utf::Char32& outChar) const;

    const char* m_data;
    size_t m_size;
    size_t m_position;
};

} // namespace Hyperion::DataProcessing
