#ifndef MESH_H
#define MESH_H

#include "renderable.h"
#include "../math/triangle.h"
#include "../math/vertex.h"

#include <vector>
#include <map>

#include <cstddef>
#include <cstdint>
#include <bitset>

namespace hyperion {

typedef uint32_t MeshIndex;

class Renderer;

class Mesh : public Renderable {
public:
    enum PrimitiveType {
        PRIM_POINTS = 0x0000,
        PRIM_LINES,
        PRIM_LINE_LOOP,
        PRIM_LINE_STRIP,
        PRIM_TRIANGLES,
        PRIM_TRIANGLE_STRIP,
        PRIM_TRIANGLE_FAN
    };

    enum MeshAttributeType {
        ATTR_POSITIONS = 0x01,
        ATTR_NORMALS = 0x02,
        ATTR_TEXCOORDS0 = 0x04,
        ATTR_TEXCOORDS1 = 0x08,
        ATTR_TANGENTS = 0x10,
        ATTR_BITANGENTS = 0x20,
        ATTR_BONEWEIGHTS = 0x40,
        ATTR_BONEINDICES = 0x80,

        // ...

        ATTR_MAX = 0x400
    };

    struct MeshAttribute {
        using MeshAttributeField_t = uint64_t;

        static const MeshAttribute Positions, Normals,
            TexCoords0, TexCoords1,
            Tangents, Bitangents,
            BoneWeights, BoneIndices;

        MeshAttributeField_t offset, size, index;

        MeshAttribute()
            : offset(0), size(0), index(0)
        {
        }

        MeshAttribute(MeshAttributeField_t offset, MeshAttributeField_t size, MeshAttributeField_t index)
            : offset(offset), size(size), index(index)
        {
        }

        MeshAttribute(const MeshAttribute &other)
            : offset(other.offset), size(other.size), index(other.index)
        {
        }

        bool operator==(const MeshAttribute &other) const
        {
            return offset == other.offset &&
                size == other.size &&
                index == other.index;
        }
    };

    Mesh();
    Mesh(const Mesh &other) = delete;
    Mesh &operator=(const Mesh &other) = delete;
    virtual ~Mesh();

    void SetVertices(const std::vector<Vertex> &verts);
    void SetVertices(const std::vector<Vertex> &verts, const std::vector<MeshIndex> &ind);
    inline const std::vector<Vertex> &GetVertices() const { return vertices; }
    inline const std::vector<MeshIndex> &GetIndices() const { return indices; }
    void SetTriangles(const std::vector<Triangle> &triangles);

    void SetAttribute(MeshAttributeType type, const MeshAttribute &attribute);
    inline const std::map<MeshAttributeType, MeshAttribute> &GetAttributes() const { return attribs; }
    inline bool HasAttribute(MeshAttributeType type) const { return attribs.find(type) != attribs.end(); }
    inline void SetPrimitiveType(PrimitiveType prim_type) { primitive_type = prim_type; }
    inline PrimitiveType GetPrimitiveType() const { return primitive_type; }

    virtual bool IntersectRay(const Ray &ray, const Transform &transform, RaytestHit &out) const override;
    virtual bool IntersectRay(const Ray &ray, const Transform &transform, RaytestHitList_t &out) const override;

    void CalculateNormals();
    void InvertNormals();
    void CalculateTangents();

