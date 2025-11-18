/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <scene/world_grid/WorldGridLayer.hpp>
#include <scene/world_grid/WorldGrid.hpp>

#include <streaming/StreamingCell.hpp>

#include <asset/AssetObject.hpp>
#include <asset/AssetReference.hpp>

#include <core/logging/Logger.hpp>

#include <WorldGridLayer.generated.inl>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Streaming);

#pragma region WorldGridLayer

Handle<StreamingCell> WorldGridLayer::CreateStreamingCell_Impl(const StreamingCellInfo& cellInfo)
{
    HYP_SCOPE;
    Handle<StreamingCell> cell = CreateObject<StreamingCell>(cellInfo);

    auto objectsByCoordIt = m_objectsByCoord.Find(cellInfo.coord);
    if (objectsByCoordIt != m_objectsByCoord.End())
    {
        for (const AssetReference& assetReference : objectsByCoordIt->second)
        {
            cell->AddAssetReference(assetReference, /* shouldLoad */ false);
        }
    }

    objectsByCoordIt = m_transientObjectsByCoord.Find(cellInfo.coord);
    if (objectsByCoordIt != m_transientObjectsByCoord.End())
    {
        for (const AssetReference& assetReference : objectsByCoordIt->second)
        {
            cell->AddAssetReference(assetReference, /* shouldLoad */ false);
        }
    }
}

void WorldGridLayer::InsertNewObject(const AssetObject* obj, const Vec2i& coord)
{
    HYP_SCOPE;

    if (!obj)
    {
        HYP_LOG(Streaming, Error, "Cannot insert NULL object into layer!");

        return;
    }

    // @TODO needs to add to actual StreamingCell if already loaded!!
    // @TODO How will we update if the obj moves to a different path in editor?? - FIXME when we add some Delegate like OnAssetPathChanged to AssetObject

    // if the object is transient we need to keep it persistently loaded...
    // would be nice to find a good way to move transient object that get saved over to non-transient list so they can be unloaded
    if (obj->IsTransient())
    {
        m_transientObjectsByCoord[coord].EmplaceBack(MakeStrongRef(obj));
    }
    else
    {
        m_objectsByCoord[coord].EmplaceBack(obj->GetPath());
    }
        
}

#pragma endregion WorldGridLayer

} // namespace hyperion
