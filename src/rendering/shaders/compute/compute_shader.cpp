#include "compute_shader.h"
#include "../../../core_engine.h"
#include "../../../util.h"

namespace hyperion {
ComputeShader::ComputeShader(const ShaderProperties &properties)
    : Shader(properties),
      m_work_group_size(nullptr)
{
}

ComputeShader::~ComputeShader()
{
    if (m_work_group_size != nullptr) {
        delete[] m_work_group_size;
    }
}

void ComputeShader::ApplyMaterial(const Material &mat)
{
    AssertExitMsg(false, "Compute shader does not implement ApplyMaterial");
}

void ComputeShader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    AssertExitMsg(false, "Compute shader does not implement ApplyTransforms");
}

void ComputeShader::Dispatch(int width, int height, int length)
{
    AssertThrow(GetId() != 0);

    if (m_work_group_size == nullptr) {
        GetWorkGroupSize();
    }

    CoreEngine::GetInstance()->DispatchCompute(width / m_work_group_size[0], height / m_work_group_size[1], length / m_work_group_size[2]);
}

void ComputeShader::GetWorkGroupSize()
{
    AssertExit(m_work_group_size == nullptr);

    m_work_group_size = new int[3];

    CoreEngine::GetInstance()->GetProgram(GetId(), CoreEngine::GLEnums::COMPUTE_WORK_GROUP_SIZE, m_work_group_size);
}
} // namespace hyperion