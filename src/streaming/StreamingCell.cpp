/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <streaming/StreamingCell.hpp>

#include <scene/world_grid/WorldGrid.hpp>

#include <asset/AssetReference.hpp>

#include <core/logging/Logger.hpp>

#include <StreamingCell.generated.inl>

namespace hyperion {

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

    // @TODO Trigger appropriate callback if shouldLoad was true
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
            if (!assetReference.Resolve())
            {
                HYP_LOG(Streaming, Warning, "Failed to resolve AssetReference {} in OnStreamStart", assetReference.GetAssetPath());

                continue;
            }
        }
    }
}

void StreamingCell::OnLoaded_Impl()
{
    OnCellLoaded(this);
}

void StreamingCell::OnRemoved_Impl()
{
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

} // namespace hyperion
