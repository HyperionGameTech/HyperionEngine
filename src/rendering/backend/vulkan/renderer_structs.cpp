#include "renderer_structs.h"

#include <utility>

namespace hyperion::renderer {

const decltype(VertexAttribute::mapping) VertexAttribute::mapping({
    std::make_pair(MESH_INPUT_ATTRIBUTE_POSITION,     VertexAttribute{.location = 0, .binding = 0, .size = 3 * sizeof(float)}),
    std::make_pair(MESH_INPUT_ATTRIBUTE_NORMAL,       VertexAttribute{.location = 1, .binding = 0, .size = 3 * sizeof(float)}),
    std::make_pair(MESH_INPUT_ATTRIBUTE_TEXCOORD0,    VertexAttribute{.location = 2, .binding = 0, .size = 2 * sizeof(float)}),
    std::make_pair(MESH_INPUT_ATTRIBUTE_TEXCOORD1,    VertexAttribute{.location = 3, .binding = 0, .size = 2 * sizeof(float)}),
    std::make_pair(MESH_INPUT_ATTRIBUTE_TANGENT,      VertexAttribute{.location = 4, .binding = 0, .size = 3 * sizeof(float)}),
    std::make_pair(MESH_INPUT_ATTRIBUTE_BITANGENT,    VertexAttribute{.location = 5, .binding = 0, .size = 3 * sizeof(float)}),
    std::make_pair(MESH_INPUT_ATTRIBUTE_BONE_INDICES, VertexAttribute{.location = 6, .binding = 0, .size = 4 * sizeof(float)}),
    std::make_pair(MESH_INPUT_ATTRIBUTE_BONE_WEIGHTS, VertexAttribute{.location = 7, .binding = 0, .size = 4 * sizeof(float)})
});

} // hyperion::renderer