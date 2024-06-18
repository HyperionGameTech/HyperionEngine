/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_PHYSICS_RIGID_BODY_HPP
#define HYPERION_PHYSICS_RIGID_BODY_HPP

#include <core/Base.hpp>
#include <core/Defines.hpp>

#include <math/Transform.hpp>
#include <math/BoundingBox.hpp>
#include <math/BoundingSphere.hpp>
#include <math/Vector3.hpp>
#include <math/Vector4.hpp>

#include <physics/PhysicsMaterial.hpp>

#include <Types.hpp>

#include <type_traits>

namespace hyperion {

class Engine;

} // namespace hyperion

namespace hyperion::physics {

enum class PhysicsShapeType : uint32
{
    NONE,
    BOX,
    SPHERE,
    PLANE,
    CONVEX_HULL
};

class PhysicsShape
{
public:
    PhysicsShape()
        : m_type(PhysicsShapeType::NONE)
    {
    }

    PhysicsShape(PhysicsShapeType type)
        : m_type(type)
    {
    }

    ~PhysicsShape() = default;

    HYP_NODISCARD HYP_FORCE_INLINE
    PhysicsShapeType GetType() const
        { return m_type; }

    /*! \brief Return the handle specific to the physics engine in use */
    HYP_NODISCARD HYP_FORCE_INLINE
    UniquePtr<void> &GetHandle()
        { return m_handle; }

    /*! \brief Return the handle specific to the physics engine in use */
    HYP_NODISCARD HYP_FORCE_INLINE
    const UniquePtr<void> &GetHandle() const
        { return m_handle; }

    /*! \brief Set the internal handle of the PhysicsShape. Only to be used
        by a PhysicsAdapter. */
    HYP_FORCE_INLINE
    void SetHandle(UniquePtr<void> &&handle)
        { m_handle = std::move(handle); }

protected:
    PhysicsShapeType    m_type;

    UniquePtr<void>     m_handle;
};

class BoxPhysicsShape : public PhysicsShape
{
public:
    BoxPhysicsShape(const BoundingBox &aabb)
        : PhysicsShape(PhysicsShapeType::BOX),
          m_aabb(aabb)
    {
    }

    ~BoxPhysicsShape() = default;

    const BoundingBox &GetAABB() const
        { return m_aabb; }

protected:
    BoundingBox m_aabb;
};

class SpherePhysicsShape : public PhysicsShape
{
public:
    SpherePhysicsShape(const BoundingSphere &sphere)
        : PhysicsShape(PhysicsShapeType::SPHERE),
          m_sphere(sphere)
    {
    }

    ~SpherePhysicsShape() = default;

    const BoundingSphere &GetSphere() const
        { return m_sphere; }

protected:
    BoundingSphere  m_sphere;
};

class PlanePhysicsShape : public PhysicsShape
{
public:
    PlanePhysicsShape(const Vec4f &plane)
        : PhysicsShape(PhysicsShapeType::PLANE),
          m_plane(plane)
    {
    }

    ~PlanePhysicsShape() = default;

    const Vec4f &GetPlane() const
        { return m_plane; }

protected:
    Vec4f   m_plane;
};

class ConvexHullPhysicsShape : public PhysicsShape
{
public:
    ConvexHullPhysicsShape(const Array<Vec3f> &vertices)
        : PhysicsShape(PhysicsShapeType::CONVEX_HULL)
    {
        m_vertices.Resize(vertices.Size() * 3);

        for (SizeType index = 0; index < vertices.Size(); index++) {
            m_vertices[index * 3] = vertices[index].x;
            m_vertices[index * 3 + 1] = vertices[index].y;
            m_vertices[index * 3 + 2] = vertices[index].z;
        }
    }

    ~ConvexHullPhysicsShape() = default;

    const float *GetVertexData() const
        { return m_vertices.Data(); }

    SizeType NumVertices() const
        { return m_vertices.Size() / 3; }

protected:
    Array<float>    m_vertices;
};

class HYP_API RigidBody : public BasicObject<STUB_CLASS(RigidBody)>
{
public:
    RigidBody();
    RigidBody(const PhysicsMaterial &physics_material);
    RigidBody(RC<PhysicsShape> &&shape, const PhysicsMaterial &physics_material);

    RigidBody(const RigidBody &other)               = delete;
    RigidBody &operator=(const RigidBody &other)    = delete;
    ~RigidBody();

    void Init();

    /*! \brief Get the world-space transform of this RigidBody.
        If changed, you will have to flag that the transform has changed,
        so that the physics engine's internal rigidbody will have its transform updated. */
    HYP_NODISCARD HYP_FORCE_INLINE
    Transform &GetTransform()
        { return m_transform; }

    /*! \brief Get the world-space transform of this RigidBody. */
    HYP_NODISCARD HYP_FORCE_INLINE
    const Transform &GetTransform() const
        { return m_transform; }
    
    HYP_FORCE_INLINE
    void SetTransform(const Transform &transform)
        { m_transform = transform; }

    HYP_NODISCARD HYP_FORCE_INLINE
    RC<PhysicsShape> &GetShape()
        { return m_shape; }

    HYP_NODISCARD HYP_FORCE_INLINE
    const RC<PhysicsShape> &GetShape() const
        { return m_shape; }

    HYP_FORCE_INLINE
    void SetShape(RC<PhysicsShape> &&shape);

    HYP_NODISCARD HYP_FORCE_INLINE
    PhysicsMaterial &GetPhysicsMaterial()
        { return m_physics_material; }

    HYP_NODISCARD HYP_FORCE_INLINE
    const PhysicsMaterial &GetPhysicsMaterial() const
        { return m_physics_material; }

    void SetPhysicsMaterial(const PhysicsMaterial &physics_material);

    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsKinematic() const
        { return m_is_kinematic; }

    HYP_FORCE_INLINE
    void SetIsKinematic(bool is_kinematic)
        { m_is_kinematic = is_kinematic; }

    /*! \brief Return the handle specific to the physics engine in use */
    HYP_NODISCARD HYP_FORCE_INLINE
    UniquePtr<void> &GetHandle()
        { return m_handle; }

    /*! \brief Return the handle specific to the physics engine in use */
    HYP_NODISCARD HYP_FORCE_INLINE
    const UniquePtr<void> &GetHandle() const
        { return m_handle; }

    /*! \brief Set the internal handle of the RigidBody. Only to be used
        by a PhysicsAdapter. */
    HYP_FORCE_INLINE
    void SetHandle(UniquePtr<void> &&handle)
        { m_handle = std::move(handle); }

    void ApplyForce(const Vec3f &force);

private:
    Transform           m_transform;
    RC<PhysicsShape>    m_shape;
    PhysicsMaterial     m_physics_material;
    bool                m_is_kinematic;

    UniquePtr<void>     m_handle;
};

} // namespace hyperion::physics

#endif