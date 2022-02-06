#include "mesh.h"
#include "../gl_util.h"

namespace hyperion {

const Mesh::MeshAttribute Mesh::MeshAttribute::Positions = {0, 3, 0 };
const Mesh::MeshAttribute Mesh::MeshAttribute::Normals = { 0, 3, 1 };
const Mesh::MeshAttribute Mesh::MeshAttribute::TexCoords0 = { 0, 2, 2 };
const Mesh::MeshAttribute Mesh::MeshAttribute::TexCoords1 = { 0, 2, 3 };
const Mesh::MeshAttribute Mesh::MeshAttribute::Tangents = { 0, 3, 4 };
const Mesh::MeshAttribute Mesh::MeshAttribute::Bitangents = { 0, 3, 5 };
const Mesh::MeshAttribute Mesh::MeshAttribute::BoneWeights = { 0, 4, 6 };
const Mesh::MeshAttribute Mesh::MeshAttribute::BoneIndices = { 0, 4, 7 };

Mesh::Mesh()
    : Renderable()
{
    SetAttribute(ATTR_POSITIONS, MeshAttribute::Positions);
    SetPrimitiveType(PRIM_TRIANGLES);
    is_uploaded = false;
    is_created = false;
}

Mesh::~Mesh()
{
    if (is_created) {
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
        glDeleteBuffers(1, &ibo);
    }

    is_uploaded = false;
    is_created = false;
}

void Mesh::SetVertices(const std::vector<Vertex> &verts)
{
    vertices = verts;
    indices.clear();
    for (size_t i = 0; i < verts.size(); i++) {
        indices.push_back(static_cast<MeshIndex>(i));
    }

    // update the aabb
    // TODO: more concrete (virtual method?) way of setting aabb on Renderable
    m_aabb.Clear();
    for (Vertex &vertex : vertices) {
        m_aabb.Extend(vertex.GetPosition());
    }

    is_uploaded = false;
}

void Mesh::SetVertices(const std::vector<Vertex> &verts, const std::vector<MeshIndex> &ind)
{
    vertices = verts;
    indices = ind;

    // update the aabb
    m_aabb.Clear();
    for (Vertex &vertex : vertices) {
        m_aabb.Extend(vertex.GetPosition());
    }

    is_uploaded = false;
}

void Mesh::SetTriangles(const std::vector<Triangle> &triangles)
{
    std::vector<Vertex> verts;
    verts.resize(triangles.size() * 3);

    for (size_t i = 0; i < triangles.size(); i++) {
        for (int j = 0; j < 3; j++) {
            verts[i] = triangles[i][j];
        }
    }

    SetVertices(verts);
}

void Mesh::SetAttribute(MeshAttributeType type, const MeshAttribute &attribute)
{
    attribs[type] = attribute;
    is_uploaded = false;
}

std::vector<float> Mesh::CreateBuffer()
{
    unsigned int vert_size = 0, prev_size = 0, offset = 0;

    for (auto &&attr : attribs) {
        offset += prev_size;
        attr.second.offset = offset;
        prev_size = attr.second.size;
        vert_size += prev_size;
    }

    vertex_size = vert_size;

    std::vector<float> buffer(vert_size * vertices.size());

    auto pos_it = attribs.find(ATTR_POSITIONS);
    auto norm_it = attribs.find(ATTR_NORMALS);
    auto tc0_it = attribs.find(ATTR_TEXCOORDS0);
    auto tc1_it = attribs.find(ATTR_TEXCOORDS1);
    auto tan_it = attribs.find(ATTR_TANGENTS);
    auto bitan_it = attribs.find(ATTR_BITANGENTS);
    auto bonew_it = attribs.find(ATTR_BONEWEIGHTS);
    auto bonei_it = attribs.find(ATTR_BONEINDICES);

    for (size_t i = 0; i < vertices.size(); i++) {
        auto &vertex = vertices[i];

        if (pos_it != attribs.end()) {
            buffer[(i * vert_size) + pos_it->second.offset] = vertex.GetPosition().x;
            buffer[(i * vert_size) + pos_it->second.offset + 1] = vertex.GetPosition().y;
            buffer[(i * vert_size) + pos_it->second.offset + 2] = vertex.GetPosition().z;
        }
        if (norm_it != attribs.end()) {
            buffer[(i * vert_size) + norm_it->second.offset] = vertex.GetNormal().x;
            buffer[(i * vert_size) + norm_it->second.offset + 1] = vertex.GetNormal().y;
            buffer[(i * vert_size) + norm_it->second.offset + 2] = vertex.GetNormal().z;
        }
        if (tc0_it != attribs.end()) {
            buffer[(i * vert_size) + tc0_it->second.offset] = vertex.GetTexCoord0().x;
            buffer[(i * vert_size) + tc0_it->second.offset + 1] = vertex.GetTexCoord0().y;
        }
        if (tc1_it != attribs.end()) {
            buffer[(i * vert_size) + tc1_it->second.offset] = vertex.GetTexCoord1().x;
            buffer[(i * vert_size) + tc1_it->second.offset + 1] = vertex.GetTexCoord1().y;
        }
        if (tan_it != attribs.end()) {
            buffer[(i * vert_size) + tan_it->second.offset] = vertex.GetTangent().x;
            buffer[(i * vert_size) + tan_it->second.offset + 1] = vertex.GetTangent().y;
            buffer[(i * vert_size) + tan_it->second.offset + 2] = vertex.GetTangent().z;
        }
        if (bitan_it != attribs.end()) {
            buffer[(i * vert_size) + bitan_it->second.offset] = vertex.GetBitangent().x;
            buffer[(i * vert_size) + bitan_it->second.offset + 1] = vertex.GetBitangent().y;
            buffer[(i * vert_size) + bitan_it->second.offset + 2] = vertex.GetBitangent().z;
        }
        if (bonei_it != attribs.end()) {
            buffer[(i * vert_size) + bonei_it->second.offset] = static_cast<float>(vertex.GetBoneIndex(0));
            buffer[(i * vert_size) + bonei_it->second.offset + 1] = static_cast<float>(vertex.GetBoneIndex(1));
            buffer[(i * vert_size) + bonei_it->second.offset + 2] = static_cast<float>(vertex.GetBoneIndex(2));
            buffer[(i * vert_size) + bonei_it->second.offset + 3] = static_cast<float>(vertex.GetBoneIndex(3));
        }
        if (bonew_it != attribs.end()) {
            buffer[(i * vert_size) + bonew_it->second.offset] = vertex.GetBoneWeight(0);
            buffer[(i * vert_size) + bonew_it->second.offset + 1] = vertex.GetBoneWeight(1);
            buffer[(i * vert_size) + bonew_it->second.offset + 2] = vertex.GetBoneWeight(2);
            buffer[(i * vert_size) + bonew_it->second.offset + 3] = vertex.GetBoneWeight(3);
        }
    }

    return buffer;
}

void Mesh::CalculateTangents()
{
    Vertex *v[3];
    Vector2 uv[3];

    for (size_t i = 0; i < indices.size(); i += 3) {
        for (int j = 0; j < 3; j++) {
            v[j] = &vertices[indices[i + j]];
            uv[j] = v[j]->GetTexCoord0();
        }

        Vector3 edge1 = v[1]->GetPosition() - v[0]->GetPosition();
        Vector3 edge2 = v[2]->GetPosition() - v[0]->GetPosition();

        Vector2 edge1uv = uv[1] - uv[0];
        Vector2 edge2uv = uv[2] - uv[0];

        const float cp = edge1uv.x * edge2uv.y - edge1uv.y * edge2uv.x;

        if (cp != 0.0f) {
            const float mul = 1.0f / cp;

            Vector3 tangent;
            tangent.x = edge2uv.y * edge1.x - edge1uv.y * edge2.x;
            tangent.y = edge2uv.y * edge1.y - edge1uv.y * edge2.y;
            tangent.z = edge2uv.y * edge1.z - edge1uv.y * edge2.z;
            tangent *= mul;
            tangent.Normalize();

            Vector3 bitangent;

            bitangent.x = -edge2uv.x * edge1.x + edge1uv.x * edge2.x;
            bitangent.y = -edge2uv.x * edge1.y + edge1uv.x * edge2.y;
            bitangent.z = -edge2uv.x * edge1.z + edge1uv.x * edge2.z;
            bitangent *= mul;
            bitangent.Normalize();

            for (int j = 0; j < 3; j++) {
                v[j]->SetTangent(tangent);
                v[j]->SetBitangent(bitangent);
            }
        }
    }

    SetAttribute(ATTR_TANGENTS, MeshAttribute::Tangents);
    SetAttribute(ATTR_BITANGENTS, MeshAttribute::Bitangents);
}

void Mesh::Render(Renderer *renderer, Camera *cam)
{
    if (!is_created) {
        glGenVertexArrays(1, &vao);
        CatchGLErrors("Failed to generate vertex arrays.");

        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ibo);

        is_created = true;
    }

