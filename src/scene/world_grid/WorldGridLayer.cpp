/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <ScenePch.hpp>

#include <scene/world_grid/WorldGridLayer.hpp>
#include <scene/world_grid/WorldGrid.hpp>

#include <streaming/StreamingCell.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetObject.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/AssetReference.hpp>

#include <WorldGridLayer.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Streaming);

#pragma region WorldGridLayer

Handle<StreamingCell> WorldGridLayer::CreateStreamingCell_Impl(const StreamingCellInfo& cellInfo)
{
    HYP_SCOPE;
    Handle<StreamingCell> cell = MakeHandle<StreamingCell>(cellInfo);

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
                    const Handle<AssetObject>& obj = assetReference.Resolve();

                    AssertDebug(obj.IsValid(), "Could not resolve AssetReference: {}",
                        assetReference.GetAssetPath().ToString());

                    if (obj.IsValid())
                    {
                        objs.PushBack(obj.Get());
                    }
                }
                
                if (objs.Any())
                {
                    OnStreamingObjectsLoaded(cell, objs);
                }
            })
        .Detach();

    cell->OnCellUnloaded
        .Bind([this](StreamingCell* cell)
            {
                Array<const AssetObject*> objs;
                objs.Reserve(cell->GetAssetReferences().Size());

                for (const AssetReference& assetReference : cell->GetAssetReferences())
                {
                    const Handle<AssetObject>& obj = assetReference.Resolve();

                    AssertDebug(obj.IsValid(), "Could not resolve AssetReference: {}",
                        assetReference.GetAssetPath().ToString());

                    if (obj.IsValid())
                    {
                        objs.PushBack(obj.Get());
                    }
                }

                if (objs.Any())
                {
                    OnStreamingObjectsUnloaded(cell, objs);
                }
            })
        .Detach();

    return cell;
}

void WorldGridLayer::AddStreamingObject(const AssetObject* assetObject, const Vec2i& coord)
{
    HYP_SCOPE;

    if (!assetObject)
    {
        HYP_LOG(Streaming, Error, "Cannot insert NULL object into layer!");

        return;
    }

    if (!assetObject->IsRegistered())
    {
        // When AssetRegistry::RegisterAssetsRecursively is called, these will be moved to their proper packages/paths (i.e project package)
        g_assetManager->GetAssetRegistry()->RegisterAsset(
            HYP_FORMAT("$Memory/Objects/Types/{}", assetObject->InstanceClass()->GetName()),
            MakeStrongRef(assetObject));
    }

    /// \todo How will we update if the obj moves to a different path in editor?? - FIXME when we add some Delegate like OnAssetPathChanged to AssetObject

    if (assetObject->IsTransient())
    {
        // transient assets must be kept in memory as their path may change if they are saved
        m_objectsByCoord[coord].EmplaceBack(MakeStrongRef(assetObject));

        return;
    }

    // don't keep transient assets in memory; store path instead.
    m_objectsByCoord[coord].EmplaceBack(assetObject->GetPath());
}

void WorldGridLayer::RemoveStreamingObject(const AssetObject* assetObject)
{
    HYP_SCOPE;

    if (!assetObject)
    {
        HYP_LOG(Streaming, Error, "Cannot remove NULL object from layer!");

        return;
    }

    for (auto objectsIt = m_objectsByCoord.Begin(); objectsIt != m_objectsByCoord.End(); ++objectsIt)
    {
        Array<AssetReference, DynamicAllocator>& assetsAtCoord = objectsIt->second;

        for (SizeType i = 0; i < assetsAtCoord.Size(); ++i)
        {
            if (assetsAtCoord[i].GetAssetPath() == assetObject->GetPath())
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

    HYP_LOG(Streaming, Warning, "Object {} not found in layer {}", assetObject->GetName(), m_name);

    /// \todo needs to remove from actual StreamingCell if already loaded!!
}

#pragma endregion WorldGridLayer

} // namespace Hyperion
