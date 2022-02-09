#include "envmap_probe.h"
#include "envmap_probe_camera.h"
#include "../../environment.h"
#include "../../shader_manager.h"
#include "../../shaders/cubemap_renderer_shader.h"
#include "../../shaders/compute/sh_compute_shader.h"
#include "../../renderer.h"

#include "../../camera/perspective_camera.h"
#include "../../../math/matrix_util.h"

#include "../../../core_engine.h"
#include "../../../opengl.h"
#include "../../../gl_util.h"

namespace hyperion {
EnvMapProbe::EnvMapProbe(const Vector3 &origin, const BoundingBox &bounds, int width, int height, float near, float far)
    : Probe(fbom::FBOMObjectType("ENVMAP_PROBE"), origin, bounds),
      m_width(width),
      m_height(height),
      m_near(near),
      m_far(far),
      m_render_tick(0.0),
      m_render_index(0),
      m_is_first_run(true)
{
    m_fbo = new FramebufferCube(width, height);

    SetShader(ShaderManager::GetInstance()->GetShader<CubemapRendererShader>(ShaderProperties()));

    m_spherical_harmonics_shader = ShaderManager::GetInstance()->GetShader<SHComputeShader>(ShaderProperties());
    m_sh_texture = std::make_shared<Texture2D>(8, 8, nullptr);
    m_sh_texture->SetFormat(CoreEngine::GLEnums::RGBA);
    m_sh_texture->SetInternalFormat(CoreEngine::GLEnums::RGBA8);
    m_sh_texture->SetFilter(CoreEngine::GLEnums::NEAREST, CoreEngine::GLEnums::NEAREST);
    m_sh_texture->SetWrapMode(CoreEngine::GLEnums::CLAMP_TO_EDGE, CoreEngine::GLEnums::CLAMP_TO_EDGE);

    for (int i = 0; i < m_cameras.size(); i++) {
        ProbeRegion region;
        region.origin = m_origin;
        region.bounds = m_bounds;
        region.direction = m_directions[i].first;
        region.up_vector = m_directions[i].second;
        region.index = i;

        auto *cam = new EnvMapProbeCamera(region, width, height, near, far);
        cam->SetTexture(GetColorTexture().get());
        m_cameras[i] = cam;
    }
}

EnvMapProbe::~EnvMapProbe()
{
    for (ProbeCamera *cam : m_cameras) {
        if (cam == nullptr) {
            continue;
        }

        delete cam;
    }

    delete m_fbo;
}

void EnvMapProbe::Bind(Shader *shader)
{
    GetColorTexture()->Prepare();
    shader->SetUniform("env_GlobalCubemap", GetColorTexture().get());

    shader->SetUniform("EnvProbe.position", m_origin);
    shader->SetUniform("EnvProbe.max", m_bounds.GetMax());
    shader->SetUniform("EnvProbe.min", m_bounds.GetMin());

    shader->SetUniform("SphericalHarmonicsMap", m_sh_texture.get());
    shader->SetUniform("HasSphericalHarmonicsMap", 1);
}

void EnvMapProbe::Update(double dt)
{
    m_render_tick += dt;

    for (ProbeCamera *cam : m_cameras) {
        cam->Update(dt); // update matrices
    }
}

void EnvMapProbe::Render(Renderer *renderer, Camera *cam)
{
    if (!ProbeManager::GetInstance()->EnvMapEnabled()) {
        return;
    }

    if (m_is_first_run) {
        RenderCubemap(renderer, cam);
        RenderSphericalHarmonics();

        m_is_first_run = false;

        return;
    }

    if (m_render_tick < 0.25) {
        return;
    }

    m_render_tick = 0.0;

    RenderCubemap(renderer, cam);
    RenderSphericalHarmonics();
}


void EnvMapProbe::RenderCubemap(Renderer *renderer, Camera *cam)
{
    m_fbo->Use();

    CoreEngine::GetInstance()->Clear(CoreEngine::GLEnums::COLOR_BUFFER_BIT | CoreEngine::GLEnums::DEPTH_BUFFER_BIT);

    for (int i = 0; i < m_cameras.size(); i++) {
        m_shader->SetUniform("u_shadowMatrices[" + std::to_string(i) + "]", m_cameras[i]->GetCamera()->GetViewProjectionMatrix());
    }

    renderer->RenderBucket(
        cam,
        renderer->GetBucket(Renderable::RB_OPAQUE),
        m_shader.get(),
        false
    );

    renderer->RenderBucket(
        cam,
        renderer->GetBucket(Renderable::RB_TRANSPARENT),
        m_shader.get(),
        false
    );

    m_fbo->End();
}

void EnvMapProbe::RenderSphericalHarmonics()
{
    if (!ProbeManager::GetInstance()->SphericalHarmonicsEnabled()) {
        return;
    }

    if (!m_sh_texture->IsUploaded()) {
        m_sh_texture->Begin(false); // do not upload texture data
        CatchGLErrors("Failed to begin texture storage 2d for spherical harmonics");
        // this ought to be refactored into a more reusable format
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, m_sh_texture->GetWidth(), m_sh_texture->GetHeight());
        CatchGLErrors("Failed to set texture storage 2d for spherical harmonics");
        m_sh_texture->End();
        CatchGLErrors("Failed to end texture storage 2d for spherical harmonics");
    }

    glBindImageTexture(0, m_sh_texture->GetId(), 0, false, 0, GL_WRITE_ONLY, GL_RGBA8);
    CatchGLErrors("Failed to bind imagetexture");
    m_spherical_harmonics_shader->SetUniform("srcTex", GetColorTexture().get());
    m_spherical_harmonics_shader->Use();
    m_spherical_harmonics_shader->Dispatch(8, 8, 1);
    m_spherical_harmonics_shader->End();
    glBindImageTexture(0, 0, 0, false, 0, GL_WRITE_ONLY, GL_RGBA8);
}

std::shared_ptr<Renderable> EnvMapProbe::CloneImpl()
{
    return std::make_shared<EnvMapProbe>(m_origin, m_bounds, m_width, m_height, m_near, m_far);
}
} // namespace hyperion
