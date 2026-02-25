/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#ifndef HYPERION_CODEGEN_SOURCE_STREAM_HPP
#define HYPERION_CODEGEN_SOURCE_STREAM_HPP

#include <parser/SourceFile.hpp>
#include <Core/Unicode.hpp>

namespace Hyperion::CodeGen {

class SourceStream
{
public:
    SourceStream(const SourceFile* file);
    SourceStream(const SourceStream& other);

    HYP_FORCE_INLINE const SourceFile* GetFile() const
    {
        return m_file;
    }

    HYP_FORCE_INLINE SizeType GetPosition() const
    {
        return m_position;
    }

    HYP_FORCE_INLINE bool HasNext() const
    {
        return m_position < m_file->GetSize();
    }

    utf::Char32 Peek() const;
    utf::Char32 Next();
    utf::Char32 Next(int& posChange);
    void GoBack(int n = 1);
    void Read(char* ptr, SizeType numBytes);

private:
    const SourceFile* m_file;
    SizeType m_position;
};

} // namespace Hyperion::CodeGen

#endif
