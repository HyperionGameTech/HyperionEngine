/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <AssetPch.hpp>

#include <asset/AssetPath.hpp>

#include <AssetPath.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

AssetPath::AssetPath(const UTF8StringView& path)
{
    if (!path)
    {
        return;
    }

    const size_t slashIdx = path.FindFirstIndex('/');

    if (slashIdx != UTF8StringView::NotFound)
    {
        UTF8StringView bucketName = path.Substr(0, slashIdx);

        const AssetBucket& bucket = GetAssetBucketByName(StringHash(bucketName));
        AssertDebug(bucket != AssetBuckets::None, "Invalid asset bucket {}", bucketName);

        bucketIndex = bucket.GetIndex();
    }

    assetName = CreateNameFromDynamicString(ANSIString(path.Substr(slashIdx != UTF8StringView::NotFound ? slashIdx + 1 : 0, SIZE_MAX)));
}

String AssetPath::ToString() const
{
    if (!IsValid())
    {
        return "<invalid asset path>";
    }

    const char* bucketName = GetAssetBucketName(bucketIndex);
    AssertDebug(bucketName != nullptr, "Invalid bucket index {}", bucketIndex);

    String str;
    str.Reserve(32);

    str += bucketName;
    str += '/';
    str += *assetName;

    return str;
}

} // namespace Hyperion
