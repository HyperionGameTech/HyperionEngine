#ifndef HYPERION_BLUE_NOISE_HPP
#define HYPERION_BLUE_NOISE_HPP

#include <Types.hpp>

// based on:
// https://eheitzresearch.wordpress.com/762-2/

namespace hyperion {

struct BlueNoise
{
    static const SizeType total_buffer_size;

    static const Int32 sobol_256spp_256d[256 * 256];
    static const Int32 scrambling_tile[128 * 128 * 8];
    static const Int32 ranking_tile[128 * 128 * 8];

    static Float Sample(Int pixel_i, Int pixel_j, Int sample_index, Int sample_dimension);
};

} // namespace hyperion

#endif
