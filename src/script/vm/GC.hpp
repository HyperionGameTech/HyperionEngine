#pragma once

#include <script/vm/Value.hpp>

#include <core/Types.hpp>

#include <core/memory/pool/Pool.hpp>

#include <core/utilities/IdGenerator.hpp>

namespace hyperion {

class Script_GC
{
public:
    Script_GC();

    Script_GC(const Script_GC& other) = delete;
    Script_GC& operator=(const Script_GC& other) = delete;

    Script_GC(Script_GC&& other) = delete;
    Script_GC& operator=(Script_GC&& other) = delete;

    ~Script_GC();

    void MoveToTrackedMemory(HypData& inOutRefValue);

private:
    Pool m_pool;
    IdGenerator m_idGenerator;
};

} // namespace hyperion
