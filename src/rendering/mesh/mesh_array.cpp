#include "mesh_array.h"

#include "../../util/mesh_factory.h"

namespace hyperion {
MeshArray::MeshArray()
    : Renderable(fbom::FBOMObjectType("MESH_ARRAY"))
{
}

void MeshArray::Render(Renderer *renderer, Camera *cam)
{
    for (auto &submesh : m_submeshes) {
        submesh.mesh->Render(renderer, cam);
    }
}

void MeshArray::AddSubmesh(const Submesh &submesh)
{
    m_submeshes.push_back(submesh);

    UpdateSubmeshes();
}

void MeshArray::ClearSubmeshes()
{
    m_submeshes.clear();
}

void MeshArray::UpdateSubmeshes()
{
    if (m_submeshes.empty()) {
        return;
    }

    if (m_submeshes[0].mesh != nullptr) {
        SetRenderBucket(m_submeshes[0].mesh->GetRenderBucket());
    }

    ApplyTransforms();
}

void MeshArray::ApplyTransforms()
{
    for (auto &submesh : m_submeshes) {
        submesh.mesh = MeshFactory::TransformMesh(
            submesh.mesh,
            submesh.transform
        );

        // reset transform
        submesh.transform = Transform();
    }
}

void MeshArray::Optimize()
{
    if (m_submeshes.size() <= 1) {
        return;
    }

    for (size_t i = 1; i < m_submeshes.size(); i++) {
        m_submeshes[0].mesh = MeshFactory::MergeMeshes(
            m_submeshes[0].mesh,
            m_submeshes[i].mesh,
            m_submeshes[0].transform,
            m_submeshes[i].transform
        );
    }

    m_submeshes.resize(1);
}

std::shared_ptr<Renderable> MeshArray::CloneImpl()
{
    auto clone = std::make_shared<MeshArray>();

    for (const auto &submesh : m_submeshes) {
        soft_assert_continue(submesh.mesh != nullptr);

        Submesh submesh_clone;
        submesh_clone.mesh = std::dynamic_pointer_cast<Mesh>(submesh.mesh->Clone());
        submesh_clone.transform = submesh.transform;

        clone->m_submeshes.push_back(submesh_clone);
    }

    return clone;
}
} // namespace hyperion
