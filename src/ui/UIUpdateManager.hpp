/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Array.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/HashSet.hpp>
#include <core/object/Handle.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/IdGenerator.hpp>
#include <ui/UIObject.hpp>

namespace hyperion {

class UIObject;
class UIStage;

/*! \brief Manages selective updates for UI objects to avoid expensive tree traversals */
class HYP_API UIUpdateManager
{
public:
    UIUpdateManager();
    virtual ~UIUpdateManager() = default;

    UIUpdateManager(const UIUpdateManager&) = delete;
    UIUpdateManager& operator=(const UIUpdateManager&) = delete;

    /*! \brief Register a UIObject that needs updating */
    virtual void RegisterForUpdate(UIObject* uiObject, EnumFlags<UIObjectUpdateType> updateTypes);

    /*! \brief Unregister a UIObject from updates */
    virtual void UnregisterFromUpdate(UIObject* uiObject);

    /*! \brief Process all pending updates in optimal order */
    virtual void ProcessUpdates(float delta);

    /*! \brief Clear all pending updates */
    void Clear();

    /*! \brief Get the number of objects waiting for updates */
    SizeType GetPendingUpdateCount() const { return m_pendingObjects.Size(); }

private:
    struct UpdateEntry
    {
        int index = -1;
        WeakHandle<UIObject> object;
        EnumFlags<UIObjectUpdateType> updateTypes;
        int depth = 0; // For sorting by hierarchy depth
    };

    SparsePagedArray<UpdateEntry, 2048> m_entryPool;
    IdGenerator m_entryIdGenerator;

    // Objects that need updating, organized by individual update type for proper batching
    HashMap<UIObjectUpdateType, Array<UpdateEntry*>> m_updateQueues;
    
    // All objects that have pending updates (for quick lookup)
    HashMap<WeakHandle<UIObject>, UpdateEntry*> m_pendingObjects;

    static constexpr UIObjectUpdateType s_updateOrder[] = {
        UIObjectUpdateType::UPDATE_SIZE,
        UIObjectUpdateType::UPDATE_POSITION,
        UIObjectUpdateType::UPDATE_CLAMPED_SIZE,
        UIObjectUpdateType::UPDATE_COMPUTED_VISIBILITY,
        UIObjectUpdateType::UPDATE_MATERIAL,
        UIObjectUpdateType::UPDATE_MESH_DATA,
        UIObjectUpdateType::UPDATE_CUSTOM
    };

    void ProcessUpdateType(UIObjectUpdateType updateType, float delta);
    void SortByDepth(Array<UpdateEntry*>& entries);
};

} // namespace hyperion