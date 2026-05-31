/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <AssetPch.hpp>

#include <Asset/AssetPath.hpp>

#include <AssetPath.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Assets);

static constexpr AssetRegistryId GetAssetRegistryIndex(StringHash hash)
{
    constexpr HashCode::ValueType GameRegistryHash = ("Game"_sh).hashCode;
    constexpr HashCode::ValueType EngineRegistryHash = ("Engine"_sh).hashCode;
    constexpr HashCode::ValueType EditorRegistryHash = ("Editor"_sh).hashCode;

    switch (hash.hashCode)
    {
    case GameRegistryHash:
        return AssetRegistryId::Game;
    case EngineRegistryHash:
        return AssetRegistryId::Engine;
    case EditorRegistryHash:
        return AssetRegistryId::Editor;
    }

    return AssetRegistryId::Game;
}

static constexpr const char* GetAssetRegistryName(AssetRegistryId registryId)
{
    switch (registryId)
    {
    case AssetRegistryId::Game:
        return "Game";
    case AssetRegistryId::Engine:
        return "Engine";
    case AssetRegistryId::Editor:
        return "Editor";
    }

    return "Game";
}

AssetPath::AssetPath(const ANSIStringView& path)
    : registryId(AssetRegistryId::Game),
      bucketIndex(AssetBucket::InvalidIndex)
{
    if (!path)
    {
        return;
    }

    ANSIStringView curr = path;

    size_t tokenIdx;

    // if ':' is found, we assume registry id is contained in the string,
    // (e.g Game://Materials/Barrel)
    // otherwise, we use the default registry id (Game)
    if (curr.Contains(':'))
    {
        tokenIdx = curr.FindFirstIndex(':');

        registryId = GetAssetRegistryIndex(StringHash(curr.Substr(0, tokenIdx)));

        // +3 handles "://"
        curr = curr.Substr(tokenIdx + 3, SIZE_MAX);
    }

    tokenIdx = curr.FindFirstIndex('/');

    if (tokenIdx != ANSIStringView::NotFound)
    {
        ANSIStringView bucketName = curr.Substr(0, tokenIdx);

        const AssetBucket& bucket = GetAssetBucketByName(StringHash(bucketName));
        AssertDebug(bucket != AssetBuckets::None, "Invalid asset bucket {}", bucketName);

        bucketIndex = bucket.GetIndex();
    }

    assetName = CreateNameFromDynamicString(curr.Substr(tokenIdx != ANSIStringView::NotFound ? tokenIdx + 1 : 0, SIZE_MAX));
}

String AssetPath::ToString() const
{
    if (!IsValid())
    {
        return String::empty;
    }

    const char* bucketName = GetAssetBucketName(bucketIndex);
    AssertDebug(bucketName != nullptr, "Invalid bucket index {}", bucketIndex);

    String str;
    str.Reserve(32);

    str += GetAssetRegistryName(registryId);
    str += "://";
    str += bucketName;
    str += '/';
    str += *assetName;

    return str;
}

} // namespace Hyperion
