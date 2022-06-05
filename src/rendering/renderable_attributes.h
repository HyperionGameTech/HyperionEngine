#ifndef HYPERION_V2_RENDERABLE_ATTRIBUTES_H
#define HYPERION_V2_RENDERABLE_ATTRIBUTES_H

#include <rendering/backend/renderer_structs.h>
#include <rendering/render_bucket.h>
#include <rendering/shader.h>

#include <util/defines.h>

namespace hyperion::v2 {

using renderer::FaceCullMode;
using renderer::Topology;
using renderer::FillMode;
using renderer::StencilState;

struct RenderableAttributeSet {
    Bucket             bucket{Bucket::BUCKET_OPAQUE};
    Shader::ID         shader_id{Shader::empty_id};
    VertexAttributeSet vertex_attributes;
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

    HYP_DEF_STRUCT_COMPARE_EQL(RenderableAttributeSet);

    bool operator<(const RenderableAttributeSet &other) const
    {
         return ToTuple() < other.ToTuple();
    }
};


} // namespace hyperion::v2

#endif
