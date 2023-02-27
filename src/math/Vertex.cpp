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

Vertex &Vertex::operator=(const Vertex &other)
{
    position = other.position;
    normal = other.normal;
    texcoord0 = other.texcoord0;
    texcoord1 = other.texcoord1;
    tangent = other.tangent;
    bitangent = other.bitangent;
    num_indices = other.num_indices;
    num_weights = other.num_weights;
    bone_weights = other.bone_weights;
    bone_indices = other.bone_indices;

    return *this;
}

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

static struct VertexScriptBindings : ScriptBindingsBase
{
    VertexScriptBindings()
        : ScriptBindingsBase(TypeID::ForType<Vertex>())
    {
    }

    virtual void Generate(APIInstance &api_instance) override
    {
        api_instance.Module(Config::global_module_name)
            .Class<Vertex>(
                "Vertex",
                {
                    API::NativeMemberDefine("__intern", BuiltinTypes::ANY, vm::Value(vm::Value::HEAP_POINTER, { .ptr = nullptr })),
                    API::NativeMemberDefine(
                        "$construct",
                        BuiltinTypes::ANY,
                        {
                            { "self", BuiltinTypes::ANY }
                        },
                        CxxCtor< Vertex > 
                    ),
                    API::NativeMemberDefine(
                        "GetPosition",
                        BuiltinTypes::ANY,
                        {
                            { "self", BuiltinTypes::ANY }
                        },
                        CxxMemberFn< const Vector3 &, Vertex, &Vertex::GetPosition >
                    ),
                    API::NativeMemberDefine(
                        "SetPosition",
                        BuiltinTypes::VOID_TYPE,
                        {
                            { "self", BuiltinTypes::ANY },
                            { "position", BuiltinTypes::ANY }
                        },
                        CxxMemberFn< void, Vertex, const Vector3 &, &Vertex::SetPosition >
                    ),
                    API::NativeMemberDefine(
                        "GetNormal",
                        BuiltinTypes::ANY,
                        {
                            { "self", BuiltinTypes::ANY }
                        },
                        CxxMemberFn< const Vector3 &, Vertex, &Vertex::GetNormal >
                    ),
                    API::NativeMemberDefine(
                        "SetNormal",
                        BuiltinTypes::VOID_TYPE,
                        {
                            { "self", BuiltinTypes::ANY },
                            { "normal", BuiltinTypes::ANY }
                        },
                        CxxMemberFn< void, Vertex, const Vector3 &, &Vertex::SetNormal >
                    ),
                    API::NativeMemberDefine(
                        "GetTangent",
                        BuiltinTypes::ANY,
                        {
                            { "self", BuiltinTypes::ANY }
                        },
                        CxxMemberFn< const Vector3 &, Vertex, &Vertex::GetTangent >
                    ),
                    API::NativeMemberDefine(
                        "SetTangent",
                        BuiltinTypes::VOID_TYPE,
                        {
                            { "self", BuiltinTypes::ANY },
                            { "normal", BuiltinTypes::ANY }
                        },
                        CxxMemberFn< void, Vertex, const Vector3 &, &Vertex::SetTangent >
                    ),
                    API::NativeMemberDefine(
                        "GetBitangent",
                        BuiltinTypes::ANY,
                        {
                            { "self", BuiltinTypes::ANY }
                        },
                        CxxMemberFn< const Vector3 &, Vertex, &Vertex::GetBitangent >
                    ),
                    API::NativeMemberDefine(
                        "SetBitangent",
                        BuiltinTypes::VOID_TYPE,
                        {
                            { "self", BuiltinTypes::ANY },
                            { "normal", BuiltinTypes::ANY }
                        },
                        CxxMemberFn< void, Vertex, const Vector3 &, &Vertex::SetBitangent >
                    ),
                    API::NativeMemberDefine(
                        "GetBoneIndex",
                        BuiltinTypes::INT,
                        {
                            { "self", BuiltinTypes::ANY },
                            { "index", BuiltinTypes::INT }
                        },
                        CxxMemberFn< Int, Vertex, Int, &Vertex::GetBoneIndex >
                    ),
                    API::NativeMemberDefine(
                        "SetBoneIndex",
                        BuiltinTypes::VOID_TYPE,
                        {
                            { "self", BuiltinTypes::ANY },
                            { "index", BuiltinTypes::INT },
                            { "value", BuiltinTypes::INT }
                        },
                        CxxMemberFn< void, Vertex, Int, Int, &Vertex::SetBoneIndex >
                    ),
                    API::NativeMemberDefine(
                        "GetBoneWeight",
                        BuiltinTypes::FLOAT,
                        {
                            { "self", BuiltinTypes::ANY },
                            { "index", BuiltinTypes::INT }
                        },
                        CxxMemberFn< Float, Vertex, Int, &Vertex::GetBoneWeight >
                    ),
                    API::NativeMemberDefine(
                        "SetBoneWeight",
                        BuiltinTypes::VOID_TYPE,
                        {
                            { "self", BuiltinTypes::ANY },
                            { "index", BuiltinTypes::INT },
                            { "value", BuiltinTypes::FLOAT }
                        },
                        CxxMemberFn< void, Vertex, Int, Float, &Vertex::SetBoneWeight >
                    )
                }
            );
    }
} vertex_script_bindings = { };

} // namespace hyperion