    void Render(Renderer *renderer, Camera *cam);


#pragma region serialization
    FBOM_DEF_DESERIALIZER(loader, in, out) {
        using namespace fbom;

        auto out_mesh = std::make_shared<Mesh>();
        out = out_mesh;

        in->GetProperty("bucket").ReadInt((int32_t*)&out_mesh->m_bucket);
        in->GetProperty("primitive_type").ReadInt((int32_t*)&out_mesh->primitive_type);
        in->GetProperty("vertex_size").ReadUnsignedInt(&out_mesh->vertex_size);

        // read attribute bitflag
        int32_t num_attributes;
        
        if (auto err = in->GetProperty("num_attributes").ReadInt(&num_attributes)) {
            return err;
        }

        if (num_attributes > 8) {
            return FBOMResult(FBOMResult::FBOM_ERR, "num_attributes greater than maximum number of possible attributes");
        }

        uint64_t attributes_bitflag;
        if (auto err = in->GetProperty("attributes").ReadUnsignedLong(&attributes_bitflag)) {
            return err;
        }

        {
            // now read each attributes' data
            size_t total_size = in->GetProperty("attribute_data").TotalSize();
            size_t attributes_size_bytes = num_attributes * sizeof(MeshAttribute);

            if (total_size != attributes_size_bytes) {
                return FBOMResult(FBOMResult::FBOM_ERR, std::string("expected total size of attribute_data field to be equal to num attribute * sizeof MeshAttribute (") + std::to_string(total_size) + " != " + std::to_string(attributes_size_bytes));
            }

            MeshAttribute *mesh_attributes = new MeshAttribute[num_attributes];

            in->GetProperty("attribute_data").ReadBytes(attributes_size_bytes, (unsigned char*)mesh_attributes);

            // now set mapped data on the mesh
            uint64_t mask = MeshAttributeType::ATTR_MAX;

            for (int i = num_attributes - 1; i >= 0 && mask != 0; mask >>= 1) {
                if (attributes_bitflag & mask) {
                    out_mesh->attribs[MeshAttributeType(mask)] = mesh_attributes[i];

                    i--;
                }
            }

            delete[] mesh_attributes;

            // sanity check before reading vertex data

            size_t stored_vertex_size = out_mesh->vertex_size;
            out_mesh->CalculateVertexSize();

            if (out_mesh->vertex_size != stored_vertex_size) {
                return FBOMResult(FBOMResult::FBOM_ERR, std::string("stored vertex_size value not equal to mesh calculated vertex size (") + std::to_string(stored_vertex_size) + " != " + std::to_string(out_mesh->vertex_size) + ")");
            }
        }
        
        // TODO: refactor mesh class so we can use SetVertices() in one shot rather than writing right into data
        {
            size_t float_buffer_size_bytes = in->GetProperty("vertices").TotalSize();
            size_t float_buffer_size = float_buffer_size_bytes / sizeof(float);
            size_t num_vertices = float_buffer_size / out_mesh->vertex_size;

            // sanity
            AssertThrowMsg(float_buffer_size % out_mesh->vertex_size == 0, "vertex_size does not evenly divide into float buffer size");

            std::vector<float> vertex_buffer;
            vertex_buffer.resize(float_buffer_size);

            in->GetProperty("vertices").ReadBytes(float_buffer_size_bytes, (unsigned char*)&vertex_buffer[0]);

            out_mesh->SetVerticesFromFloatBuffer(vertex_buffer);

            // size_t total_vertices_bytes = in->GetProperty("vertices").TotalSize();
            // size_t num_vertices = total_vertices_bytes / out_mesh->vertex_size;

            // out_mesh->vertices.resize(num_vertices);

            // in->GetProperty("vertices").ReadBytes(total_vertices_bytes, reinterpret_cast<unsigned char*>(out_mesh->vertices.data()));
        }

        {
            size_t total_indices_bytes = in->GetProperty("indices").TotalSize();
            size_t num_indices = total_indices_bytes / sizeof(MeshIndex);

            out_mesh->indices.resize(num_indices);

            in->GetProperty("indices").ReadBytes(total_indices_bytes, reinterpret_cast<unsigned char*>(out_mesh->indices.data()));
        }

        return FBOMResult::FBOM_OK;
    }

    FBOM_DEF_SERIALIZER(loader, in, out)
    {
        using namespace fbom;

        auto mesh = dynamic_cast<Mesh*>(in);

        if (mesh == nullptr) {
            return FBOMResult::FBOM_ERR;
        }

        { // write vertices from float buffer
            std::vector<float> float_buffer = mesh->CreateBuffer();

            out->SetProperty("vertices", FBOMArray(FBOMFloat(), float_buffer.size()), (void*)float_buffer.data());
        }

        { // write indices
            out->SetProperty("indices", FBOMArray(FBOMUnsignedInt(), mesh->indices.size()), (void*)mesh->indices.data());
        }

        { // write number of attributes
            int32_t num_attributes = mesh->attribs.size();

            out->SetProperty("num_attributes", FBOMInt(), &num_attributes);
        }

        uint64_t attributes_bitflag = 0;

        { // store attributes as bitflag

            for (auto &attrib : mesh->attribs) {
                attributes_bitflag |= attrib.first;
            }

            out->SetProperty("attributes", FBOMUnsignedLong(), &attributes_bitflag);
        }

        { // store attribute data as array of struct
            std::vector<MeshAttribute> attribute_data;
            attribute_data.reserve(mesh->attribs.size());

            for (auto &attrib : mesh->attribs) {
                attribute_data.push_back(attrib.second);
            }

            out->SetProperty("attribute_data", FBOMArray(FBOMStruct(sizeof(MeshAttribute)), attribute_data.size()), (void*)attribute_data.data());
        }

        out->SetProperty("bucket", FBOMInt(), (void*)&mesh->m_bucket);
        out->SetProperty("vertex_size", FBOMUnsignedInt(), (void*)&mesh->vertex_size);
        out->SetProperty("primitive_type", FBOMInt(), (void*)&mesh->primitive_type);

        return FBOMResult::FBOM_OK;
    }
#pragma endregion

protected:
    virtual std::shared_ptr<Renderable> CloneImpl() override;

private:
    bool is_uploaded, is_created;
    unsigned int vao, vbo, ibo, vertex_size;
    std::vector<Vertex> vertices;
    std::vector<MeshIndex> indices;
    PrimitiveType primitive_type;

    // map attribute to offset
    std::map<MeshAttributeType, MeshAttribute> attribs;

    // calculate size of a vertex in this mesh, using mesh attributes
    // note: also sets offset in attributes
    void CalculateVertexSize();

    std::vector<float> CreateBuffer();
    // use mesh attributes to determine vertices
    void SetVerticesFromFloatBuffer(const std::vector<float> &buffer);
};
} // namespace hyperion

#endif
