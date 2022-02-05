#ifndef BLUR_COMPUTE_SHADER_H
#define BLUR_COMPUTE_SHADER_H

#include "compute_shader.h"

namespace hyperion {
class BlurComputeShader : public ComputeShader {
public:
    BlurComputeShader(const ShaderProperties &properties, int width, int height, int length);
    virtual ~BlurComputeShader();
};
} // namespace hyperion

#endif