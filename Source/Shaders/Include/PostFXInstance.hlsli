#ifndef HYP_POST_FX_INSTANCE
#define HYP_POST_FX_INSTANCE

#include "PostFXCommon.hlsli"

layout(push_constant) uniform PushConstant {
    uint current_effect_index_stage; // 31 bits for index, 1 bit for stage
} this_effect;

// use from post fx, to sample the image "behind" this
vec4 SamplePrevEffectInChain(uint stage, uint index, vec2 texcoord, in vec4 default_value)
{
    if (index == 0) {
        if (stage == HYP_STAGE_POST) {
            return SAMPLE_TEXTURE_2D(HYP_SAMPLER_NEAREST, DeferredResult, texcoord);
        }
    } else {
        return SAMPLE_TEXTURE_2D(HYP_SAMPLER_NEAREST, effects_post_stack[index - 1], texcoord);
    }

    return default_value;
}

// use from post fx, to sample the image "behind" this
vec4 SamplePrevEffectInChain(uint index_stage_combined, vec2 texcoord, in vec4 default_value)
{
    uint index = index_stage_combined >> 1;
    uint stage = index_stage_combined & 0x1;

    if (index == 0) {
        if (stage == HYP_STAGE_POST) {
            return SAMPLE_TEXTURE_2D(HYP_SAMPLER_NEAREST, DeferredResult, texcoord);
        }
    } else {
        return SAMPLE_TEXTURE_2D(HYP_SAMPLER_NEAREST, effects_post_stack[index - 1], texcoord);
    }

    return default_value;
}

// use from post fx, to sample the image "behind" this
vec4 SamplePrevEffectInChain(vec2 texcoord, in vec4 default_value)
{
    uint index = this_effect.current_effect_index_stage >> 1;
    uint stage = this_effect.current_effect_index_stage & 0x1;

    if (index == 0) {
        if (stage == HYP_STAGE_POST) {
            return SAMPLE_TEXTURE_2D(HYP_SAMPLER_NEAREST, DeferredResult, texcoord);
        }
    } else {
        return SAMPLE_TEXTURE_2D(HYP_SAMPLER_NEAREST, effects_post_stack[index - 1], texcoord);
    }

    return default_value;
}


#endif