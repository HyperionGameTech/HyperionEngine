/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/rhi/RHICommandList.hpp>


namespace hyperion {

RHICommandList::RHICommandList()
{   
}

RHICommandList::~RHICommandList()
{
    for (RHICommandBase *command : m_commands) {
        FreeCommand(command);
    }
}

void RHICommandList::Execute(const CommandBufferRef &cmd)
{
    AssertThrow(cmd != nullptr);

    for (RHICommandBase *command : m_commands) {
        command->Execute(cmd);

        FreeCommand(command);
    }

    m_commands.Clear();
}

void RHICommandList::FreeCommand(RHICommandBase *command)
{
    AssertThrow(command != nullptr);

    RHICommandMemoryPoolBase *pool = command->m_pool_handle.pool;
    AssertThrow(pool != nullptr);

    pool->FreeCommand(command);
}

} // namespace hyperion
