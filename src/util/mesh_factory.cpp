#include "mesh_factory.h"
#include "../math/math_util.h"
#include "../entity.h"
#include "../hash_code.h"

#include <unordered_map>

namespace hyperion {

std::shared_ptr<Mesh> MeshFactory::CreateQuad(bool triangle_fan)
{
    auto mesh = std::make_shared<Mesh>();

    std::vector<Vertex> vertices = {
        Vertex(
            Vector3(-1, -1, 0),
            Vector2(0, 0),
            Vector3(0, 0, 1)
            ),
        Vertex(
            Vector3(1, -1, 0),
            Vector2(1, 0),
            Vector3(0, 0, 1)
            ),
        Vertex(
            Vector3(1, 1, 0),
            Vector2(1, 1),
            Vector3(0, 0, 1)
            ),
        Vertex(
            Vector3(-1, 1, 0),
            Vector2(0, 1),
            Vector3(0, 0, 1)
            )
    };

    const std::vector<MeshIndex> quad_indices = {
        0, 3, 2,
        0, 2, 1
    };

    mesh->SetAttribute(Mesh::ATTR_TEXCOORDS0, Mesh::MeshAttribute::TexCoords0);
    mesh->SetAttribute(Mesh::ATTR_NORMALS, Mesh::MeshAttribute::Normals);

    if (triangle_fan) {
        mesh->SetVertices(vertices);
        mesh->SetPrimitiveType(Mesh::PrimitiveType::PRIM_TRIANGLE_FAN);
    } else {
        mesh->SetVertices(vertices, quad_indices);
        mesh->SetPrimitiveType(Mesh::PrimitiveType::PRIM_TRIANGLES);
    }

    return mesh;
}

std::shared_ptr<Mesh> MeshFactory::TransformMesh(const std::shared_ptr<Mesh> &mesh,
    const Transform &transform)
{
    auto new_mesh = std::make_shared<Mesh>();

    std::map<Mesh::MeshAttributeType, Mesh::MeshAttribute> all_mesh_attributes;

    all_mesh_attributes.insert(mesh->GetAttributes().begin(), mesh->GetAttributes().end());

    std::vector<Vertex> all_vertices = mesh->GetVertices();
    std::vector<MeshIndex> all_indices = mesh->GetIndices();

    for (Vertex &a_vertex : all_vertices) {
        a_vertex.SetPosition(a_vertex.GetPosition() * transform.GetMatrix());
        // a_vertex.SetNormal(a_vertex.GetNormal() * transform.GetMatrix());
        // a_vertex.SetTangent(a_vertex.GetTangent() * transform.GetMatrix());
        // a_vertex.SetBitangent(a_vertex.GetBitangent() * transform.GetMatrix());
    }

    for (auto it : all_mesh_attributes) {
        new_mesh->SetAttribute(it.first, it.second);
    }

    new_mesh->SetVertices(all_vertices, all_indices);
    new_mesh->SetPrimitiveType(mesh->GetPrimitiveType());

    return new_mesh;
}

std::shared_ptr<Mesh> MeshFactory::MergeMeshes(const std::shared_ptr<Mesh> &a,
    const std::shared_ptr<Mesh> &b,
    Transform transform_a,
    Transform transform_b)
{
    // TODO: raise error if primitive types differ
    std::shared_ptr<Mesh> new_mesh = std::make_shared<Mesh>(),
        a_transformed = TransformMesh(a, transform_a),
        b_transformed = TransformMesh(b, transform_b);

    std::map<Mesh::MeshAttributeType, Mesh::MeshAttribute> all_mesh_attributes;

    all_mesh_attributes.insert(a_transformed->GetAttributes().begin(), a_transformed->GetAttributes().end());
    all_mesh_attributes.insert(b_transformed->GetAttributes().begin(), b_transformed->GetAttributes().end());

    std::vector<Vertex> all_vertices;
    all_vertices.reserve(a_transformed->GetVertices().size() + b_transformed->GetVertices().size());

    std::vector<MeshIndex> all_indices;
    all_indices.reserve(a_transformed->GetIndices().size() + b_transformed->GetIndices().size());

    for (Vertex a_vertex : a_transformed->GetVertices()) {
        all_vertices.push_back(a_vertex);
    }

    for (MeshIndex a_index : a_transformed->GetIndices()) {
        all_indices.push_back(a_index);
    }

    const size_t b_index_offset = all_vertices.size();

    for (Vertex b_vertex : b_transformed->GetVertices()) {
        all_vertices.push_back(b_vertex);
    }

    for (MeshIndex b_index : b_transformed->GetIndices()) {
        all_indices.push_back(b_index_offset + b_index);
    }

    for (auto it : all_mesh_attributes) {
        new_mesh->SetAttribute(it.first, it.second);
    }

    new_mesh->SetVertices(all_vertices, all_indices);
    new_mesh->SetPrimitiveType(Mesh::PrimitiveType::PRIM_TRIANGLES);

    new_mesh->SetShader(b->GetShader()); // hmm..
    new_mesh->SetRenderBucket(b->GetRenderBucket());

    return new_mesh;
}

std::shared_ptr<Mesh> MeshFactory::MergeMeshes(const RenderableMesh_t &a, const RenderableMesh_t &b)
{
    return MergeMeshes(std::get<0>(a), std::get<0>(b), std::get<1>(a), std::get<1>(b));
}

std::shared_ptr<Mesh> MeshFactory::MergeMeshes(const std::vector<RenderableMesh_t> &meshes)
{
    std::shared_ptr<Mesh> mesh;

    for (auto &it : meshes) {
        if (mesh == nullptr) {
            mesh = std::make_shared<Mesh>();
        }

        mesh = MergeMeshes(std::make_tuple(mesh, Transform(), Material()), it);
    }

    return mesh;
}

std::vector<RenderableMesh_t> MeshFactory::MergeMeshesOnMaterial(const std::vector<RenderableMesh_t> &meshes)
{
    std::unordered_map<HashCode_t, RenderableMesh_t> renderable_map;

    for (auto &renderable : meshes) {
        Material material = std::get<2>(renderable);
        HashCode_t material_hash_code = material.GetHashCode().Value();

        auto it = renderable_map.find(material_hash_code);

        if (it == renderable_map.end()) {
            renderable_map[material_hash_code] = std::make_tuple(
                std::make_shared<Mesh>(),
                Transform(),
                Material()
            );
        }

        auto merged_mesh = MergeMeshes(
            renderable_map[material_hash_code],
            renderable
        );

        renderable_map[material_hash_code] = std::make_tuple(merged_mesh, Transform(), material);
    }

    std::vector<RenderableMesh_t> values;
    values.reserve(renderable_map.size());

    for (auto &it : renderable_map) {
        values.push_back(it.second);
    }

    return values;
}

std::shared_ptr<Mesh> MeshFactory::CreateCube(Vector3 offset)
{
    const std::vector<Transform> sides = {
        Transform(Vector3(0, 0, -1), Vector3::One(), Quaternion::Identity()),
        Transform(Vector3(0, 0, 1), Vector3::One(), Quaternion(Vector3::UnitY(), MathUtil::DegToRad(180.0f))),
        Transform(Vector3(1, 0, 0), Vector3::One(), Quaternion(Vector3::UnitY(), MathUtil::DegToRad(90.0f))),
        Transform(Vector3(-1, 0, 0), Vector3::One(), Quaternion(Vector3::UnitY() * -1, MathUtil::DegToRad(90.0f))),
        Transform(Vector3(0, 1, 0), Vector3::One(), Quaternion(Vector3::UnitX() * -1, MathUtil::DegToRad(90.0f))),
        Transform(Vector3(0, -1, 0), Vector3::One(), Quaternion(Vector3::UnitX(), MathUtil::DegToRad(90.0f)))
    };

    std::shared_ptr<Mesh> mesh;

    for (auto transform : sides) {
        if (mesh == nullptr) {
            mesh = std::make_shared<Mesh>();
        }

        mesh = MergeMeshes(
            mesh,
            CreateQuad(false),
            Transform(),
            transform
        );
    }

    // position it so that position is defined as the center of the cube.

    mesh = TransformMesh(
        mesh,
        Transform(
            offset,
            Vector3::One(),
            Quaternion::Identity()
        )
    );

    mesh->CalculateNormals();

    return mesh;
}

// https://www.danielsieger.com/blog/2021/03/27/generating-spheres.html
std::shared_ptr<Mesh> MeshFactory::CreateSphere(float radius, int num_slices, int num_stacks)
{
    std::vector<Vertex> vertices;
    std::vector<MeshIndex> indices;

    // top vertex
    vertices.push_back(Vertex(Vector3(0, 1, 0)));
    indices.push_back(0);
    MeshIndex v0 = indices.back(); 

    for (int i = 0; i < num_stacks - 1; i++) {
        auto phi = MathUtil::PI * double(i + 1) / double(num_stacks);

        for (int j = 0; j < num_slices; j++) {
            double theta = 2.0 * MathUtil::PI * double(j) / double(num_slices);
            double x = std::sin(phi) * std::cos(theta);
            double y = std::cos(phi);
            double z = std::sin(phi) * std::sin(theta);

            vertices.push_back(Vertex(Vector3(x, y, z)));
            // indices.push_back(indices.size());
        }
    }

    // bottom vertex
    vertices.push_back(Vertex(Vector3(0, -1, 0)));
    indices.push_back(indices.size());
    MeshIndex v1 = indices.back();

    // add top / bottom triangles
    for (int i = 0; i < num_slices; ++i) {
        int i0 = i + 1;
        int i1 = (i + 1) % num_slices + 1;

        indices.push_back(v0);
        indices.push_back(i1);
        indices.push_back(i0);

        i0 = i + num_slices * (num_stacks - 2) + 1;
        i1 = (i + 1) % num_slices + num_slices * (num_stacks - 2) + 1;

        indices.push_back(v1);
        indices.push_back(i0);
        indices.push_back(i1);
    }

    // add quads per stack / slice
    for (int j = 0; j < num_stacks - 2; j++)
    {
        auto j0 = j * num_slices + 1;
        auto j1 = (j + 1) * num_slices + 1;
        for (int i = 0; i < num_slices; i++) {
            auto i0 = j0 + i;
            auto i1 = j0 + (i + 1) % num_slices;
            auto i2 = j1 + (i + 1) % num_slices;
            auto i3 = j1 + i;

            indices.push_back(i0);
            indices.push_back(i3);
            indices.push_back(i2);
            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);
        }
    }

    std::shared_ptr<Mesh> mesh = std::make_shared<Mesh>();
    mesh->SetVertices(vertices, indices);

    return mesh;
}

std::vector<RenderableMesh_t> MeshFactory::GatherMeshes(Entity *entity)
{
    AssertThrow(entity != nullptr);

    entity->UpdateTransform();

    std::vector<RenderableMesh_t> meshes;
    meshes.reserve(10);

    if (auto mesh = std::dynamic_pointer_cast<Mesh>(entity->GetRenderable())) {
        meshes.push_back(std::make_tuple(
            mesh,
            entity->GetGlobalTransform(),
            entity->GetMaterial()
        ));
    }

    for (size_t i = 0; i < entity->NumChildren(); i++) {
        std::vector<RenderableMesh_t> sub_meshes = GatherMeshes(entity->GetChild(i).get());

        meshes.insert(meshes.end(), sub_meshes.begin(), sub_meshes.end());
    }

    return meshes;
}

} // namespace hyperion
