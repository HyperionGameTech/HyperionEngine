#include "bounding_box_renderer.h"
#include "../shaders/shader_code.h"

namespace hyperion {
const std::vector<MeshIndex> BoundingBoxRenderer::indices = {
    0, 1, 1, 2, 2, 3,
    3, 0, 0, 4, 4, 5,
    5, 3, 5, 6, 6, 7,
    4, 7, 7, 1, 6, 2
};

BoundingBoxRenderer::BoundingBoxRenderer()
    : Renderable(fbom::FBOMObjectType("BOUNDING_BOX_RENDERER"), RenderBucket::RB_DEBUG),
      m_mesh(new Mesh)
{
    m_vertices.resize(8);

    m_mesh->SetPrimitiveType(Mesh::PRIM_LINES);

    m_shader.reset(new Shader(ShaderProperties(), ShaderCode::aabb_debug_vs, ShaderCode::aabb_debug_fs));
}

BoundingBoxRenderer::~BoundingBoxRenderer()
{
    delete m_mesh;
}

void BoundingBoxRenderer::UpdateVertices()
{
    auto corners = m_aabb.GetCorners();

    for (int i = 0; i < corners.size(); i++) {
        m_vertices[i].SetPosition(corners[i]);
    }
}

void BoundingBoxRenderer::Render(Renderer *renderer, Camera *cam)
{
    UpdateVertices();

    m_shader->Use();
    m_mesh->SetVertices(m_vertices, BoundingBoxRenderer::indices);
    m_mesh->Render(renderer, cam);
    m_shader->End();
}

std::shared_ptr<Renderable> BoundingBoxRenderer::CloneImpl()
{
    auto clone = std::make_shared<BoundingBoxRenderer>();

    clone->m_vertices = m_vertices;
    clone->m_aabb = m_aabb;

    // TODO: Mesh?

    return clone;
}
} // namespace hyperion
