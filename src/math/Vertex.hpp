#ifndef VERTEX_H
#define VERTEX_H

#define MAX_BONE_WEIGHTS 4
#define MAX_BONE_INDICES 4

#include "Vector2.hpp"
#include "Vector3.hpp"
#include "Transform.hpp"
#include "Matrix4.hpp"
#include <HashCode.hpp>
#include <util/Defines.hpp>
#include <Types.hpp>

#include <array>
#include <cstring>

namespace hyperion {

class Vertex {
public:
    Vertex()
        : num_indices(0),
          num_weights(0)
    {
        for (UInt i = 0; i < MAX_BONE_INDICES; i++) {
            bone_indices[i] = 0;
            bone_weights[i] = 0;
        }
    }

    Vertex(const Vector3 &position)
        : position(position),
          num_indices(0),
          num_weights(0)
    {
        for (UInt i = 0; i < MAX_BONE_INDICES; i++) {
            bone_indices[i] = 0;
            bone_weights[i] = 0;
        }
    }

    Vertex(const Vector3 &position, const Vector2 &texcoord0)
        : position(position),
          texcoord0(texcoord0),
          num_indices(0),
          num_weights(0)
    {
        for (UInt i = 0; i < MAX_BONE_INDICES; i++) {
            bone_indices[i] = 0;
            bone_weights[i] = 0;
        }
    }

    Vertex(const Vector3 &position, const Vector2 &texcoord0, const Vector3 &normal)
        : position(position),
          normal(normal),
          texcoord0(texcoord0),
          num_indices(0),
          num_weights(0)
    {
        for (UInt i = 0; i < MAX_BONE_INDICES; i++) {
            bone_indices[i] = 0;
            bone_weights[i] = 0;
        }
    }

    Vertex(const Vertex &other)
        : position(other.position),
          normal(other.normal),
          texcoord0(other.texcoord0),
          texcoord1(other.texcoord1),
          tangent(other.tangent),
          bitangent(other.bitangent),
          num_indices(other.num_indices),
          num_weights(other.num_weights),
          bone_weights(other.bone_weights),
          bone_indices(other.bone_indices)
    {
    }
    
    bool operator==(const Vertex &other) const;
    Vertex &operator=(const Vertex &other);
    Vertex operator*(float scalar) const;
    Vertex operator*(const Matrix4 &mat) const;
    Vertex operator*(const Transform &transform) const;
    Vertex &operator*=(float scalar);
    Vertex &operator*=(const Matrix4 &mat);
    Vertex &operator*=(const Transform &transform);

    void SetPosition(const Vector3 &vec)  { position = vec; }
    const Vector3 &GetPosition() const    { return position; }
    void SetNormal(const Vector3 &vec)    { normal = vec; }
    const Vector3 &GetNormal() const      { return normal; }
    void SetTexCoord0(const Vector2 &vec) { texcoord0 = vec; }
    const Vector2 &GetTexCoord0() const   { return texcoord0; }
    void SetTexCoord1(const Vector2 &vec) { texcoord1 = vec; }
    const Vector2 &GetTexCoord1() const   { return texcoord1; }
    void SetTangent(const Vector3 &vec)   { tangent = vec; }
    const Vector3 &GetTangent() const     { return tangent; }
    void SetBitangent(const Vector3 &vec) { bitangent = vec; }
    const Vector3 &GetBitangent() const   { return bitangent; }

    void SetBoneWeight(int i, float val) { bone_weights[i] = val; }
    float GetBoneWeight(int i) const     { return bone_weights[i]; }
    void SetBoneIndex(int i, int val)    { bone_indices[i] = val; }
    int GetBoneIndex(int i) const        { return bone_indices[i]; }

    void AddBoneWeight(float val) { if (num_weights < MAX_BONE_WEIGHTS) bone_weights[num_weights++] = val; }
    void AddBoneIndex(int val)    { if (num_indices < MAX_BONE_INDICES) bone_indices[num_indices++] = val; }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(position.GetHashCode());
        hc.Add(normal.GetHashCode());
        hc.Add(texcoord0.GetHashCode());
        hc.Add(texcoord1.GetHashCode());
        hc.Add(tangent.GetHashCode());
        hc.Add(bitangent.GetHashCode());
        hc.Add(num_indices);
        hc.Add(num_weights);

        for (UInt i = 0; i < MAX_BONE_INDICES; i++) {
            hc.Add(bone_indices[i]);
        }

        for (UInt i = 0; i < MAX_BONE_WEIGHTS; i++) {
            hc.Add(bone_weights[i]);
        }

        return hc;
    }

private:
    Vector3 position;
    Vector3 normal;
    Vector2 texcoord0;
    Vector2 texcoord1;
    Vector3 tangent;
    Vector3 bitangent;

    uint8_t num_indices,
            num_weights;

    std::array<Float, MAX_BONE_WEIGHTS> bone_weights;
    std::array<UInt,  MAX_BONE_INDICES> bone_indices;
};

} // namespace hyperion

HYP_DEF_STL_HASH(hyperion::Vertex);

#endif
