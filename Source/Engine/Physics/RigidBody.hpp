/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Math/Transform.hpp>
#include <Core/Math/BoundingBox.hpp>
#include <Core/Math/BoundingSphere.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>

#include <Physics/PhysicsMaterial.hpp>

#include <Asset/AssetObject.hpp>

#include <type_traits>

namespace Hyperion {

class PhysicsWorldBase;
class PhysicsShape;

HYP_CLASS()
class ENGINE_API RigidBody final : public ObjectBase
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
    HYP_FORCE_INLINE void* GetInternalData() const
    {
        return m_internalData.Get();
    }

    /*! \brief Set the internal handle of the RigidBody. Only to be used
        by a PhysicsAdapter. */
    HYP_FORCE_INLINE void SetInternalData(SharedPtr<void>&& internalData)
    {
        m_internalData = std::move(internalData);
    }

    PhysicsShape* shape;
    PhysicsMaterial* physicsMaterial;

private:
    Transform m_transform;

    bool m_isKinematic;

    SharedPtr<void> m_internalData;
};

} // namespace Hyperion
