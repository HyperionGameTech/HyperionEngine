#ifndef COMPUTE_SHADER_H
#define COMPUTE_SHADER_H

#include "../../shader.h"

namespace hyperion {
class ComputeShader : public Shader {
public:
    ComputeShader(const ShaderProperties &properties, int width, int height, int length);
    virtual ~ComputeShader();

    virtual void ApplyMaterial(const Material &mat) override final;
    virtual void ApplyTransforms(const Transform &transform, Camera *camera) override final;

    void Dispatch();

protected:
    int *m_work_group_size;
    int m_width;
    int m_height;
    int m_length;

    void GetWorkGroupSize();
};
} // namespace hyperion

#endif