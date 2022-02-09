#ifndef COMPUTE_SHADER_H
#define COMPUTE_SHADER_H

#include "../../shader.h"

namespace hyperion {
class ComputeShader : public Shader {
public:
    ComputeShader(const ShaderProperties &propertiesh);
    virtual ~ComputeShader();

    virtual void ApplyMaterial(const Material &mat) override final;
    virtual void ApplyTransforms(const Transform &transform, Camera *camera) override final;

    void Dispatch(int width, int height, int length);

protected:
    int *m_work_group_size;

    void GetWorkGroupSize();
};
} // namespace hyperion

#endif