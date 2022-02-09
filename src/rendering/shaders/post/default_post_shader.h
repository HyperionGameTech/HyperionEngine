#ifndef DEFAULT_POST_H
#define DEFAULT_POST_H

#include "../post_shader.h"

namespace hyperion {

class DefaultPostShader : public PostShader {
public:
    DefaultPostShader(const ShaderProperties &properties);
    virtual ~DefaultPostShader() = default;

    virtual void ApplyTransforms(const Transform &transform, Camera *camera) override;
};

} // namespace hyperion

#endif
