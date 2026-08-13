/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Streaming/StreamingCell.hpp>

#include <Scene/WorldGrid/WorldGrid.hpp>

#include <Asset/AssetReference.hpp>
#include <Asset/AssetObject.hpp>

#include <StreamingCell.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Streaming);

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

void StreamingCell::OnStreamStart()
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

void StreamingCell::OnLoaded()
{
    HYP_LOG(Streaming, Info, "OnLoaded: {} loaded at {} with {} AssetReferences",
        InstanceClass()->GetName(),
        m_cellInfo.coord,
        m_assetReferences.Size());

    OnCellLoaded(this);
}

void StreamingCell::OnRemoved()
{
    HYP_LOG(Streaming, Info, "OnRemoved: {} removed at {} with {} AssetReferences",
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
