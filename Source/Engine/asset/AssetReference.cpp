/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <AssetPch.hpp>

#include <asset/AssetReference.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/AssetObject.hpp>
#include <asset/Assets.hpp>

#include <AssetReference.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

static const AssetPath s_invalidAssetPath;

const Handle<AssetObject>& ResolveAssetImpl(const AssetReference& assetReference)
{
    return assetReference.Resolve();
}

AssetReference::AssetReference(const Handle<AssetObject>& assetObject)
{
    if (assetObject)
    {
        m_data = assetObject;
    }
    else
    {
        m_data = AssetPath();
    }
}

const AssetPath& AssetReference::GetAssetPath() const
{
    if (m_data.Is<Handle<AssetObject>>())
    {
        const Handle<AssetObject>& assetObject = m_data.GetUnchecked<Handle<AssetObject>>();
        AssertDebug(assetObject != nullptr);

        const AssetPath& assetPath = assetObject->GetPath();
        AssertDebug(assetPath.IsValid());

        return assetPath;
    }

#if HYP_DEBUG_MODE
    AssertDebug(m_data.Is<AssetPath>());

    if (!m_data.Is<AssetPath>())
    {
        return s_invalidAssetPath;
    }
#endif

    const AssetPath& assetPath = m_data.GetUnchecked<AssetPath>();
    AssertDebug(assetPath.IsValid());

    return assetPath;
}

const Handle<AssetObject>& AssetReference::Resolve() const
{
    if (m_data.Is<Handle<AssetObject>>())
    {
        return m_data.GetUnchecked<Handle<AssetObject>>();
    }

    AssertDebug(m_data.Is<AssetPath>());

    const AssetPath& assetPath = m_data.GetUnchecked<AssetPath>();

    if (assetPath.IsValid())
    {
        Handle<AssetObject> assetObject = g_assetManager->GetAssetRegistry()->GetAssetFromPath(assetPath.ToString());

        if (assetObject)
        {
            return m_data.Emplace<Handle<AssetObject>>(assetObject);
        }

        HYP_LOG(Assets, Error, "Failed to resolve asset reference for path '{}'", assetPath);
    }

    return Handle<AssetObject>::empty;
}

void AssetReference::Reload()
{
    // only re-resolve if we're currently loaded
    if (m_data.Is<Handle<AssetObject>>())
    {
        AssetPath path = GetAssetPath();
        m_data = path;

        (void)Resolve();
    }
}

} // namespace Hyperion
