/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

namespace Hyperion {

enum class AssetRegistryId : uint32;

HYP_STRUCT()
struct BlobEntry
{
    HYP_STRUCT_BODY(BlobEntry)

    HYP_FIELD()
    uint64 key;

    HYP_FIELD()
    uint64 size;

    HYP_FIELD()
    String magic;
};

HYP_STRUCT()
struct AssetEntry
{
    HYP_STRUCT_BODY(AssetEntry)

    HYP_FIELD()
    AssetRegistryId registryId;

    HYP_FIELD()
    uint32 bucketIndex;

    HYP_FIELD()
    Name name;

    HYP_FIELD()
    uint64 lastModifiedTimestamp;

    HYP_FIELD()
    Array<BlobEntry> blobs;
};

HYP_STRUCT()
struct CookManifest
{
    HYP_STRUCT_BODY(CookManifest)

    HYP_FIELD()
    uint64 cookTimestamp;

    HYP_FIELD()
    Array<AssetEntry> assets;
};

} // namespace Hyperion