/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <rendering/PostFX.hpp>
#include <Core/Types.hpp>

namespace Hyperion {

class HYP_API FXAAEffect : public PostProcessingEffect
{
public:
    static constexpr PostProcessingStage stage = POST_PROCESSING_STAGE_POST_SHADING;
    static constexpr uint32 index = 0;

    FXAAEffect(GBuffer* gbuffer);
    virtual ~FXAAEffect() override;

    virtual void OnAdded() override;
    virtual void OnRemoved() override;

protected:
    virtual ShaderDesc GetShaderDesc() override;
};

} // namespace Hyperion
