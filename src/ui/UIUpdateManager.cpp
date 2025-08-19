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
    
    // Check if already registered and merge update types
    if (m_pendingObjects.Contains(weakHandle))
    {
        // Find and update existing entries
        for (auto& kv : m_updateQueues)
        {
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

    // Create entry with all requested update types
    UpdateEntry newEntry {
        .object = weakHandle,
        .updateTypes = updateTypes,
        .depth = uiObject->GetComputedDepth()
    };

    // Add to each individual update queue based on the flags set
    for (const auto& updateType : s_updateOrder)
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

    // Remove from all individual update type queues
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

    // Process each update type in optimal order
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

    Array<UpdateEntry>& entries = it->second;
    
    // Sort by depth for optimal processing order (parents before children for size/position)
    if (updateType == UIObjectUpdateType::UPDATE_SIZE || updateType == UIObjectUpdateType::UPDATE_POSITION)
    {
        SortByDepth(entries);
    }

    // Process all objects with this specific update type
    for (const UpdateEntry& entry : entries)
    {
        Handle<UIObject> object = entry.object.Lock();
        if (!object)
        {
            continue; // Object was destroyed
        }

        // Only apply the specific update type we're processing
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
            
        case UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA:
            object->UpdateComputedTextSize();
            break;
            
        default:
            break;
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