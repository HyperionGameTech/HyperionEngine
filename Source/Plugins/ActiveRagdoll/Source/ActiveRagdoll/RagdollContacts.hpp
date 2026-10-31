/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Map.hpp>
#include <Core/Threading/Mutex.hpp>

#include <Core/Types.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

#include <atomic>

namespace Hyperion {

namespace RagdollUserData {

static constexpr uint64 Tag = 0x5247444C00000000ull;
static constexpr uint64 TagMask = 0xFFFFFFFF00000000ull;

HYP_FORCE_INLINE bool Matches(uint64 userData)
{
    return (userData & TagMask) == Tag;
}

HYP_FORCE_INLINE uint32 GetSerial(uint64 userData)
{
    return uint32(userData & ~TagMask);
}

} // namespace RagdollUserData

struct RagdollGroundContact
{
    uint32 count = 0;
    JPH::Vec3 point = JPH::Vec3::sZero();
    JPH::Vec3 normal = JPH::Vec3::sZero();
    float speed = 0.0f;
};

// something running into a ragdoll body, or a ragdoll body running into something
struct RagdollImpact
{
    JPH::BodyID body;
    JPH::Vec3 point = JPH::Vec3::sZero();
    // pushes the ragdoll body away from the other one
    JPH::Vec3 normal = JPH::Vec3::sZero();
    // closing speed along the normal
    float speed = 0.0f;
    // how fast the other body moves into the ragdoll body, so a still prop the ragdoll falls on doesn't count as a hit
    float otherSpeed = 0.0f;
    float otherMass = 0.0f;
    bool isOtherStatic = false;
    uint32 serial = 0;
    // the other ragdoll's serial, 0 when it isn't one
    uint32 otherSerial = 0;

    HYP_FORCE_INLINE bool IsStruck(float minSpeed) const
    {
        return speed >= minSpeed && otherSpeed >= minSpeed;
    }
};

struct RagdollBump
{
    JPH::Vec3 point = JPH::Vec3::sZero();
    JPH::Vec3 normal = JPH::Vec3::sZero();
    float speed = 0.0f;
    float characterMass = 0.0f;
    uint32 serial = 0;
};

class RagdollContacts final : public JPH::ContactListener, public JPH::CharacterContactListener
{
public:
    static constexpr float MinImpactSpeed = 0.5f;
    static constexpr uint32 MaxQueuedImpacts = 64;

    void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override;
    void OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override;

    void OnContactAdded(const JPH::CharacterVirtual* inCharacter, const JPH::CharacterContact& inContact, JPH::CharacterContactSettings& ioSettings) override;
    void OnContactPersisted(const JPH::CharacterVirtual* inCharacter, const JPH::CharacterContact& inContact, JPH::CharacterContactSettings& ioSettings) override;

    // between physics updates
    void Clear();

    void DrainImpacts(Array<RagdollImpact>& outImpacts);
    void DrainBumps(Array<RagdollBump>& outBumps);

    bool GetGroundContact(JPH::BodyID bodyId, RagdollGroundContact& outContact) const;

    HYP_FORCE_INLINE uint32 GetGeneration() const
    {
        return m_generation.load(std::memory_order_acquire);
    }

private:
    struct Accum
    {
        uint32 count = 0;
        JPH::Vec3 pointSum = JPH::Vec3::sZero();
        JPH::Vec3 normalSum = JPH::Vec3::sZero();
        float speed = 0.0f;
    };

    void Record(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold);
    void QueueImpact(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold);
    void RecordBump(const JPH::CharacterVirtual& character, const JPH::CharacterContact& contact, JPH::CharacterContactSettings& ioSettings);

    mutable Mutex m_mutex;
    Map<uint32, Accum> m_accums;
    Array<RagdollImpact> m_impacts;
    // the fastest bump on each ragdoll this update, by serial
    Map<uint32, RagdollBump> m_bumps;
    std::atomic<uint32> m_generation { 0 };
};

} // namespace Hyperion
