/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <ui/UIUpdateManager.hpp>
#include <ui/UIObject.hpp>
#include <core/profiling/ProfileScope.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

void UIUpdateManager::RegisterForUpdate(UIObject* uiObject, EnumFlags<UIObjectUpdateType> updateTypes)
{
    HYP_SCOPE;

    if (!uiObject || updateTypes == UIObjectUpdateType::NONE)
    {
        return;
    }

    WeakHandle<UIObject> weakHandle = uiObject->WeakHandleFromThis();
    
    // Check if already registered
    if (m_pendingObjects.Contains(weakHandle))
    {
        // Update existing entry with new update types
        for (auto& kv : m_updateQueues)
        {
            EnumFlags<UIObjectUpdateType> updateType = kv.first;
            Array<UpdateEntry>& entries = kv.second;

            for (UpdateEntry& entry : entries)
            {
                if (entry.object == weakHandle)
                {
                    entry.updateTypes |= updateTypes;

                    return;
                }
            }
        }
    }

    // Add to pending set
    m_pendingObjects.Insert(weakHandle);

    // Add to appropriate queues based on update types
    UpdateEntry newEntry {
        .object = weakHandle,
        .updateTypes = updateTypes,
        .depth = uiObject->GetComputedDepth()
    };

    // Group by the primary update type for efficient batch processing
    EnumFlags<UIObjectUpdateType> primaryType = UIObjectUpdateType::NONE;
    for (const auto& orderType : s_updateOrder)
    {
        if (updateTypes & orderType)
        {
            primaryType = orderType;
            break;
        }
    }

    if (primaryType != UIObjectUpdateType::NONE)
    {
        m_updateQueues[primaryType].PushBack(newEntry);
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
        Array<UpdateEntry>& entries = kv.second;

        if (entries.Empty())
        {
            continue;
        }

        auto it = entries.FindIf([&weakHandle](const UpdateEntry& entry)
        {
            return entry.object == weakHandle;
        });

        if (it != entries.End())
        {
            entries.Erase(it);
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

void UIUpdateManager::ProcessUpdateType(EnumFlags<UIObjectUpdateType> updateType, float delta)
{
    HYP_SCOPE;

    auto it = m_updateQueues.Find(updateType);
    if (it == m_updateQueues.End() || it->second.Empty())
    {
        return;
    }

    Array<UpdateEntry>& entries = it->second;
    
    // Sort by depth for optimal processing order (parents before children for size/position)
    if (updateType & (UIObjectUpdateType::UPDATE_SIZE | UIObjectUpdateType::UPDATE_POSITION))
    {
        SortByDepth(entries);
    }

    // Process all objects with this update type
    for (const UpdateEntry& entry : entries)
    {
        Handle<UIObject> object = entry.object.Lock();
        if (!object)
        {
            continue; // Object was destroyed
        }

        // Apply the specific updates
        if (entry.updateTypes & UIObjectUpdateType::UPDATE_SIZE)
        {
            object->UpdateSize(false); // Don't cascade to children - they're in the list too
        }
        
        if (entry.updateTypes & UIObjectUpdateType::UPDATE_POSITION)
        {
            object->UpdatePosition(false);
        }
        
        if (entry.updateTypes & UIObjectUpdateType::UPDATE_CLAMPED_SIZE)
        {
            object->UpdateClampedSize(false);
        }
        
        if (entry.updateTypes & UIObjectUpdateType::UPDATE_COMPUTED_VISIBILITY)
        {
            object->UpdateComputedVisibility(false);
        }
        
        if (entry.updateTypes & UIObjectUpdateType::UPDATE_MATERIAL)
        {
            object->UpdateMaterial(false);
        }
        
        if (entry.updateTypes & UIObjectUpdateType::UPDATE_MESH_DATA)
        {
            object->UpdateMeshData(false);
        }
        
        if (entry.updateTypes & UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA)
        {
            object->UpdateComputedTextSize();
        }
    }
}

void UIUpdateManager::SortByDepth(Array<UpdateEntry>& entries)
{
    HYP_SCOPE;

    std::sort(entries.Begin(), entries.End(), [](const UpdateEntry& a, const UpdateEntry& b) {
        return a.depth < b.depth; // Parents (lower depth) first
    });
}

void UIUpdateManager::Clear()
{
    HYP_SCOPE;

    m_pendingObjects.Clear();
    m_updateQueues.Clear();
}

} // namespace hyperion