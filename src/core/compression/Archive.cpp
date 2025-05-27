/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/compression/Archive.hpp>

#ifdef HYP_ZLIB
    #include <zlib.h>
#endif

namespace hyperion {
namespace compression {

#pragma region ArchiveBuilder

ArchiveBuilder& ArchiveBuilder::Append(ByteBuffer&& buffer)
{
    if (m_uncompressed_buffer.Empty())
    {
        m_uncompressed_buffer = std::move(buffer);

        return *this;
    }

    const SizeType offset = m_uncompressed_buffer.Size();
    m_uncompressed_buffer.SetSize(m_uncompressed_buffer.Size() + buffer.Size());
    m_uncompressed_buffer.Write(buffer.Size(), offset, buffer.Data());

    return *this;
}

ArchiveBuilder& ArchiveBuilder::Append(const ByteBuffer& buffer)
{
    if (m_uncompressed_buffer.Empty())
    {
        m_uncompressed_buffer = buffer;

        return *this;
    }

    const SizeType offset = m_uncompressed_buffer.Size();
    m_uncompressed_buffer.SetSize(m_uncompressed_buffer.Size() + buffer.Size());
    m_uncompressed_buffer.Write(buffer.Size(), offset, buffer.Data());

    return *this;
}

Archive ArchiveBuilder::Build() const
{
    const unsigned long uncompressed_size = m_uncompressed_buffer.Size();
    ByteBuffer compressed_buffer;

#ifdef HYP_ZLIB
    // https://bobobobo.wordpress.com/2008/02/23/how-to-use-zlib/
    unsigned long compressed_size = static_cast<unsigned long>(double(uncompressed_size) * 1.1) + 12;
    compressed_buffer.SetSize(compressed_size);

    const int compress_result = compress(
        compressed_buffer.Data(),
        &compressed_size,
        m_uncompressed_buffer.Data(),
        uncompressed_size);

    AssertThrowMsg(compress_result == Z_OK, "zlib compression failed with error %d", compress_result);

    compressed_buffer.SetSize(compressed_size);
#else
    HYP_THROW("Cannot build Archive - zlib not linked to library");
#endif

    return Archive(std::move(compressed_buffer), uncompressed_size);
}

#pragma endregion ArchiveBuilder

#pragma region Archive

bool Archive::IsEnabled()
{
#ifdef HYP_ZLIB
    return true;
#else
    return false;
#endif
}

Archive::Archive()
    : m_uncompressed_size(0)
{
}

Archive::Archive(ByteBuffer&& compressed_buffer, SizeType uncompressed_size)
    : m_compressed_buffer(std::move(compressed_buffer)),
      m_uncompressed_size(uncompressed_size)
{
}

Archive::Archive(Archive&& other) noexcept
    : m_compressed_buffer(std::move(other.m_compressed_buffer)),
      m_uncompressed_size(other.m_uncompressed_size)
{
    other.m_uncompressed_size = 0;
}

Archive& Archive::operator=(Archive&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    m_compressed_buffer = std::move(other.m_compressed_buffer);

    m_uncompressed_size = other.m_uncompressed_size;
    other.m_uncompressed_size = 0;

    return *this;
}

Archive::~Archive() = default;

Result Archive::Decompress(ByteBuffer& out_buffer) const
{
#ifdef HYP_ZLIB
    out_buffer.SetSize(m_uncompressed_size);

    unsigned long compressed_size = m_compressed_buffer.Size();
    unsigned long decompressed_size = m_uncompressed_size;

    const unsigned long compressed_size_before = compressed_size;
    const unsigned long decompressed_size_before = decompressed_size;

    if (uncompress2(out_buffer.Data(), &decompressed_size, m_compressed_buffer.Data(), &compressed_size) != Z_OK)
    {
        return HYP_MAKE_ERROR(Error, "Failed to decompress: zlib returned an error");
    }

    if (compressed_size != compressed_size_before)
    {
        return HYP_MAKE_ERROR(Error, "Compressed data size was incorrect");
    }

    if (decompressed_size != decompressed_size_before)
    {
        return HYP_MAKE_ERROR(Error, "Decompressed data size was incorrect");
    }

    return {};
#else
    return HYP_MAKE_ERROR(Error, "zlib not linked, cannot decompress");
#endif
}

#pragma endregion Archive

} // namespace compression
} // namespace hyperion