#include "Vertex.hpp"

#include <script/ScriptApi.hpp>
#include <script/ScriptBindingDef.generated.hpp>

namespace hyperion {
bool Vertex::operator==(const Vertex &other) const
{
    return position == other.position
        && normal == other.normal
        && texcoord0 == other.texcoord0
        && texcoord1 == other.texcoord1
        && tangent == other.tangent
        && num_indices == other.num_indices
        && num_weights == other.num_weights
        && bone_weights == other.bone_weights
        && bone_indices == other.bone_indices;
}

// Vertex &Vertex::operator=(const Vertex &other)
// {
//     position = other.position;
//     normal = other.normal;
//     texcoord0 = other.texcoord0;
//     texcoord1 = other.texcoord1;
//     tangent = other.tangent;
//     bitangent = other.bitangent;
//     num_indices = other.num_indices;
//     num_weights = other.num_weights;
//     bone_weights = other.bone_weights;
//     bone_indices = other.bone_indices;

//     return *this;
// }

Vertex Vertex::operator*(float scalar) const
{
    Vertex other(*this);
    other.SetPosition(GetPosition() * scalar);

    return other;
}

Vertex &Vertex::operator*=(float scalar)
{
    return *this = operator*(scalar);
}

Vertex operator*(const Matrix4 &mat, const Vertex &vertex)
{
    Vertex other(vertex);
    other.SetPosition(mat * vertex.GetPosition());

    return other;
}

Vertex operator*(const Transform &transform, const Vertex &vertex)
{
    return transform.GetMatrix() * vertex;
}

} // namespace hyperion
