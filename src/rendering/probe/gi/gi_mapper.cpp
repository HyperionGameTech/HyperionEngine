#include "gi_mapper.h"

#include "../../shader_manager.h"
#include "../../shaders/gi/gi_voxel_shader.h"
#include "../../shaders/gi/gi_voxel_clear_shader.h"
#include "../../renderer.h"
#include "../../environment.h"
#include "../../../math/math_util.h"

#include "../../../opengl.h"
#include "../../../gl_util.h"

namespace hyperion {
GIMapper::GIMapper(const Vector3 &origin, const BoundingBox &bounds)
    : Probe(fbom::FBOMObjectType("GI_MAPPER"), origin, bounds),
      m_render_tick(0.0),
      m_render_index(0),
      m_is_first_run(true)
{
    SetShader(ShaderManager::GetInstance()->GetShader<GIVoxelShader>(ShaderProperties()));

    for (int i = 0; i < m_cameras.size(); i++) {
        ProbeRegion region;
        region.bounds = m_bounds;
        region.direction = m_directions[i].first;
        region.up_vector = m_directions[i].second;
        region.index = i;

        m_cameras[i] = new GIMapperCamera(region);
        m_cameras[i]->SetShader(m_shader);
    }
}

GIMapper::~GIMapper()
{
    for (ProbeCamera *cam : m_cameras) {
        if (cam == nullptr) {
            continue;
        }

        delete cam;
    }
}

void GIMapper::Update(double dt)
{
    m_render_tick += dt;

    for (ProbeCamera *gi_cam : m_cameras) {
        gi_cam->Update(dt); // update matrices
    }
}

void GIMapper::Bind(Shader *shader)
{
    shader->SetUniform("VoxelProbePosition", m_bounds.GetCenter());
    shader->SetUniform("VoxelSceneScale", m_bounds.GetDimensions());

    for (int j = 0; j < m_cameras.size(); j++) {
        shader->SetUniform(std::string("VoxelMap[") + std::to_string(j) + "]", m_cameras[j]->GetTexture());
    }
}

void GIMapper::Render(Renderer *renderer, Camera *cam)
{
    if (!ProbeManager::GetInstance()->VCTEnabled()) {
        return;
    }

    if (m_is_first_run) {
        for (int i = 0; i < m_cameras.size(); i++) {
            m_cameras[i]->Render(renderer, cam);
        }

        m_is_first_run = false;

        return;
    }

    if (m_render_tick < 1.0) {
        return;
    }

    m_render_tick = 0.0;

    m_cameras[m_render_index]->Render(renderer, cam);

    m_render_index++;
    m_render_index = m_render_index % m_cameras.size();
}

std::shared_ptr<Renderable> GIMapper::CloneImpl()
{
    return std::make_shared<GIMapper>(m_origin, m_bounds);
}
} // namespace apex