/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/SafeDeleter.hpp>
#include <rendering/RenderGlobalState.hpp>

#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(RemoveTextureFromBindlessStorage)
    : RenderCommand
{
    ID<Texture> id;

    RENDER_COMMAND(RemoveTextureFromBindlessStorage)(ID<Texture> id)
        : id(id)
    {
    }

    virtual ~RENDER_COMMAND(RemoveTextureFromBindlessStorage)() override = default;

    virtual RendererResult operator()() override
    {
        g_render_global_state->BindlessTextures.RemoveResource(id);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region DeletionEntryBase

bool DeletionEntryBase::DecrementCycle()
{
    if (m_cycles_remaining == 0)
    {
        return true;
    }

    --m_cycles_remaining;

    return m_cycles_remaining == 0;
}

bool DeletionEntryBase::PerformDeletion(bool force)
{
    if (force)
    {
        m_cycles_remaining = 0;
    }

    if (m_cycles_remaining != 0)
    {
        return false;
    }

    PerformDeletion_Internal();

    return true;
}

#pragma endregion DeletionEntryBase

#pragma region SafeDeleter

void SafeDeleter::PerformEnqueuedDeletions()
{
    if (int32 num_deletion_entries = m_num_deletion_entries.Get(MemoryOrder::ACQUIRE))
    {
        Array<UniquePtr<DeletionEntryBase>> deletion_entries;

        { // Critical section
            Mutex::Guard guard(m_mutex);

            CollectAllEnqueuedItems(deletion_entries);
        }

        for (auto it = deletion_entries.Begin(); it != deletion_entries.End(); ++it)
        {
            AssertThrow((*it)->PerformDeletion());

            m_num_deletion_entries.Decrement(1, MemoryOrder::RELEASE);
        }
    }
}

void SafeDeleter::ForceDeleteAll()
{
    while (int32 num_deletion_entries = m_num_deletion_entries.Get(MemoryOrder::ACQUIRE))
    {
        Array<UniquePtr<DeletionEntryBase>> deletion_entries;

        { // Critical section
            Mutex::Guard guard(m_mutex);

            CollectAllEnqueuedItems(deletion_entries);
        }

        for (auto it = deletion_entries.Begin(); it != deletion_entries.End();)
        {
            AssertThrow((*it)->PerformDeletion(true /* force */));

            it = deletion_entries.Erase(it);

            m_num_deletion_entries.Decrement(1, MemoryOrder::RELEASE);
        }
    }
}

bool SafeDeleter::CollectAllEnqueuedItems(Array<UniquePtr<DeletionEntryBase>>& out_entries)
{
    for (auto it = m_deletion_entries.Begin(); it != m_deletion_entries.End();)
    {
        auto& entry = *it;

        if (entry->DecrementCycle())
        {
            out_entries.PushBack(std::move(*it));

            it = m_deletion_entries.Erase(it);
        }
        else
        {
            ++it;
        }
    }

    return m_deletion_entries.Empty();
}

#pragma endregion SafeDeleter

} // namespace hyperion