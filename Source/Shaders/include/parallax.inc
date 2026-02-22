#ifndef HYP_PARALLAX_GLSL
#define HYP_PARALLAX_GLSL

#include "material.inc"

#define PARALLAX_NUM_LAYERS 20

#define PARALLAX_SAMPLE_TEXTURE(texcoords) \
    (1.0 - SAMPLE_TEXTURE(CURRENT_MATERIAL, ParallaxMap, texcoords).r)

vec2 ParallaxMappedTexCoords(float parallax_height, vec2 texCoords, vec3 viewDir)
{
    // calculate the size of each layer
    float layerDepth = 1.0 / PARALLAX_NUM_LAYERS;
    // depth of current layer
    float currentLayerDepth = 0.0;
    // the amount to shift the texture coordinates per layer (from vector P)
    vec2 P = viewDir.xy * parallax_height;
    vec2 deltaTexCoords = P / PARALLAX_NUM_LAYERS;
    // get initial values
    vec2 currentTexCoords = texCoords;
    float currentDepthMapValue = PARALLAX_SAMPLE_TEXTURE(currentTexCoords);

    while (currentLayerDepth < currentDepthMapValue)
    {
        // shift texture coordinates along direction of P
        currentTexCoords -= deltaTexCoords;
        // get depthmap value at current texture coordinates
        currentDepthMapValue = PARALLAX_SAMPLE_TEXTURE(currentTexCoords);
        // get depth of next layer
        currentLayerDepth += layerDepth;
    }
    // get texture coordinates before collision (reverse operations)
    vec2 prevTexCoords = currentTexCoords + deltaTexCoords;

    // get depth after and before collision for linear interpolation
    float afterDepth = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = PARALLAX_SAMPLE_TEXTURE(prevTexCoords) - currentLayerDepth + layerDepth;

    // interpolation of texture coordinates
    float weight = afterDepth / (afterDepth - beforeDepth);
    vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

    return finalTexCoords;
}

#endif