    glBindVertexArray(vao);

    if (!is_uploaded) {
        std::vector<float> buffer = CreateBuffer();

        if (buffer.empty()) {
            is_uploaded = true;

            soft_assert_msg(false, "CreateBuffer() returned no data");
        }

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, buffer.size() * sizeof(float), &buffer[0], GL_STATIC_DRAW);
        CatchGLErrors("Failed to set buffer data.");

        unsigned int error;

        for (auto &&attr : attribs) {
            glEnableVertexAttribArray(attr.second.index);
            CatchGLErrors("Failed to enable vertex attribute array." __FILE__);

            glVertexAttribPointer(attr.second.index, attr.second.size, GL_FLOAT,
                false, vertex_size * sizeof(float), (void*)(attr.second.offset * sizeof(float)));

            CatchGLErrors("Failed to set vertex attribute pointer.");
        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(MeshIndex), &indices[0], GL_STATIC_DRAW);

        is_uploaded = true;
    }

    glDrawElements(primitive_type, indices.size(), GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
}

bool Mesh::IntersectRay(const Ray &ray, const Transform &transform, RaytestHit &out) const
{
    if (primitive_type != PRIM_TRIANGLES) {
        // fall back to aabb test
        return Renderable::IntersectRay(ray, transform, out);
    }

    for (size_t i = 0; i < indices.size(); i += 3) {
        Triangle t(
            vertices[indices[i]].GetPosition(),
            vertices[indices[i + 1]].GetPosition(),
            vertices[indices[i + 2]].GetPosition()
        );

        t *= transform;

        if (t.IntersectRay(ray, out)) {
            return true;
        }
    }

    return false;
}

