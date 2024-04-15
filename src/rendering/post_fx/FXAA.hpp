/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_FXAA_H
#define HYPERION_V2_FXAA_H

#include <rendering/PostFX.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

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
    virtual Handle<Shader> CreateShader() override;
};

} // namespace hyperion::v2

#endif