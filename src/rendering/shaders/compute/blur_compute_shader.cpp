#include "blur_compute_shader.h"

namespace hyperion {
BlurComputeShader::BlurComputeShader(const ShaderProperties &properties, int width, int height, int length)
    : ComputeShader(properties, width, height, length)
{
}

BlurComputeShader::~BlurComputeShader()
{
}
} // namespace hyperion