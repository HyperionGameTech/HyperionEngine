#include "./ssao_filter.h"
#include "../../../asset/asset_manager.h"
#include "../../shader_manager.h"
#include "../../environment.h"
#include "../../shaders/post/ssao.h"

namespace hyperion {

SSAOFilter::SSAOFilter()
    : PostFilter(
        ShaderManager::GetInstance()->GetShader<SSAOShader>(ShaderProperties()
            .Define("KERNEL_SIZE", 64)
            .Define("CAP_MIN_DISTANCE", 0.00001f)
            .Define("CAP_MAX_DISTANCE", 0.01f)
        ),
        Framebuffer::FramebufferAttachment::FRAMEBUFFER_ATTACHMENT_SSAO
    )
{
    for (int i = 0; i < m_kernel.size(); i++) {
        m_kernel[i] = Vector3(MathUtil::Random(-0.1f, 0.1f), MathUtil::Random(-0.1f, 0.1f), MathUtil::Random(-0.1f, 0.1f));

        m_shader->SetUniform(std::string("u_kernel[") + std::to_string(i) + "]", m_kernel[i]);
    }

    m_noise_scale = Vector2(500.0f);
    m_shader->SetUniform("u_noiseScale", m_noise_scale);

    m_noise_map = AssetManager::GetInstance()->LoadFromFile<Texture2D>("textures/noise_ssao.png");

    if (m_noise_map == nullptr) {
        throw std::runtime_error("Could not load noise map!");
    }
}

void SSAOFilter::SetUniforms(Camera *cam)
{
    Texture::ActiveTexture(6);
    m_noise_map->Begin();
    m_shader->SetUniform("u_noiseMap", 6);

    m_shader->SetUniform("u_resolution", Vector2(cam->GetWidth(), cam->GetHeight()));

    m_shader->SetUniform("u_radius", 2.0f);
    m_shader->SetUniform("u_clipPlanes", Vector2(
        cam->GetFar() - cam->GetNear(), cam->GetNear()
    ));

    m_shader->SetUniform("u_view", cam->GetViewMatrix());
    m_shader->SetUniform("u_projectionMatrix", cam->GetProjectionMatrix());

    m_shader->SetUniform("uTanFovs", Vector2(
        tan((float(cam->GetWidth()) / float(cam->GetHeight())) * (cam->GetFov()) * 0.5),
        tan(cam->GetFov() * 0.5)
    ));

    m_shader->SetUniform("uLightPos", Environment::GetInstance()->GetSun().GetDirection());

    Matrix4 inverse_projection(cam->GetProjectionMatrix());
    inverse_projection.Invert();

    m_shader->SetUniform("u_inverseProjectionMatrix", inverse_projection);
}

} // namespace hyperion
