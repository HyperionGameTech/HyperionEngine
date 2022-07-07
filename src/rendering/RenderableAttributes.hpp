#ifndef HYPERION_V2_RENDERABLE_ATTRIBUTES_H
#define HYPERION_V2_RENDERABLE_ATTRIBUTES_H

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/RenderBucket.hpp>
#include <rendering/Shader.hpp>

#include <util/Defines.hpp>

namespace hyperion::v2 {

using renderer::FaceCullMode;
using renderer::Topology;
using renderer::FillMode;
using renderer::StencilState;

struct RenderableAttributeSet {
    Bucket             bucket{Bucket::BUCKET_OPAQUE};
    Shader::ID         shader_id{Shader::empty_id};
    VertexAttributeSet vertex_attributes{renderer::static_mesh_vertex_attributes};
    Topology           topology{Topology::TRIANGLES};
    FillMode           fill_mode{FillMode::FILL};
    FaceCullMode       cull_faces{FaceCullMode::BACK};
    bool               alpha_blending{false};
    bool               depth_write{true};
    bool               depth_test{true};
    StencilState       stencil_state{};

    auto ToTuple() const
    {
        return std::tie(
            bucket,
            shader_id,
            vertex_attributes,
            topology,
            fill_mode,
            cull_faces,
            alpha_blending,
            depth_write,
            depth_test,
            stencil_state
        );
    }

    bool operator==(const RenderableAttributeSet &other) const
    {
        return bucket == other.bucket
            && shader_id == other.shader_id
            && vertex_attributes == other.vertex_attributes
            && topology == other.topology
            && fill_mode == other.fill_mode
            && cull_faces == other.cull_faces
            && alpha_blending == other.alpha_blending
            && depth_write == other.depth_write
            && depth_test == other.depth_test
            && stencil_state == other.stencil_state;
    }

    bool operator<(const RenderableAttributeSet &other) const
    {
         return ToTuple() < other.ToTuple();
    }
};


} // namespace hyperion::v2

#endif
