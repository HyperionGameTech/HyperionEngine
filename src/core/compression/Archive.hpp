/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ARCHIVE_HPP
#define HYPERION_ARCHIVE_HPP

#include <core/Defines.hpp>

#include <core/utilities/Result.hpp>

#include <core/memory/ByteBuffer.hpp>

#include <Types.hpp>

namespace hyperion {
namespace compression {

class HYP_API Archive
{
public:
    static bool IsEnabled();

    Archive();
    Archive(ByteBuffer&& compressed_buffer, SizeType uncompressed_size);

    /*! Deleted to prevent unintentional copying of large buffers */
    Archive(const Archive& other) = delete;

    /*! Deleted to prevent unintentional copying of large buffers */
    Archive& operator=(const Archive& other) = delete;

    Archive(Archive&& other) noexcept;
    Archive& operator=(Archive&& other) noexcept;

    ~Archive();

    HYP_FORCE_INLINE const ByteBuffer& GetCompressedBuffer() const
    {
        return m_compressed_buffer;
    }

    HYP_FORCE_INLINE SizeType GetCompressedSize() const
    {
        return m_compressed_buffer.Size();
    }

    HYP_FORCE_INLINE SizeType GetUncompressedSize() const
    {
        return m_uncompressed_size;
    }

    Result Decompress(ByteBuffer& out) const;

private:
    ByteBuffer m_compressed_buffer;
    SizeType m_uncompressed_size;
};

class HYP_API ArchiveBuilder
{
public:
    ArchiveBuilder& Append(ByteBuffer&& buffer);
    ArchiveBuilder& Append(const ByteBuffer& buffer);

    Archive Build() const;

private:
    ByteBuffer m_uncompressed_buffer;
};

} // namespace compression

using compression::Archive;
using compression::ArchiveBuilder;

} // namespace hyperion

#endif