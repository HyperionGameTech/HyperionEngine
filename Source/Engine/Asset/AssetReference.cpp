/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <AssetPch.hpp>

#include <Asset/AssetReference.hpp>
#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetObject.hpp>
#include <Asset/Assets.hpp>

#include <AssetReference.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Assets);

static const AssetPath s_invalidAssetPath;

static_assert(sizeof(AssetPath) == 12);

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
    if (assetPath.IsValid())
    {
        return assetPath;
    }

    return s_invalidAssetPath;
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
        Handle<AssetRegistry> registry;

        switch (assetPath.registryId)
        {
        case AssetRegistryId::Game:
            registry = GetCurrentAssetRegistry();
            break;
        case AssetRegistryId::Engine:
            registry = GetEngineAssetRegistry();
            break;
#ifdef HYP_EDITOR
        case AssetRegistryId::Editor:
            registry = GetEditorAssetRegistry();
            break;
#endif // HYP_EDITOR
        }

        AssertDebug(registry.IsValid());

        if (!registry.IsValid())
        {
            return Handle<AssetObject>::Null();
        }

        Handle<AssetObject> assetObject = registry->GetAsset(assetPath.GetBucket(), assetPath.assetName);

        if (assetObject)
        {
            // TEMP debug
            if (assetPath.assetName.ToString().StartsWith("reflprobe"))
            {
                HYP_LOG(Assets, Info, "reflprobe asset (obj name) = {}", assetObject->GetName());
            }

            return m_data.Emplace<Handle<AssetObject>>(assetObject);
        }

        HYP_LOG(Assets, Error, "Failed to resolve asset reference for path '{}'", assetPath);
    }

    return Handle<AssetObject>::Null();
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
