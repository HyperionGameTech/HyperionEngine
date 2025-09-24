/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <core/memory/pool/LinearPool.hpp>
#include <core/memory/ByteBuffer.hpp>
#include <core/memory/Memory.hpp>

#include <core/math/MathUtil.hpp>

#include <core/utilities/ByteUtil.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {
namespace memory {

struct DeleterInfo
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
    Array<DeleterInfo> deleters;
    uint32 offset = 0;

    void ReallocateBuffer(SizeType newSize)
    {
        ByteBuffer newBuffer;
        newBuffer.SetSize(MathUtil::Max<SizeType>(newSize, buffer.Size() * 2));

        Memory::MemCpy(newBuffer.Data(), buffer.Data(), buffer.Size());

        // invoke move constructor into new buffer since address would have changed,
        // destruct the old objects
        for (DeleterInfo& deleter : deleters)
        {
            if (deleter.moveFn)
            {
                deleter.moveFn(
                    reinterpret_cast<void*>(newBuffer.Data() + deleter.offset),
                    reinterpret_cast<void*>(buffer.Data() + deleter.offset));
            }

            // destruct old objects
            if (deleter.destructFn)
            {
                deleter.destructFn(reinterpret_cast<void*>(buffer.Data() + deleter.offset));
            }
        }

        buffer = std::move(newBuffer);
    }

    void* Allocate(SizeType size, SizeType alignment)
    {
        HYP_SCOPE;

        AssertDebug(alignment <= 16, "LinearPool only supports alignment up to 16 bytes");

        const uint32 alignedOffset = ByteUtil::AlignAs(offset, alignment);

        if (buffer.Size() < alignedOffset + size)
        {
            ReallocateBuffer(alignedOffset + size);
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
            ReallocateBuffer(alignedOffset + size);
        }

        void* ptr = buffer.Data() + alignedOffset;
        offset = alignedOffset + size;

        // only track allocation if deleter functions are provided
        if (moveFn != nullptr || destructFn != nullptr)
        {
            DeleterInfo& deleter = deleters.EmplaceBack();
            deleter.offset = alignedOffset;
            deleter.size = size;
            deleter.moveFn = moveFn;
            deleter.destructFn = destructFn;
        }

        return ptr;
    }

    void Reserve(SizeType size)
    {
        HYP_SCOPE;

        if (buffer.Size() < size)
        {
            ReallocateBuffer(size);
        }
    }

    void Reset()
    {
        HYP_SCOPE;

        if (offset == 0)
        {
            // if offset is 0, nothing to do
            return;
        }

        // destruct all tracked objects in reverse order
        for (int i = int(deleters.Size()) - 1; i >= 0; i--)
        {
            if (deleters[i].destructFn)
            {
                deleters[i].destructFn(reinterpret_cast<void*>(buffer.Data() + deleters[i].offset));
            }
        }

        deleters.Clear();
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

void LinearPool::Reserve(SizeType size)
{
    m_pImpl->Reserve(size);
}

void LinearPool::Reset()
{
    m_pImpl->Reset();
}

void* LinearPool::Alloc(SizeType size, SizeType alignment)
{
    return m_pImpl->Allocate(size, alignment);
}

void* LinearPool::AllocWithDeleter(SizeType size, SizeType alignment, void (*moveFn)(void* dst, void* src), void (*destructFn)(void*))
{
    return m_pImpl->AllocateWithDeleter(size, alignment, moveFn, destructFn);
}

} // namespace memory
} // namespace hyperion