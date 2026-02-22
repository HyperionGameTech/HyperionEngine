/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetBatch.hpp>

#include <core/Types.hpp>

using namespace Hyperion;

extern "C"
{
    struct ManagedAssetMap
    {
        AssetMap* map;
    };
} // extern "C"
