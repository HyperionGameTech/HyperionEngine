/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>

#include <core/memory/ByteBuffer.hpp>

#include <core/utilities/Result.hpp>

#include <core/io/ByteWriter.hpp>

#include <core/Util.hpp>

#include <core/utilities/ByteUtil.hpp>

#include <asset/BlobStorageStructs.hpp>

namespace Hyperion {

template <BlobSerializable T, SizeType Alignment = alignof(T)>
class TInlineBlobBuilder
{
    static_assert(Alignment <= 16);

    struct Attachment
    {
        SizeType ptrOffset;
        SizeType alignment;
        SizeType sizeBytes;
        const void* data;
        SizeType finalCalculatedOffset = 0;
    };

    static constexpr uint32 MaxAttachments = 8;

    Attachment attachments[MaxAttachments];
    uint32 attachmentCount = 0;

    const T* objBase;

public:
    explicit TInlineBlobBuilder(const T* objBase)
        : objBase(objBase)
    {
        Assert(objBase != nullptr);
    }

    template <class ElementType>
    TInlineBlobBuilder& Append(SizeType memberOffset, Span<const ElementType> span)
    {
        Assert(attachmentCount < MaxAttachments);

        attachments[attachmentCount++] = {
            memberOffset,
            alignof(ElementType),
            span.Size() * sizeof(ElementType),
            span.Data()
        };

        return *this;
    }

    T* Build(BlobHeader& outHeader)
    {
        outHeader = {};

        SizeType totalSize = sizeof(T);
        
        for (uint32 i = 0; i < attachmentCount; i++)
        {
            totalSize = ByteUtil::AlignAs(totalSize, attachments[i].alignment);
            attachments[i].finalCalculatedOffset = totalSize;
            
            totalSize += attachments[i].sizeBytes;
        }

        T* ptr = (T*)HYP_ALLOC_ALIGNED(totalSize, Alignment);
        if (!ptr)
        {
            return nullptr;
        }

        uint8* basePtr = reinterpret_cast<uint8*>(ptr);

        // copy the struct data to the base ptr before
        // filling out the attachment data
        Memory::Copy(basePtr, objBase, sizeof(T));

        for (uint32 i = 0; i < attachmentCount; i++)
        {
            const auto& att = attachments[i];

            if (att.sizeBytes > 0 && att.data != nullptr)
            {
                Memory::Copy(basePtr + att.finalCalculatedOffset, att.data, att.sizeBytes);
            }

            int32 relativeJump = int32(att.finalCalculatedOffset - att.ptrOffset);

            Memory::Copy(basePtr + att.ptrOffset, &relativeJump, sizeof(int32));
        }
        
        static_assert(sizeof(outHeader.magic) <= sizeof(T::Header) && std::is_array_v<decltype(T::Header)>);

        Memory::Copy(outHeader.magic, T::Header, sizeof(outHeader.magic));

        outHeader.version = uint8(T::Version);
        outHeader.payloadOffset = uint16(ByteUtil::AlignAs(sizeof(BlobHeader), Alignment) - sizeof(BlobHeader));
        outHeader.payloadSize = uint64(totalSize);

        return ptr;
    }
};

} // namespace Hyperion
