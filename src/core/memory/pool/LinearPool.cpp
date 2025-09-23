/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <core/memory/pool/LinearPool.hpp>
#include <core/memory/ByteBuffer.hpp>
#include <core/memory/Memory.hpp>

#include <core/math/MathUtil.hpp>

#include <core/utilities/ByteUtil.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

struct AllocHeader
{
    uint32 offset;
    uint32 size;
    void (*moveFn)(void* dst, void* src);
    void (*destructFn)(void*);
};

class LinearPoolImpl
{
public:
    ByteBuffer buffer;
    Array<AllocHeader> headers;
    uint32 offset = 0;

    void* Allocate(SizeType size, SizeType alignment)
    {
        HYP_SCOPE;

        AssertDebug(alignment <= 16, "LinearPool only supports alignment up to 16 bytes");

        const uint32 alignedOffset = ByteUtil::AlignAs(offset, alignment);

        if (buffer.Size() < alignedOffset + size)
        {
            ByteBuffer newBuffer;
            newBuffer.SetSize(MathUtil::Max<SizeType>(alignedOffset + size, buffer.Size() * 2));

            // Move existing objects to new buffer
            for (AllocHeader& header : headers)
            {
                if (header.moveFn)
                {
                    header.moveFn(
                        reinterpret_cast<void*>(newBuffer.Data() + header.offset),
                        reinterpret_cast<void*>(buffer.Data() + header.offset)
                    );
                }
                else
                {
                    Memory::MemCpy(
                        newBuffer.Data() + header.offset,
                        buffer.Data() + header.offset,
                        header.size
                    );
                }

                // Destruct old objects
                if (header.destructFn)
                {
                    header.destructFn(reinterpret_cast<void*>(buffer.Data() + header.offset));
                }
            }

            buffer = std::move(newBuffer);
        }

        void* ptr = buffer.Data() + alignedOffset;
        offset = alignedOffset + size;

        return ptr;
    }

    void* AllocateWithDeleter(SizeType size, SizeType alignment, void (*moveFn)(void* dst, void* src), void (*destructFn)(void*))
    {
        HYP_SCOPE;

        AssertDebug(alignment <= 16, "LinearPool only supports alignment up to 16 bytes");

        const uint32 alignedOffset = ByteUtil::AlignAs(offset, alignment);

        if (buffer.Size() < alignedOffset + size)
        {
            ByteBuffer newBuffer;
            newBuffer.SetSize(MathUtil::Max<SizeType>(alignedOffset + size, buffer.Size() * 2));

            // Move existing objects to new buffer
            for (AllocHeader& header : headers)
            {
                if (header.moveFn)
                {
                    header.moveFn(
                        reinterpret_cast<void*>(newBuffer.Data() + header.offset),
                        reinterpret_cast<void*>(buffer.Data() + header.offset)
                    );
                }
                else
                {
                    Memory::MemCpy(
                        newBuffer.Data() + header.offset,
                        buffer.Data() + header.offset,
                        header.size
                    );
                }

                // Destruct old objects
                if (header.destructFn)
                {
                    header.destructFn(reinterpret_cast<void*>(buffer.Data() + header.offset));
                }
            }

            buffer = std::move(newBuffer);
        }

        void* ptr = buffer.Data() + alignedOffset;
        offset = alignedOffset + size;

        AllocHeader& header = headers.EmplaceBack();
        header.offset = alignedOffset;
        header.size = size;
        header.moveFn = moveFn;
        header.destructFn = destructFn;

        return ptr;
    }

    void Reset()
    {
        HYP_SCOPE;

        for (int i = int(headers.Size()) - 1; i >= 0; i--)
        {
            if (headers[i].destructFn)
            {
                headers[i].destructFn(reinterpret_cast<void*>(buffer.Data() + headers[i].offset));
            }
        }

        headers.Clear();
        offset = 0;
    }
};

LinearPool::LinearPool()
    : m_pImpl(MakePimpl<LinearPoolImpl>())
{
}

LinearPool::~LinearPool()
{
    if (m_pImpl)
    {
        m_pImpl->Reset();
    }
}

void* LinearPool::Alloc(SizeType size, SizeType alignment)
{
    return m_pImpl->Allocate(size, alignment);
}

void* LinearPool::AllocWithDeleter(SizeType size, SizeType alignment, void (*destructFn)(void*))
{
    return m_pImpl->AllocateWithDeleter(size, alignment, nullptr, destructFn);
}

void* LinearPool::Alloc(SizeType size, SizeType alignment, void (*moveFn)(void* dst, void* src), void (*destructFn)(void*))
{
    return m_pImpl->AllocateWithDeleter(size, alignment, moveFn, destructFn);
}

void LinearPool::Reset()
{
    m_pImpl->Reset();
}

} // namespace hyperion