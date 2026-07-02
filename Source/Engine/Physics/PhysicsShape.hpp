/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 * */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Memory/SharedPtr.hpp>

#include <Core/Math/Transform.hpp>
#include <Core/Math/BoundingBox.hpp>
#include <Core/Math/BoundingSphere.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>

#include <Asset/AssetObject.hpp>

namespace Hyperion {

HYP_ENUM()
enum class PhysicsShapeType : uint8
{
    BOX,
    SPHERE,
    PLANE,
    CONVEX_HULL,
    CAPSULE
};

HYP_CLASS(Abstract, AssetBucket = "PhysicsShapes")
class PhysicsShape : public AssetObject
{
    HYP_OBJECT_BODY(PhysicsShape);

protected:
    PhysicsShape() = default;

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
    HYP_FORCE_INLINE void SetHandle(SharedPtr<void>&& handle)
    {
        m_handle = std::move(handle);
    }

protected:
    PhysicsShapeType m_type;

    SharedPtr<void> m_handle;
};

HYP_CLASS()
class BoxPhysicsShape final : public PhysicsShape
{
    HYP_OBJECT_BODY(BoxPhysicsShape);

public:
    BoxPhysicsShape() = default;

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
    SpherePhysicsShape() = default;

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
    PlanePhysicsShape() = default;

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
    ConvexHullPhysicsShape() = default;

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
    CapsulePhysicsShape() = default;

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

} // namespace Hyperion
