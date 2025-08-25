#pragma once

#include <script/vm/Value.hpp>

#include <core/Types.hpp>

#include <core/memory/MemoryPool.hpp>

namespace hyperion {
namespace vm {

class GC
{
    using Pool = MemoryPool<Value>;

public:
    GC();

    GC(const GC& other) = delete;
    GC& operator=(const GC& other) = delete;

    GC(GC&& other) = delete;
    GC& operator=(GC&& other) = delete;

    ~GC();

    // returns the new pointer in tracked memory
    Value* MoveToTrackedMemory(Value&& value);

private:
    Pool m_pool;
};

} // namespace vm
} // namespace hyperion
