#include "gi_mapper.h"

#include "../shader_manager.h"
#include "../shaders/gi/gi_voxel_shader.h"
#include "../shaders/gi/gi_voxel_clear_shader.h"
#include "../renderer.h"
#include "../environment.h"
#include "../../math/math_util.h"
#include "../../opengl.h"
#include "../../gl_util.h"

namespace hyperion {
GIMapper::GIMapper(const BoundingBox &bounds)
    : Renderable(RB_BUFFER),
      m_bounds(bounds),
      m_render_tick(0.0),
      m_render_index(0),
      m_is_first_run(true),
      m_directions({
          std::make_pair(Vector3(1, 0, 0), Vector3(0, -1, 0)),
          std::make_pair(Vector3(-1, 0, 0), Vector3(0, -1, 0)),
          std::make_pair(Vector3(0, 1, 0), Vector3(0, 0, 1)),
          std::make_pair(Vector3(0, -1, 0), Vector3(0, 0, -1)),
          std::make_pair(Vector3(0, 0, 1), Vector3(0, -1, 0)),
          std::make_pair(Vector3(0, 0, -1), Vector3(0, -1, 0))
      })
{
    SetShader(ShaderManager::GetInstance()->GetShader<GIVoxelShader>(ShaderProperties()));

    for (int i = 0; i < m_cameras.size(); i++) {
        GIMapperRegion region;
        region.bounds = m_bounds;
        region.direction = m_directions[i].first;
        region.up_vector = m_directions[i].second;

        m_cameras[i] = new GIMapperCamera(region);
        m_cameras[i]->SetShader(m_shader);
    }
}

GIMapper::~GIMapper()
{
    for (GIMapperCamera *cam : m_cameras) {
        if (cam == nullptr) {
            continue;
        }

        delete cam;
    }
}

void GIMapper::SetOrigin(const Vector3 &origin)
{
    m_bounds.SetCenter(origin);

    for (GIMapperCamera *cam : m_cameras) {
        cam->GetRegion().bounds = m_bounds;
    }
}

void GIMapper::UpdateRenderTick(double dt)
{
    m_render_tick += dt;

    for (GIMapperCamera *gi_cam : m_cameras) {
        gi_cam->Update(dt); // update matrices
    }
}

// side is an opengl define - TEMP
Matrix4 cubemapmatrix(int side)
{
    Matrix4 m;
    switch (side)
    {
    case 0x8515: // +X
        m[0] = 0; m[4] = 0; m[8] = -1; m[12] = 0;
        m[1] = 0; m[5] = -1; m[9] = 0; m[13] = 0;
        m[2] = -1; m[6] = 0; m[10] = 0; m[14] = 0;
        m[3] = 0; m[7] = 0; m[11] = 0; m[15] = 1;
        break;
    case 0x8516: // -X
        m[0] = 0; m[4] = 0; m[8] = 1; m[12] = 0;
        m[1] = 0; m[5] = -1; m[9] = 0; m[13] = 0;
        m[2] = 1; m[6] = 0; m[10] = 0; m[14] = 0;
        m[3] = 0; m[7] = 0; m[11] = 0; m[15] = 1;
        break;
    case 0x8517: // +Y
        m[0] = 1; m[4] = 0; m[8] = 0; m[12] = 0;
        m[1] = 0; m[5] = 0; m[9] = 1; m[13] = 0;
        m[2] = 0; m[6] = -1; m[10] = 0; m[14] = 0;
        m[3] = 0; m[7] = 0; m[11] = 0; m[15] = 1;
        break;
    case 0x8518: // -Y
        m[0] = 1; m[4] = 0; m[8] = 0; m[12] = 0;
        m[1] = 0; m[5] = 0; m[9] = -1; m[13] = 0;
        m[2] = 0; m[6] = 1; m[10] = 0; m[14] = 0;
        m[3] = 0; m[7] = 0; m[11] = 0; m[15] = 1;
        break;
    case 0x8519: // +Z
        m[0] = 1; m[4] = 0; m[8] = 0; m[12] = 0;
        m[1] = 0; m[5] = -1; m[9] = 0; m[13] = 0;
        m[2] = 0; m[6] = 0; m[10] = -1; m[14] = 0;
        m[3] = 0; m[7] = 0; m[11] = 0; m[15] = 1;
        break;
    case 0x851A: // -Z
        m[0] = -1; m[4] = 0; m[8] = 0; m[12] = 0;
        m[1] = 0; m[5] = -1; m[9] = 0; m[13] = 0;
        m[2] = 0; m[6] = 0; m[10] = 1; m[14] = 0;
        m[3] = 0; m[7] = 0; m[11] = 0; m[15] = 1;
        break;
    }

    return m;
}

void GIMapper::Bind(Shader *shader)
{
    shader->SetUniform("VoxelProbePosition", m_bounds.GetCenter());
    shader->SetUniform("VoxelSceneScale", m_bounds.GetDimensions());

    Vector3 scale = Vector3(1.0f) / Vector3::Max(m_bounds.GetDimensions(), 0.0001f);
    float voxel_scale = MathUtil::Min(scale.x, MathUtil::Min(scale.y, scale.z));
    Vector3 bias = -m_bounds.GetMin();
    Transform world_to_voxel(Vector3(0), Vector3(voxel_scale), Quaternion::Identity());
    world_to_voxel *= Transform (bias, Vector3(1), Quaternion::Identity());
    shader->SetUniform("WorldToVoxel", world_to_voxel.GetMatrix());

    Transform world_to_unit_cube;
    world_to_unit_cube.SetTranslation(-1.0f);
    world_to_unit_cube.SetScale(2.0f);
    world_to_unit_cube *= world_to_voxel;

    for (int i = 0; i < 6; i++) {

        Matrix4 unit_cube_to_ndc = cubemapmatrix(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i);

        Matrix4 world_to_ndc = unit_cube_to_ndc * world_to_unit_cube.GetMatrix();

        Matrix4 ndc_to_tex = Matrix4(unit_cube_to_ndc).Transpose();

        m_storage_transform_matrices[i] = std::make_pair(world_to_ndc, ndc_to_tex);
    }

    //SetUniform("StorageTransformMatrix", ndc_to_tex);
}

void GIMapper::Render(Renderer *renderer, Camera *cam)
{
    if (m_is_first_run) {
        if (!Environment::GetInstance()->VCTEnabled()) {
            return;
        }

        for (int i = 0; i < m_cameras.size(); i++) {
            GIMapperCamera *gi_cam = m_cameras[i];
            gi_cam->GetShader()->SetUniform("WorldToNdcMatrix", m_storage_transform_matrices[i].first);
            gi_cam->GetShader()->SetUniform("StorageTransformMatrix", m_storage_transform_matrices[i].second);
            gi_cam->Render(renderer, cam);
        }

        m_is_first_run = false;

        return;
    }

    if (m_render_tick < 1.0) {
        return;
    }

    m_render_tick = 0.0;

    if (!Environment::GetInstance()->VCTEnabled()) {
        return;
    }

    m_cameras[m_render_index]->Render(renderer, cam);

    m_render_index++;
    m_render_index = m_render_index % m_cameras.size();
}
} // namespace apex