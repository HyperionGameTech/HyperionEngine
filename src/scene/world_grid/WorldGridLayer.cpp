/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <scene/world_grid/WorldGridLayer.hpp>
#include <scene/world_grid/WorldGrid.hpp>

#include <streaming/StreamingCell.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetObject.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/AssetReference.hpp>

#include <engine/EngineGlobals.hpp>

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

    cell->OnCellLoaded
        .Bind([this](StreamingCell* cell)
            {
                Array<const AssetObject*> objs;
                objs.Reserve(cell->GetAssetReferences().Size());

                for (const AssetReference& assetReference : cell->GetAssetReferences())
                {
                    AssertDebug(assetReference.IsLoaded());

                    const Handle<AssetObject>& obj = assetReference.Resolve();
                    AssertDebug(obj != nullptr);

                    objs.PushBack(obj.Get());
                }

                OnStreamingObjectsLoaded(cell, objs);
            })
        .Detach();

    cell->OnCellUnloaded
        .Bind([this](StreamingCell* cell)
            {
                Array<const AssetObject*> objs;
                objs.Reserve(cell->GetAssetReferences().Size());

                for (const AssetReference& assetReference : cell->GetAssetReferences())
                {
                    AssertDebug(assetReference.IsLoaded());

                    const Handle<AssetObject>& obj = assetReference.Resolve();
                    AssertDebug(obj != nullptr);

                    objs.PushBack(obj.Get());
                }

                OnStreamingObjectsUnloaded(cell, objs);
            })
        .Detach();

    return cell;
}

void WorldGridLayer::AddStreamingObject(const AssetObject* obj, const Vec2i& coord)
{
    HYP_SCOPE;

    if (!obj)
    {
        HYP_LOG(Streaming, Error, "Cannot insert NULL object into layer!");

        return;
    }

    if (!obj->IsRegistered())
    {
        // @TODO change this; either ensure registered before calling this, or import to temp location which will be added to this package upon save / RegisterAssetsRecursively()
        //g_assetManager->GetAssetRegistry()->RegisterAsset(HYP_FORMAT("$Import/Objects/Types/{}/{}", obj->InstanceClass()->GetName(), obj->GetName()), MakeStrongRef(obj));
        g_assetManager->GetAssetRegistry()->RegisterAsset(HYP_FORMAT("$Temp/{}", obj->GetUUID()), MakeStrongRef(obj));
    }

    // @TODO needs to add to actual StreamingCell if already loaded!!
    // @TODO How will we update if the obj moves to a different path in editor?? - FIXME when we add some Delegate like OnAssetPathChanged to AssetObject

    if (obj->IsTransient())
    {
        // transient assets must be kept in memory as their path may change if they are saved
        m_objectsByCoord[coord].EmplaceBack(MakeStrongRef(obj));
        return;
    }

    // don't keep transient assets in memory; store path instead.
    m_objectsByCoord[coord].EmplaceBack(obj->GetPath());
}

void WorldGridLayer::RemoveStreamingObject(const AssetObject* obj)
{
    HYP_SCOPE;

    if (!obj)
    {
        HYP_LOG(Streaming, Error, "Cannot remove NULL object from layer!");

        return;
    }

    for (auto objectsIt = m_objectsByCoord.Begin(); objectsIt != m_objectsByCoord.End(); ++objectsIt)
    {
        Array<AssetReference, DynamicAllocator>& assetsAtCoord = objectsIt->second;

        for (SizeType i = 0; i < assetsAtCoord.Size(); ++i)
        {
            if (assetsAtCoord[i].GetAssetPath() == obj->GetPath())
            {
                assetsAtCoord.EraseAt(i);

                if (assetsAtCoord.Empty())
                {
                    m_objectsByCoord.Erase(objectsIt);
                }

                return;
            }
        }
    }

    HYP_LOG(Streaming, Warning, "Object {} not found in layer {}", obj->GetName(), m_name);

    // @TODO needs to remove from actual StreamingCell if already loaded!!
}

#pragma endregion WorldGridLayer

} // namespace hyperion
