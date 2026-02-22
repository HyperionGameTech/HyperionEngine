#ifndef HYP_BLUE_NOISE
#define HYP_BLUE_NOISE

// https://eheitzresearch.wordpress.com/762-2/
float SampleBlueNoise(int pixel_i, int pixel_j, int sample_index, int sample_dimension)
{
    // wrap arguments
    pixel_i = pixel_i & 127;
    pixel_j = pixel_j & 127;
    sample_index = sample_index & 255;
    sample_dimension = sample_dimension & 255;

#ifdef LANG_HLSL
    // HLSL: StructuredBuffer<int4> layout
    // sobol_256spp_256d: offset 0, size 16384 (256*256/4)
    // scrambling_tile: offset 16384, size 32768 (128*128*8/4)
    // ranking_tile: offset 49152, size 32768 (128*128*8/4)
    
    const uint ranking_tile_index = uint(sample_dimension + (pixel_i + pixel_j * 128) * 8);
    const uint ranking_tile_offset = 49152u;
    int ranked_sample_index = sample_index ^ BlueNoiseBuffer[ranking_tile_offset + (ranking_tile_index >> 2)][ranking_tile_index & 3];

    const uint sobol_index = uint(sample_dimension + ranked_sample_index * 256);
    const uint sobol_offset = 0u;
    
    const uint scrambling_tile_index = uint((sample_dimension % 8) + (pixel_i + pixel_j * 128) * 8);
    const uint scrambling_tile_offset = 16384u;

    int value = BlueNoiseBuffer[sobol_offset + (sobol_index >> 2)][sobol_index & 3];
    value = value ^ BlueNoiseBuffer[scrambling_tile_offset + (scrambling_tile_index >> 2)][scrambling_tile_index & 3];
#else
    // GLSL: named array layout
    const uint ranking_tile_index = uint(sample_dimension + (pixel_i + pixel_j * 128) * 8);
    int ranked_sample_index = sample_index ^ ranking_tile[ranking_tile_index >> 2][ranking_tile_index & 3];

    const uint sobol_index = uint(sample_dimension + ranked_sample_index * 256);
    const uint scrambling_tile_index = uint((sample_dimension % 8) + (pixel_i + pixel_j * 128) * 8);

    int value = sobol_256spp_256d[sobol_index >> 2][sobol_index & 3];
    value = value ^ scrambling_tile[scrambling_tile_index >> 2][scrambling_tile_index & 3];
#endif

    // convert to float and return
    float v = (0.5 + value) / 256.0;

    return v;
}

#endif