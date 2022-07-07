#ifndef HYPERION_V2_SSAO_H
#define HYPERION_V2_SSAO_H

#include <rendering/PostFX.hpp>

#include <Types.hpp>

namespace hyperion::v2 {

class SSAOEffect : public PostProcessingEffect {
public:
    static constexpr Stage stage = Stage::PRE_SHADING;
    static constexpr UInt  index = 0;

    SSAOEffect();
    virtual ~SSAOEffect();

protected:
    virtual Ref<Shader> CreateShader(Engine *engine) override;
};

} // namespace hyperion::v2

#endif