bool Mesh::IntersectRay(const Ray &ray, const Transform &transform, RaytestHitList_t &out) const
{
    bool intersected = false;

    if (primitive_type != PRIM_TRIANGLES) {
        // fall back to aabb test
        return Renderable::IntersectRay(ray, transform, out);
    }

    for (size_t i = 0; i < indices.size(); i += 3) {
        Triangle t(
            vertices[indices[i]].GetPosition(),
            vertices[indices[i + 1]].GetPosition(),
            vertices[indices[i + 2]].GetPosition()
        );

        t *= transform;

        RaytestHit intersection;

        if (t.IntersectRay(ray, intersection)) {
            intersected = true;

            out.push_back(intersection);
        }
    }

    return intersected;
}

void Mesh::CalculateNormals()
{
    // reset to zero initially
    for (Vertex &vert : vertices) {
        vert.SetNormal(Vector3::Zero());
    }

    for (size_t i = 0; i < indices.size(); i += 3) {
        MeshIndex i0 = indices[i];
        MeshIndex i1 = indices[i + 1];
        MeshIndex i2 = indices[i + 2];

        const Vector3 &p0 = vertices[i0].GetPosition();
        const Vector3 &p1 = vertices[i1].GetPosition();
        const Vector3 &p2 = vertices[i2].GetPosition();

        Vector3 u = p2 - p0;
        Vector3 v = p1 - p0;
        Vector3 n = v;

        n.Cross(u);
        n.Normalize();

        vertices[i0].SetNormal(vertices[i0].GetNormal() + n);
        vertices[i1].SetNormal(vertices[i1].GetNormal() + n);
        vertices[i2].SetNormal(vertices[i2].GetNormal() + n);
    }

    for (Vertex &vert : vertices) {
        Vector3 tmp(vert.GetNormal());
        tmp.Normalize();
        vert.SetNormal(tmp);
    }

    SetAttribute(ATTR_NORMALS, MeshAttribute::Normals);
}


void Mesh::InvertNormals()
{
    for (Vertex &vert : vertices) {
        vert.SetNormal(vert.GetNormal() * -1);
    }
}

} // namespace hyperion
