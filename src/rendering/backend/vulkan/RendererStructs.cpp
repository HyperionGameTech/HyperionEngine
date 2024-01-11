#include <rendering/backend/RendererStructs.hpp>

#include <utility>

namespace hyperion::renderer {

const decltype(VertexAttribute::mapping) VertexAttribute::mapping({
    { MESH_INPUT_ATTRIBUTE_POSITION,     VertexAttribute { .name = "a_position",     .location = 0, .binding = 0, .size = 3 * sizeof(float) } },
    { MESH_INPUT_ATTRIBUTE_NORMAL,       VertexAttribute { .name = "a_normal",       .location = 1, .binding = 0, .size = 3 * sizeof(float) } },
    { MESH_INPUT_ATTRIBUTE_TEXCOORD0,    VertexAttribute { .name = "a_texcoord0",    .location = 2, .binding = 0, .size = 2 * sizeof(float) } },
    { MESH_INPUT_ATTRIBUTE_TEXCOORD1,    VertexAttribute { .name = "a_texcoord1",    .location = 3, .binding = 0, .size = 2 * sizeof(float) } },
    { MESH_INPUT_ATTRIBUTE_TANGENT,      VertexAttribute { .name = "a_tangent",      .location = 4, .binding = 0, .size = 3 * sizeof(float) } },
    { MESH_INPUT_ATTRIBUTE_BITANGENT,    VertexAttribute { .name = "a_bitangent",    .location = 5, .binding = 0, .size = 3 * sizeof(float) } },
    { MESH_INPUT_ATTRIBUTE_BONE_INDICES, VertexAttribute { .name = "a_bone_weights", .location = 6, .binding = 0, .size = 4 * sizeof(float) } },
    { MESH_INPUT_ATTRIBUTE_BONE_WEIGHTS, VertexAttribute { .name = "a_bone_indices", .location = 7, .binding = 0, .size = 4 * sizeof(float) } }
});

} // hyperion::renderer