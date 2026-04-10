#pragma once

#include <ThirdParty/Jolt/Jolt.h>
#include <ThirdParty/Jolt/Physics/Body/Body.h>
#include <ThirdParty/Jolt/Physics/Body/BodyID.h>

#include <Core/FxPagedArray.hpp>
#include <FxEntity.hpp>
#include <FxMaterial.hpp>
#include <Renderer/FxPrimitiveMesh.hpp>

class FxPrimitiveMesh;

struct PhProperties
{
public:
    PhProperties() = default;

    float32 ConvexRadius = 0.001f;
    float32 Friction = 0.2f;
    float32 Restitution = 0.1f;

    /**
     * @brief Density of the physics material in kg / m^3
     * @default The density of white pine.
     */
    float32 Density = 300.0f;
};


enum class PhMotionType
{
    eStatic,
    eDynamic,
};

enum class PhPrimitiveType
{
    eBox,
};


class PhObject
{
public:
    enum Flags
    {
        eNone = 0x00,
        eCreateInactive = 0x01,
    };


public:
    void CreatePrimitiveBody(PhPrimitiveType primitive_type, const FxVec3f& dimensions, PhMotionType motion_type,
                             const PhProperties& object_properties);

    void CreateMeshBody(const FxPrimitiveMesh& mesh, PhMotionType motion_type, const PhProperties& object_properties);

    void DestroyPhysicsBody();

    void Teleport(FxVec3f position, FxQuat rotation);

    FX_FORCE_INLINE FxVec3f GetPosition() { return FxVec3f(mpPhysicsBody->GetPosition()); }
    FX_FORCE_INLINE FxQuat GetRotation() { return FxQuat(mpPhysicsBody->GetRotation()); }

    FX_FORCE_INLINE JPH::Body* GetBody() { return mpPhysicsBody; };
    FX_FORCE_INLINE const JPH::BodyID& GetBodyId() { return mpPhysicsBody->GetID(); };

    FX_FORCE_INLINE PhMotionType GetMotionType() const { return mMotionType; }

private:
    void CreateJoltBody(JPH::ShapeRefC shape, Flags flags, PhMotionType type, const PhProperties& properties);

public:
    JPH::Body* mpPhysicsBody = nullptr;
    PhMotionType mMotionType = PhMotionType::eStatic;

    bool mbHasPhysicsBody : 1 = false;
};
