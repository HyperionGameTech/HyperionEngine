/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

#include <Core/utilities/Result.hpp>

#include <Core/memory/ByteBuffer.hpp>

#include <Core/Types.hpp>

namespace Hyperion {
namespace compression {

class HYP_API Archive
{
public:
    static bool IsEnabled();

    Archive();
    Archive(ByteBuffer&& compressedBuffer, SizeType uncompressedSize);

    /*! Deleted to prevent unintentional copying of large buffers */
    Archive(const Archive& other) = delete;

    /*! Deleted to prevent unintentional copying of large buffers */
    Archive& operator=(const Archive& other) = delete;

    Archive(Archive&& other) noexcept;
    Archive& operator=(Archive&& other) noexcept;

    ~Archive();

    HYP_FORCE_INLINE const ByteBuffer& GetCompressedBuffer() const
    {
        return m_compressedBuffer;
    }

    HYP_FORCE_INLINE SizeType GetCompressedSize() const
    {
        return m_compressedBuffer.Size();
    }

    HYP_FORCE_INLINE SizeType GetUncompressedSize() const
    {
        return m_uncompressedSize;
    }

    Result Decompress(ByteBuffer& out) const;

private:
    ByteBuffer m_compressedBuffer;
    SizeType m_uncompressedSize;
};

class HYP_API ArchiveBuilder
{
public:
    ArchiveBuilder& Append(ByteBuffer&& buffer);
    ArchiveBuilder& Append(const ByteBuffer& buffer);

    Archive Build() const;

private:
    ByteBuffer m_uncompressedBuffer;
};

} // namespace compression

using compression::Archive;
using compression::ArchiveBuilder;

} // namespace Hyperion
