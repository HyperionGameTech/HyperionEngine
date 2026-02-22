#pragma once

#include <script/SourceFile.hpp>
#include <Core/Unicode.hpp>

#include <cstddef>

namespace Hyperion {

class SourceStream
{
public:
    SourceStream(SourceFile* file);
    SourceStream(const SourceStream& other);

    SourceFile* GetFile() const
    {
        return m_file;
    }
    SizeType GetPosition() const
    {
        return m_position;
    }
    bool HasNext() const
    {
        return m_position < m_file->GetSize();
    }
    utf::Char32 Peek() const;
    utf::Char32 Next();
    utf::Char32 Next(int& posChange);
    void GoBack(int n = 1);
    void Read(char* ptr, SizeType numBytes);

private:
    SourceFile* m_file;
    SizeType m_position;
};

} // namespace Hyperion
