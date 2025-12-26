/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/compression/Archive.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#ifdef HYP_ZLIB
#include <zlib.h>
#endif

namespace Hyperion {
namespace compression {

#pragma region ArchiveBuilder

ArchiveBuilder& ArchiveBuilder::Append(ByteBuffer&& buffer)
{
    if (m_uncompressedBuffer.Empty())
    {
        m_uncompressedBuffer = std::move(buffer);

        return *this;
    }

    const SizeType offset = m_uncompressedBuffer.Size();
    m_uncompressedBuffer.SetSize(m_uncompressedBuffer.Size() + buffer.Size());
    m_uncompressedBuffer.Write(buffer.Size(), offset, buffer.Data());

    return *this;
}

ArchiveBuilder& ArchiveBuilder::Append(const ByteBuffer& buffer)
{
    if (m_uncompressedBuffer.Empty())
    {
        m_uncompressedBuffer = buffer;

        return *this;
    }

    const SizeType offset = m_uncompressedBuffer.Size();
    m_uncompressedBuffer.SetSize(m_uncompressedBuffer.Size() + buffer.Size());
    m_uncompressedBuffer.Write(buffer.Size(), offset, buffer.Data());

    return *this;
}

Archive ArchiveBuilder::Build() const
{
    const unsigned long uncompressedSize = m_uncompressedBuffer.Size();
    ByteBuffer compressedBuffer;

#ifdef HYP_ZLIB
    // https://bobobobo.wordpress.com/2008/02/23/how-to-use-zlib/
    unsigned long compressedSize = static_cast<unsigned long>(double(uncompressedSize) * 1.1) + 12;
    compressedBuffer.SetSize(compressedSize);

    const int compressResult = compress(
        compressedBuffer.Data(),
        &compressedSize,
        m_uncompressedBuffer.Data(),
        uncompressedSize);

    Assert(compressResult == Z_OK, "zlib compression failed with error %d", compressResult);

    compressedBuffer.SetSize(compressedSize);
#else
    HYP_THROW("Cannot build Archive - zlib not linked to library");
#endif

    return Archive(std::move(compressedBuffer), uncompressedSize);
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
    : m_uncompressedSize(0)
{
}

Archive::Archive(ByteBuffer&& compressedBuffer, SizeType uncompressedSize)
    : m_compressedBuffer(std::move(compressedBuffer)),
      m_uncompressedSize(uncompressedSize)
{
}

Archive::Archive(Archive&& other) noexcept
    : m_compressedBuffer(std::move(other.m_compressedBuffer)),
      m_uncompressedSize(other.m_uncompressedSize)
{
    other.m_uncompressedSize = 0;
}

Archive& Archive::operator=(Archive&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    m_compressedBuffer = std::move(other.m_compressedBuffer);

    m_uncompressedSize = other.m_uncompressedSize;
    other.m_uncompressedSize = 0;

    return *this;
}

Archive::~Archive() = default;

Result Archive::Decompress(ByteBuffer& outBuffer) const
{
#ifdef HYP_ZLIB
    outBuffer.SetSize(m_uncompressedSize);

    unsigned long compressedSize = m_compressedBuffer.Size();
    unsigned long decompressedSize = m_uncompressedSize;

    const unsigned long compressedSizeBefore = compressedSize;
    const unsigned long decompressedSizeBefore = decompressedSize;

    if (uncompress2(outBuffer.Data(), &decompressedSize, m_compressedBuffer.Data(), &compressedSize) != Z_OK)
    {
        return HYP_MAKE_ERROR(Error, "Failed to decompress: zlib returned an error");
    }

    if (compressedSize != compressedSizeBefore)
    {
        return HYP_MAKE_ERROR(Error, "Compressed data size was incorrect");
    }

    if (decompressedSize != decompressedSizeBefore)
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
} // namespace Hyperion