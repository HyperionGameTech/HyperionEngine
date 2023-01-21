#ifndef HYPERION_V2_RENDERABLE_ATTRIBUTES_H
#define HYPERION_V2_RENDERABLE_ATTRIBUTES_H

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/RenderBucket.hpp>
#include <rendering/Shader.hpp>

#include <util/Defines.hpp>
#include <Types.hpp>
#include <HashCode.hpp>

namespace hyperion::v2 {

using renderer::FaceCullMode;
using renderer::Topology;
using renderer::FillMode;
using renderer::StencilState;
using renderer::VertexAttributeSet;
using renderer::BlendMode;

class Framebuffer;

struct MaterialAttributes
{
    using MaterialFlags = UInt32;

    enum MaterialFlagBits : MaterialFlags
    {
        RENDERABLE_ATTRIBUTE_FLAGS_NONE = 0x0,
        RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_WRITE = 0x1,
        RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_TEST = 0x2
    };

    Bucket bucket = Bucket::BUCKET_OPAQUE;
    FillMode fill_mode = FillMode::FILL;
    BlendMode blend_mode = BlendMode::NONE;
    FaceCullMode cull_faces = FaceCullMode::BACK;
    MaterialFlags flags = RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_WRITE | RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_TEST;

    auto ToTuple() const
    {
        return std::make_tuple(
            bucket,
            fill_mode,
            blend_mode,
            cull_faces,
            flags
        );
    }

    bool operator==(const MaterialAttributes &other) const
    {
        return bucket == other.bucket
            && fill_mode == other.fill_mode
            && blend_mode == other.blend_mode
            && cull_faces == other.cull_faces
            && flags == other.flags;
    }

    bool operator<(const MaterialAttributes &other) const
    {
         return ToTuple() < other.ToTuple();
    }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(bucket);
        hc.Add(fill_mode);
        hc.Add(blend_mode);
        hc.Add(cull_faces);
        hc.Add(flags);

        return hc;
    }
};

struct MeshAttributes
{
    VertexAttributeSet vertex_attributes { renderer::static_mesh_vertex_attributes };
    Topology topology { Topology::TRIANGLES };

    auto ToTuple() const
    {
        return std::make_tuple(
            vertex_attributes,
            topology
        );
    }

    bool operator==(const MeshAttributes &other) const
    {
        return vertex_attributes == other.vertex_attributes
            && topology == other.topology;
    }

    bool operator<(const MeshAttributes &other) const
    {
         return ToTuple() < other.ToTuple();
    }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(vertex_attributes);
        hc.Add(topology);

        return hc;
    }
};

struct RenderableAttributeSet
{
    ShaderDefinition shader_def;
    ID<Framebuffer> framebuffer_id; // only used for scenes, not per entity
    MeshAttributes mesh_attributes;
    MaterialAttributes material_attributes;
    StencilState stencil_state { };
    UInt32 override_flags;

    RenderableAttributeSet(
        const MeshAttributes &mesh_attributes = { },
        const MaterialAttributes &material_attributes = { },
        const ShaderDefinition &shader_def = { },
        UInt32 override_flags = 0
    ) : mesh_attributes(mesh_attributes),
        material_attributes(material_attributes),
        shader_def(shader_def),
        override_flags(override_flags)
    {
    }

    RenderableAttributeSet(const RenderableAttributeSet &other) = default;
    RenderableAttributeSet &operator=(const RenderableAttributeSet &other) = default;
    RenderableAttributeSet(RenderableAttributeSet &&other) noexcept = default;
    RenderableAttributeSet &operator=(RenderableAttributeSet &&other) noexcept = default;
    ~RenderableAttributeSet() = default;

    bool operator==(const RenderableAttributeSet &other) const
    {
        return GetHashCode() == other.GetHashCode();
    }

    bool operator<(const RenderableAttributeSet &other) const
    {
         return GetHashCode().Value() < other.GetHashCode().Value();
    }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(shader_def.GetHashCode());
        hc.Add(framebuffer_id.GetHashCode());
        hc.Add(mesh_attributes.GetHashCode());
        hc.Add(material_attributes.GetHashCode());
        hc.Add(stencil_state.GetHashCode());
        hc.Add(override_flags);

        return hc;
    }
};


} // namespace hyperion::v2

#endif
