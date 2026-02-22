#pragma once

#include <script/vm/Value.hpp>

#include <Core/Types.hpp>

#include <Core/memory/allocator/SlabAllocator.hpp>

#include <Core/utilities/IdGenerator.hpp>

namespace Hyperion {

class Script_GC
{
public:
    Script_GC();

    Script_GC(const Script_GC& other) = delete;
    Script_GC& operator=(const Script_GC& other) = delete;

    Script_GC(Script_GC&& other) = delete;
    Script_GC& operator=(Script_GC&& other) = delete;

    ~Script_GC();

    void MoveToTrackedMemory(BoxedValue& inOutRefValue);

private:
    SlabAllocator m_allocator;
    IdGenerator m_idGenerator;
};

} // namespace Hyperion
