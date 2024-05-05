/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FXAA_HPP
#define HYPERION_FXAA_HPP

#include <rendering/PostFX.hpp>
#include <Types.hpp>

namespace hyperion {

class HYP_API FXAAEffect : public PostProcessingEffect
{
public:
    static constexpr PostProcessingStage stage = POST_PROCESSING_STAGE_POST_SHADING;
    static constexpr uint index = 0;

    FXAAEffect();
    virtual ~FXAAEffect();

    virtual void OnAdded() override;
    virtual void OnRemoved() override;

protected:
    virtual ShaderRef CreateShader() override;
};

} // namespace hyperion

#endif