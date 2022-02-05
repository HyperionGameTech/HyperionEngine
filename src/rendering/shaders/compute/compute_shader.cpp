#include "compute_shader.h"
#include "../../../core_engine.h"
#include "../../../util.h"

namespace hyperion {
ComputeShader::ComputeShader(const ShaderProperties &properties, int width, int height, int length)
    : Shader(properties),
      m_work_group_size(nullptr),
      m_width(width),
      m_height(height),
      m_length(length)
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
    hard_assert_msg(false, "Compute shader does not implement ApplyMaterial");
}

void ComputeShader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    hard_assert_msg(false, "Compute shader does not implement ApplyTransforms");
}

void ComputeShader::Dispatch()
{
    ex_assert(GetId() != 0);

    if (m_work_group_size == nullptr) {
        GetWorkGroupSize();
    }

    CoreEngine::GetInstance()->DispatchCompute(m_width / m_work_group_size[0], m_height / m_work_group_size[1], m_length / m_work_group_size[2]);
}

void ComputeShader::GetWorkGroupSize()
{
    hard_assert(m_work_group_size == nullptr);

    m_work_group_size = new int[3];

    CoreEngine::GetInstance()->GetProgram(GetId(), CoreEngine::GLEnums::COMPUTE_WORK_GROUP_SIZE, m_work_group_size);
}
} // namespace hyperion