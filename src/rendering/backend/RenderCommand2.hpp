/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDER_COMMAND2_HPP
#define HYPERION_BACKEND_RENDER_COMMAND2_HPP

#include <rendering/backend/RenderCommand.hpp>

#include <core/memory/MemoryPool.hpp>

#include <core/utilities/ValueStorage.hpp>

#include <core/containers/Array.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/AtomicVar.hpp>

namespace hyperion {
namespace renderer {

class RenderCommandMemoryPool : public MemoryPool<ValueStorageArray<ubyte, 1024 * 1024 * 16, 1>>
{
public:
    RenderCommandMemoryPool()
        : m_offset(0)
    {
    }

    void* Allocate(SizeType size, SizeType alignment)
    {
    }

private:
    AtomicVar<uint32> m_offset;
};

class RenderCommandList2
{
    static constexpr uint32 block_size = 1024 * 1024;

public:
private:
    MemoryPool<ValueStorageArray<ubyte, block_size, 1>> m_pool;
};

} // namespace renderer
} // namespace hyperion

#endif