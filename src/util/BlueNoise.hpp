/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>

// based on:
// https://eheitzresearch.wordpress.com/762-2/

namespace Hyperion {

struct BlueNoise
{
    static const SizeType totalBufferSize;

    static const int32 sobol256spp256d[256 * 256];
    static const int32 scramblingTile[128 * 128 * 8];
    static const int32 rankingTile[128 * 128 * 8];

    static float Sample(int pixelI, int pixelJ, int sampleIndex, int sampleDimension);
};

} // namespace Hyperion
