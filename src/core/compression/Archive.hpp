/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ARCHIVE_HPP
#define HYPERION_ARCHIVE_HPP

#include <core/Defines.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/LinkedList.hpp>

#include <core/utilities/StringView.hpp>

#include <core/memory/ByteBuffer.hpp>

#include <Types.hpp>

namespace hyperion {
namespace compression {

class Archive;

struct ArchiveResult
{
    enum
    {
        ARCHIVE_OK = 0,
        ARCHIVE_ERR = 1
    } value;

    ANSIStringView message;

    ArchiveResult(decltype(ARCHIVE_OK) value = ARCHIVE_OK, const ANSIStringView &message = "")
        : value(value),
          message(message)
    {
    }

    ArchiveResult(const ArchiveResult &other)                   = default;
    ArchiveResult &operator=(const ArchiveResult &other)        = default;
    ArchiveResult(ArchiveResult &&other) noexcept               = default;
    ArchiveResult &operator=(ArchiveResult &&other) noexcept    = default;

    [[nodiscard]]
    HYP_FORCE_INLINE
    operator int() const
        { return int(value); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool IsOK() const
        { return value == ARCHIVE_OK; }
};

class HYP_API ArchiveBuilder
{
public:
    ArchiveBuilder &Append(ByteBuffer &&buffer);
    ArchiveBuilder &Append(const ByteBuffer &buffer);

    Archive Build() const;

private:
    ByteBuffer  m_uncompressed_buffer;
};

class HYP_API Archive
{
public:
    static bool IsEnabled();

    Archive();
    Archive(ByteBuffer &&compressed_buffer, SizeType uncompressed_size);

    /*! Deleted to prevent unintentional copying of large buffers */
    Archive(const Archive &other)               = delete;

    /*! Deleted to prevent unintentional copying of large buffers */
    Archive &operator=(const Archive &other)    = delete;

    Archive(Archive &&other) noexcept;
    Archive &operator=(Archive &&other) noexcept;

    ~Archive();

    HYP_NODISCARD HYP_FORCE_INLINE
    const ByteBuffer &GetCompressedBuffer() const
        { return m_compressed_buffer; }

    HYP_NODISCARD HYP_FORCE_INLINE
    SizeType GetCompressedSize() const
        { return m_compressed_buffer.Size(); }

    HYP_NODISCARD HYP_FORCE_INLINE
    SizeType GetUncompressedSize() const
        { return m_uncompressed_size; }

    HYP_NODISCARD
    ArchiveResult Decompress(ByteBuffer &out) const;

private:
    ByteBuffer  m_compressed_buffer;
    SizeType    m_uncompressed_size;
};

} // namespace compression

using compression::Archive;
using compression::ArchiveBuilder;
using compression::ArchiveResult;

} // namespace hyperion

#endif