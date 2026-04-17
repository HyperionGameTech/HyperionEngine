/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/reflection/Handle.hpp>

#include <Core/memory/UniquePtr.hpp>

#include <Core/math/Transform.hpp>
#include <Core/math/BoundingBox.hpp>
#include <Core/math/BoundingSphere.hpp>
#include <Core/math/Vector3.hpp>
#include <Core/math/Vector4.hpp>

#include <physics/PhysicsMaterial.hpp>

#include <asset/AssetObject.hpp>

#include <type_traits>

namespace Hyperion {

class PhysicsWorldBase;

HYP_ENUM()
enum class PhysicsShapeType : uint8
{
    BOX,
    SPHERE,
    PLANE,
    CONVEX_HULL,
    CAPSULE
};

HYP_CLASS(Abstract)
class PhysicsShape : public AssetObject
{
    HYP_OBJECT_BODY(PhysicsShape);

protected:
    PhysicsShape(Name name, PhysicsShapeType type)
        : AssetObject(name),
          m_type(type)
    {
    }

public:
    ~PhysicsShape() override = default;

    HYP_FORCE_INLINE PhysicsShapeType GetType() const
    {
        return m_type;
    }

    /*! \brief Return the handle specific to the physics engine in use */
    HYP_FORCE_INLINE void* GetHandle() const
    {
        return m_handle.Get();
    }

    /*! \brief Set the internal handle of the PhysicsShape. Only to be used
        by a PhysicsAdapter. */
    HYP_FORCE_INLINE void SetHandle(RC<void>&& handle)
    {
        m_handle = std::move(handle);
    }

protected:
    PhysicsShapeType m_type;

    RC<void> m_handle;
};

HYP_CLASS()
class BoxPhysicsShape final : public PhysicsShape
{
    HYP_OBJECT_BODY(BoxPhysicsShape);

public:
    BoxPhysicsShape(Name name, const BoundingBox& aabb)
        : PhysicsShape(name, PhysicsShapeType::BOX),
          m_aabb(aabb)
    {
    }

    ~BoxPhysicsShape() override = default;

    HYP_FORCE_INLINE const BoundingBox& GetAABB() const
    {
        return m_aabb;
    }

protected:
    HYP_FIELD(Property = "Bounds", Serialize)
    BoundingBox m_aabb;
};

HYP_CLASS()
class SpherePhysicsShape final : public PhysicsShape
{
    HYP_OBJECT_BODY(SpherePhysicsShape);

public:
    SpherePhysicsShape(Name name, const BoundingSphere& sphere)
        : PhysicsShape(name, PhysicsShapeType::SPHERE),
          m_sphere(sphere)
    {
    }

    ~SpherePhysicsShape() override = default;

    HYP_FORCE_INLINE const BoundingSphere& GetSphere() const
    {
        return m_sphere;
    }

protected:
    HYP_FIELD(Property = "Bounds", Serialize)
    BoundingSphere m_sphere;
};

HYP_CLASS()
class PlanePhysicsShape final : public PhysicsShape
{
    HYP_OBJECT_BODY(PlanePhysicsShape);

public:
    PlanePhysicsShape(Name name, const Vec4f& plane)
        : PhysicsShape(name, PhysicsShapeType::PLANE),
          m_plane(plane)
    {
    }

    ~PlanePhysicsShape() override = default;

    HYP_FORCE_INLINE const Vec4f& GetPlane() const
    {
        return m_plane;
    }

protected:
    HYP_FIELD(Property = "Plane", Serialize)
    Vec4f m_plane;
};

HYP_CLASS()
class ConvexHullPhysicsShape final : public PhysicsShape
{
    HYP_OBJECT_BODY(ConvexHullPhysicsShape);

public:
    ConvexHullPhysicsShape(Name name, const Array<Vec3f>& vertices)
        : PhysicsShape(name, PhysicsShapeType::CONVEX_HULL)
    {
        m_vertices.Resize(vertices.Size() * 3);

        for (size_t index = 0; index < vertices.Size(); index++)
        {
            m_vertices[index * 3] = vertices[index].x;
            m_vertices[index * 3 + 1] = vertices[index].y;
            m_vertices[index * 3 + 2] = vertices[index].z;
        }
    }

    ~ConvexHullPhysicsShape() override = default;

    HYP_FORCE_INLINE const float* GetVertexData() const
    {
        return m_vertices.Data();
    }

    HYP_FORCE_INLINE size_t NumVertices() const
    {
        return m_vertices.Size() / 3;
    }

protected:
    // @TODO Use blob data for this to serialize.

    Array<float> m_vertices;
};

HYP_CLASS()
class CapsulePhysicsShape final : public PhysicsShape
{
    HYP_OBJECT_BODY(CapsulePhysicsShape);

public:
    CapsulePhysicsShape(Name name, float radius, float height)
        : PhysicsShape(name, PhysicsShapeType::CAPSULE),
          m_radius(radius),
          m_height(height)
    {
    }

    ~CapsulePhysicsShape() override = default;

    HYP_FORCE_INLINE float GetRadius() const
    {
        return m_radius;
    }

    HYP_FORCE_INLINE float GetHeight() const
    {
        return m_height;
    }

protected:
    HYP_FIELD(Property = "Radius", Serialize)
    float m_radius;

    HYP_FIELD(Property = "Height", Serialize)
    float m_height;
};

HYP_CLASS()
class HYP_API RigidBody final : public ObjectBase
{
    HYP_OBJECT_BODY(RigidBody);

public:
    RigidBody();

    RigidBody(const RigidBody& other) = delete;
    RigidBody& operator=(const RigidBody& other) = delete;

    RigidBody(RigidBody&& other) noexcept = default;
    RigidBody& operator=(RigidBody&& other) noexcept = default;

    ~RigidBody();

    HYP_FORCE_INLINE const Transform& GetTransform() const
    {
        return m_transform;
    }

    HYP_FORCE_INLINE void SetTransform(const Transform& transform)
    {
        m_transform = transform;
    }

    HYP_FORCE_INLINE bool IsKinematic() const
    {
        return m_isKinematic;
    }

    HYP_FORCE_INLINE void SetIsKinematic(bool isKinematic)
    {
        m_isKinematic = isKinematic;
    }

    /*! \brief Return the handle specific to the physics engine in use */
    HYP_FORCE_INLINE void* GetHandle() const
    {
        return m_handle.Get();
    }

    /*! \brief Set the internal handle of the RigidBody. Only to be used
        by a PhysicsAdapter. */
    HYP_FORCE_INLINE void SetHandle(RC<void>&& handle)
    {
        m_handle = std::move(handle);
    }
    
    PhysicsShape* shape;
    PhysicsMaterial* physicsMaterial;

private:
    Transform m_transform;

    bool m_isKinematic;

    RC<void> m_handle;
};

} // namespace Hyperion
