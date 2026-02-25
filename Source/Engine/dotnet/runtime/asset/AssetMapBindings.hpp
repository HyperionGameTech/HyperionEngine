/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <asset/AssetBatch.hpp>

#include <Core/Types.hpp>

using namespace Hyperion;

extern "C"
{
    struct ManagedAssetMap
    {
        AssetMap* map;
    };
} // extern "C"
