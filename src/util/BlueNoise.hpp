/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_BLUE_NOISE_HPP
#define HYPERION_BLUE_NOISE_HPP

#include <Types.hpp>

// based on:
// https://eheitzresearch.wordpress.com/762-2/

namespace hyperion {

struct BlueNoise
{
    static const SizeType total_buffer_size;

    static const int32 sobol_256spp_256d[256 * 256];
    static const int32 scrambling_tile[128 * 128 * 8];
    static const int32 ranking_tile[128 * 128 * 8];

    static float Sample(int pixel_i, int pixel_j, int sample_index, int sample_dimension);
};

} // namespace hyperion

#endif
