#pragma once

#include <script/vm/Value.hpp>

#include <core/Types.hpp>

#include <core/memory/MemoryPool.hpp>

namespace hyperion {

class Script_GC
{
    using Pool = MemoryPool<Script_Value>;

public:
    Script_GC();

    Script_GC(const Script_GC& other) = delete;
    Script_GC& operator=(const Script_GC& other) = delete;

    Script_GC(Script_GC&& other) = delete;
    Script_GC& operator=(Script_GC&& other) = delete;

    ~Script_GC();

    void MoveToTrackedMemory(Script_Value& inOutRefValue);

private:
    Pool m_pool;
};

} // namespace hyperion
