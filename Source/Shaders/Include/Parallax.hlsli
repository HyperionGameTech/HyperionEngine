#ifndef HYP_PARALLAX
#define HYP_PARALLAX

#define PARALLAX_NUM_LAYERS 20

float SampleParallaxDepth(float2 texcoords, bool flipHeight)
{
    float height = SAMPLE_MATERIAL_TEXTURE(CURRENT_MATERIAL, ParallaxMap, texcoords).r;
    return select(flipHeight, 1.0 - height, height);
}

float2 ParallaxMappedTexCoords(float parallaxHeight, float2 texCoords, float3 viewDir, bool flipHeight)
{
    // calculate the size of each layer
    float layerDepth = 1.0 / PARALLAX_NUM_LAYERS;
    // depth of current layer
    float currentLayerDepth = 0.0;
    // the amount to shift the texture coordinates per layer (from vector P)
    float2 P = viewDir.xy / max(viewDir.z, 0.1) * parallaxHeight;
    float2 deltaTexCoords = P / PARALLAX_NUM_LAYERS;
    // get initial values
    float2 currentTexCoords = texCoords;
    float currentDepthMapValue = SampleParallaxDepth(currentTexCoords, flipHeight);

    const int MaxIterations = 20;
    int i = 0;

    while (currentLayerDepth < currentDepthMapValue && i < MaxIterations)
    {
        // shift texture coordinates along direction of P
        currentTexCoords -= deltaTexCoords;
        // get depthmap value at current texture coordinates
        currentDepthMapValue = SampleParallaxDepth(currentTexCoords, flipHeight);
        // get depth of next layer
        currentLayerDepth += layerDepth;
        
        i++;
    }
    
    // get texture coordinates before collision (reverse operations)
    float2 prevTexCoords = currentTexCoords + deltaTexCoords;

    // get depth after and before collision for linear interpolation
    float afterDepth = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = SampleParallaxDepth(prevTexCoords, flipHeight) - currentLayerDepth + layerDepth;

    // interpolation of texture coordinates
    float denom = afterDepth - beforeDepth;
    float weight = abs(denom) > 0.0001 ? afterDepth / denom : 0.0;
    float2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

    return finalTexCoords;
}

#endif