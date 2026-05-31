/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Asset/AssetBatch.hpp>

#include <Core/Types.hpp>

using namespace Hyperion;

extern "C"
{
    struct ManagedAssetMap
    {
        AssetMap* map;
    };
} // extern "C"
