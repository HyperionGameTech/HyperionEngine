/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <StreamingPch.hpp>

#include <streaming/StreamingCell.hpp>

#include <scene/world_grid/WorldGrid.hpp>

#include <asset/AssetReference.hpp>

#include <StreamingCell.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Streaming);

StreamingCell::StreamingCell(const StreamingCellInfo& cellInfo)
    : m_cellInfo(cellInfo)
{
}

StreamingCell::~StreamingCell()
{
}

void StreamingCell::AddAssetReference(const AssetReference& assetReference, bool shouldLoad)
{
    AssertDebug(assetReference.IsValid());
    if (!assetReference.IsValid())
    {
        return;
    }

    AssertDebug(!m_assetReferences.Contains(assetReference));

    if (shouldLoad && !assetReference.IsLoaded() && !assetReference.Resolve())
    {
        HYP_LOG(Streaming, Warning, "Failed to resolve AssetReference {}", assetReference.GetAssetPath());
    }

    /// \todo Trigger appropriate callback if shouldLoad was true
    m_assetReferences.PushBack(assetReference);
}

void StreamingCell::RemoveAssetReference(const AssetReference& assetReference)
{
    if (!assetReference.IsValid())
    {
        return;
    }

    auto it = m_assetReferences.Find(assetReference);
    if (it != m_assetReferences.End())
    {
        // Trigger appropriate callback if it was loaded
        if (it->IsLoaded())
        {
        }

        m_assetReferences.Erase(it);
    }
}

void StreamingCell::OnStreamStart_Impl()
{
    for (AssetReference& assetReference : m_assetReferences)
    {
        AssertDebug(assetReference.IsValid());

        if (!assetReference.IsValid())
        {
            continue;
        }

        if (!assetReference.IsLoaded())
        {
            HYP_LOG(Streaming, Verbose, "OnStreamStart: Loading AssetReference {} for {} at {}",
                assetReference.GetAssetPath().ToString(),
                InstanceClass()->GetName(),
                m_cellInfo.coord);

            const Handle<AssetObject>& assetObject = assetReference.Resolve();

            if (!assetObject.IsValid())
            {
                HYP_LOG(Streaming, Warning, "Failed to resolve AssetReference {} in OnStreamStart", assetReference.GetAssetPath().ToString());

                continue;
            }

            assetObject->InstanceClass()->PostLoad(assetObject);
        }
    }
}

void StreamingCell::OnLoaded_Impl()
{
    HYP_LOG(Streaming, Verbose, "OnLoaded: {} loaded at {} with {} AssetReferences",
        InstanceClass()->GetName(),
        m_cellInfo.coord,
        m_assetReferences.Size());

    OnCellLoaded(this);
}

void StreamingCell::OnRemoved_Impl()
{
    HYP_LOG(Streaming, Verbose, "OnRemoved: {} removed at {} with {} AssetReferences",
        InstanceClass()->GetName(),
        m_cellInfo.coord,
        m_assetReferences.Size());

    OnCellUnloaded(this);

    for (AssetReference& assetReference : m_assetReferences)
    {
        if (!assetReference.IsValid() || !assetReference.IsLoaded())
        {
            continue;
        }

        AssetPath assetPath = assetReference.GetAssetPath();
        assetReference = AssetReference(std::move(assetPath));
    }
}

} // namespace Hyperion
