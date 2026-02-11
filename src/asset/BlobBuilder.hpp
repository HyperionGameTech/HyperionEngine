/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>

#include <core/memory/ByteBuffer.hpp>

#include <core/utilities/Result.hpp>

#include <core/io/ByteWriter.hpp>

#include <core/Util.hpp>

#include <asset/BlobStorageStructs.hpp>

namespace Hyperion {

template <BlobSerializable T>
class TBlobBuilder
{
public:
    HYP_NODISCARD TResult<BlobResourceKey> Serialize(ByteWriter* writer, const T* inPtr)
    {
        const uint64 chunkStart = AllocateChunk(writer);

        Result result = T::Serialize(writer, inPtr);

        if (result.HasError())
        {
            return { result.GetError() };
        }

        SizeType payloadSize;
        UpdatePayloadSize(writer, chunkStart, payloadSize);

        return BlobResourceKey { chunkStart, payloadSize };
    }

private:
    HYP_NODISCARD uint64 AllocateChunk(ByteWriter* writer)
    {
        SizeType payloadSize = sizeof(T);
        SizeType totalSize = sizeof(BlobChunkHeader) + payloadSize;
        
        SizeType currentSize = writer->Position();
        SizeType padding = (16 - (currentSize % 16)) % 16;
        SizeType chunkStart = currentSize + padding;
        
        BlobChunkHeader header {};
        header.version = T::Version;
        header.payloadSize = payloadSize;
        
        static_assert(sizeof(T::Header) >= 4, "T::Header must be at least 4 characters long");
        Memory::StrCpy(header.magic, T::Header, 4);

        writer->Write<BlobChunkHeader>(header);
        
        return chunkStart;
    }

    void UpdatePayloadSize(ByteWriter* writer, uint64 chunkStart, SizeType& outPayloadSize)
    {
        uint64 pos = writer->Position();
        Assert(chunkStart < pos);

        const uint64 dist = chunkStart - pos;
        const uint64 payloadSize = dist - sizeof(BlobChunkHeader);

        const SizeType head = writer->Position();

        writer->Seek(chunkStart + offsetof(BlobChunkHeader, payloadSize));
        writer->Write<uint64>(payloadSize);
        writer->Seek(head);

        outPayloadSize = payloadSize;
    }

};

} // namespace Hyperion
