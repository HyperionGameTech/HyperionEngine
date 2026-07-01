/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Map.hpp>
#include <Core/Containers/Set.hpp>
#include <Core/Reflection/Handle.hpp>
#include <Core/Utilities/EnumFlags.hpp>
#include <Core/Utilities/IndexAllocator.hpp>
#include <UI/UIObject.hpp>

namespace Hyperion {

class UIObject;
class UIStage;

/*! \brief Manages selective updates for UI objects to avoid expensive tree traversals */
class ENGINE_API UIUpdateManager
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
    size_t GetPendingUpdateCount() const
    {
        return m_pendingObjects.Size();
    }

private:
    struct UpdateEntry
    {
        int index = -1;
        int depth = 0; // For sorting by hierarchy depth
        WeakHandle<UIObject> object;
        EnumFlags<UIObjectUpdateType> updateTypes;
    };

    SparsePagedArray<UpdateEntry, 2048> m_entryPool;
    IndexAllocator m_entryIndexAllocator;

    TMap<UIObjectUpdateType, Array<UpdateEntry*>> m_updateQueues;
    TMap<WeakHandle<UIObject>, UpdateEntry*> m_pendingObjects;

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

} // namespace Hyperion
