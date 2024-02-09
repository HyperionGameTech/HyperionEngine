#ifndef HYPERION_V2_FXAA_H
#define HYPERION_V2_FXAA_H

#include <rendering/PostFX.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

class FXAAEffect : public PostProcessingEffect
{
public:
    static constexpr Stage stage = Stage::POST_SHADING;
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