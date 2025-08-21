/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <ui/UIUpdateManager.hpp>
#include <ui/UIObject.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>

#include <core/object/HypClassUtils.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

UIUpdateManager::UIUpdateManager()
{
    // init hash map
    for (const auto& updateType : s_updateOrder)
    {
        m_updateQueues.Insert(updateType, Array<UpdateEntry*>());
    }
}

void UIUpdateManager::RegisterForUpdate(UIObject* uiObject, EnumFlags<UIObjectUpdateType> updateTypes)
{
    HYP_SCOPE;

    if (!uiObject || updateTypes == UIObjectUpdateType::NONE)
    {
        return;
    }

    WeakHandle<UIObject> weakHandle = uiObject->WeakHandleFromThis();
    
    // Check if already registered and merge update types
    auto pendingIt = m_pendingObjects.Find(weakHandle);
    if (pendingIt != m_pendingObjects.End())
    {
        UpdateEntry* existingEntry = pendingIt->second;
        AssertDebug(existingEntry != nullptr, "Pending entry should not be null");

        const EnumFlags<UIObjectUpdateType> addedUpdateTypes = updateTypes & ~existingEntry->updateTypes;

        // Find and update existing entries
        for (auto& kv : m_updateQueues)
        {
            const UIObjectUpdateType updateType = kv.first;

            if (!(addedUpdateTypes & updateType))
            {
                continue;
            }

            Array<UpdateEntry*>& entries = kv.second;

            // add the entry to the existing update type queue
            AssertDebug(!entries.Contains(existingEntry), "Entry should not already be in the queue");

            entries.PushBack(existingEntry);
        }

        // Update the existing entry's update types
        existingEntry->updateTypes |= addedUpdateTypes;

        return;
    }

    // Create entry with all requested update types
    const uint32 entryIndex = m_entryIdGenerator.Next();

    UpdateEntry* newEntry = &*m_entryPool.Emplace(entryIndex);
    *newEntry = {
        .index = int(entryIndex),
        .object = weakHandle,
        .updateTypes = updateTypes,
        .depth = uiObject->GetComputedDepth()
    };

    // Add to pending set
    m_pendingObjects.Insert(weakHandle, newEntry);

    // Add to each individual update queue based on the flags set
    for (UIObjectUpdateType updateType : s_updateOrder)
    {
        if (updateTypes & updateType)
        {
            m_updateQueues[updateType].PushBack(newEntry);
        }
    }
}

void UIUpdateManager::UnregisterFromUpdate(UIObject* uiObject)
{
    HYP_SCOPE;

    if (!uiObject)
    {
        return;
    }

    WeakHandle<UIObject> weakHandle = uiObject->WeakHandleFromThis();
    
    if (!m_pendingObjects.Erase(weakHandle))
    {
        return; // Not registered
    }

    // Remove from all queues
    for (auto& kv : m_updateQueues)
    {
        Array<UpdateEntry*>& entries = kv.second;

        if (entries.Empty())
        {
            continue;
        }

        auto it = entries.FindIf([&weakHandle](const UpdateEntry* entry)
        {
            return entry->object == weakHandle;
        });

        if (it != entries.End())
        {
            UpdateEntry* entry = *it;
            const int entryIndex = entry->index;
            
            entries.Erase(it);

            if (entryIndex != -1)
            {
                m_entryIdGenerator.ReleaseId((uint32)entry->index);
                entry->index = -1;

                m_entryPool.EraseAt(entryIndex);
            }
        }
    }
}

void UIUpdateManager::ProcessUpdates(float delta)
{
    HYP_SCOPE;

    if (m_pendingObjects.Empty())
    {
        return;
    }

    // Process updates in optimal order
    for (const auto& updateType : s_updateOrder)
    {
        ProcessUpdateType(updateType, delta);
    }

    // Clear all processed updates
    Clear();
}

void UIUpdateManager::ProcessUpdateType(UIObjectUpdateType updateType, float delta)
{
    HYP_SCOPE;

    auto it = m_updateQueues.Find(updateType);
    if (it == m_updateQueues.End() || it->second.Empty())
    {
        return;
    }

    // copy the array so we can modify the original while processing
    Array<UpdateEntry*> entries = it->second;
    
    // Sort by depth for optimal processing order (parents before children)
    SortByDepth(entries);

    // Process all objects with this specific update type
    for (const UpdateEntry* entry : entries)
    {
        Handle<UIObject> object = entry->object.Lock();
        if (!object)
        {
            continue; // Object was destroyed
        }

        switch (updateType)
        {
        case UIObjectUpdateType::UPDATE_SIZE:
            object->UpdateSize(false); // Don't cascade to children - they're in the list too
            break;

        case UIObjectUpdateType::UPDATE_POSITION:
            object->UpdatePosition(false);
            break;

        case UIObjectUpdateType::UPDATE_CLAMPED_SIZE:
            object->UpdateClampedSize(false);
            break;

        case UIObjectUpdateType::UPDATE_COMPUTED_VISIBILITY:
            object->UpdateComputedVisibility(false);
            break;

        case UIObjectUpdateType::UPDATE_MATERIAL:
            object->UpdateMaterial(false);
            break;

        case UIObjectUpdateType::UPDATE_MESH_DATA:
            object->UpdateMeshData(false);
            break;

        case UIObjectUpdateType::UPDATE_CUSTOM:
            object->Update_Internal(delta);

            break;

        default:
            HYP_LOG(UI, Warning, "Unhandled update type: {}", EnumToString(updateType));

            break;
        }
    }
}

void UIUpdateManager::SortByDepth(Array<UpdateEntry*>& entries)
{
    HYP_SCOPE;

    std::sort(
        entries.Begin(),
        entries.End(),
        [](const UpdateEntry* a, const UpdateEntry* b)
        {
            return a->depth < b->depth; // Parents (lower depth) first
        });
}

void UIUpdateManager::Clear()
{
    HYP_SCOPE;
    
    m_entryPool.Clear(/* freeMemory */ false);
    m_entryIdGenerator.Reset();
    m_pendingObjects.Clear();

    // don't clear update queues, but remove all entries
    for (auto& kv : m_updateQueues)
    {
        kv.second.Clear();
    }
}

} // namespace hyperion
