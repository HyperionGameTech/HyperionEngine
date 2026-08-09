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

#include <Asset/AssetPath.hpp>

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
    AssetPath path;

    HYP_FIELD()
    uint64 lastModifiedTimestamp;

    HYP_FIELD()
    Array<BlobEntry> blobs;
};

HYP_STRUCT()
struct ServerManifest
{
    HYP_STRUCT_BODY(ServerManifest)

    HYP_FIELD()
    uint64 timestamp;

    HYP_FIELD()
    Array<AssetEntry> assets;
};

} // namespace Hyperion