/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <ActiveRagdoll/RagdollContacts.hpp>

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/MotionProperties.h>
#include <Jolt/Physics/Collision/ContactListener.h>

namespace Hyperion {

void RagdollContacts::OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings)
{
    Record(inBody1, inBody2, inManifold);
    QueueImpact(inBody1, inBody2, inManifold);
}

void RagdollContacts::QueueImpact(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold)
{
    const bool isFirstTagged = RagdollUserData::Matches(inBody1.GetUserData());
    const bool isSecondTagged = RagdollUserData::Matches(inBody2.GetUserData());

    if ((!isFirstTagged && !isSecondTagged) || inManifold.mRelativeContactPointsOn2.empty())
    {
        return;
    }

    bool isFirstRagdoll = isFirstTagged;

    // two ragdolls only count when a kinematic one walks into a simulated one
    if (isFirstTagged && isSecondTagged)
    {
        if (inBody1.GetUserData() == inBody2.GetUserData() || inBody1.IsKinematic() == inBody2.IsKinematic())
        {
            return;
        }

        isFirstRagdoll = inBody1.IsKinematic();
    }

    const JPH::Body* ragdollBody = &inBody2;
    const JPH::Body* otherBody = &inBody1;
    JPH::Vec3 normal = inManifold.mWorldSpaceNormal;
    JPH::RVec3 point = inManifold.GetWorldSpaceContactPointOn2(0);

    if (isFirstRagdoll)
    {
        ragdollBody = &inBody1;
        otherBody = &inBody2;
        normal = -inManifold.mWorldSpaceNormal;
        point = inManifold.GetWorldSpaceContactPointOn1(0);
    }

    const JPH::Vec3 otherVelocity = otherBody->GetPointVelocity(point);
    const float closing = (otherVelocity - ragdollBody->GetPointVelocity(point)).Dot(normal);

    if (closing < MinImpactSpeed)
    {
        return;
    }

    RagdollImpact impact;
    impact.body = ragdollBody->GetID();
    impact.point = JPH::Vec3(point);
    impact.normal = normal;
    impact.speed = closing;
    impact.otherSpeed = otherVelocity.Dot(normal);
    impact.isOtherStatic = otherBody->IsStatic();
    impact.serial = RagdollUserData::GetSerial(ragdollBody->GetUserData());

    if (RagdollUserData::Matches(otherBody->GetUserData()))
    {
        impact.otherSerial = RagdollUserData::GetSerial(otherBody->GetUserData());
    }

    if (otherBody->IsDynamic())
    {
        const float inverseMass = otherBody->GetMotionProperties()->GetInverseMass();

        if (inverseMass > 0.0f)
        {
            impact.otherMass = 1.0f / inverseMass;
        }
    }

    Mutex::Guard guard(m_mutex);

    if (m_impacts.Size() < MaxQueuedImpacts)
    {
        m_impacts.PushBack(impact);
    }
}

void RagdollContacts::DrainImpacts(Array<RagdollImpact>& outImpacts)
{
    Mutex::Guard guard(m_mutex);

    outImpacts = std::move(m_impacts);
    m_impacts.Clear();
}

void RagdollContacts::DrainBumps(Array<RagdollBump>& outBumps)
{
    Mutex::Guard guard(m_mutex);

    outBumps.Clear();

    for (const auto& it : m_bumps)
    {
        outBumps.PushBack(it.second);
    }

    m_bumps.Clear();
}

void RagdollContacts::OnContactAdded(const JPH::CharacterVirtual* inCharacter, const JPH::CharacterContact& inContact, JPH::CharacterContactSettings& ioSettings)
{
    RecordBump(*inCharacter, inContact, ioSettings);
}

void RagdollContacts::OnContactPersisted(const JPH::CharacterVirtual* inCharacter, const JPH::CharacterContact& inContact, JPH::CharacterContactSettings& ioSettings)
{
    RecordBump(*inCharacter, inContact, ioSettings);
}

void RagdollContacts::RecordBump(
    const JPH::CharacterVirtual& character,
    const JPH::CharacterContact& contact,
    JPH::CharacterContactSettings& ioSettings)
{
    if (!RagdollUserData::Matches(contact.mUserData))
    {
        return;
    }

    ioSettings.mCanPushCharacter = false;
    ioSettings.mCanReceiveImpulses = false;

    const JPH::Vec3 normal = -contact.mContactNormal;
    const float speed = (character.GetLinearVelocity() - contact.mLinearVelocity).Dot(normal);

    if (speed <= 0.0f)
    {
        return;
    }

    const uint32 serial = RagdollUserData::GetSerial(contact.mUserData);

    Mutex::Guard guard(m_mutex);

    const auto it = m_bumps.Find(serial);

    if (it != m_bumps.End() && it->second.speed >= speed)
    {
        return;
    }

    RagdollBump bump;
    bump.point = JPH::Vec3(contact.mPosition);
    bump.normal = normal;
    bump.speed = speed;
    bump.characterMass = character.GetMass();
    bump.serial = serial;

    m_bumps.Set(serial, bump);
}

void RagdollContacts::OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings)
{
    Record(inBody1, inBody2, inManifold);
}

void RagdollContacts::Record(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold)
{
    const bool firstIsRagdoll = inBody2.IsStatic() && RagdollUserData::Matches(inBody1.GetUserData());
    const bool secondIsRagdoll = inBody1.IsStatic() && RagdollUserData::Matches(inBody2.GetUserData());

    if (!firstIsRagdoll && !secondIsRagdoll)
    {
        return;
    }

    const JPH::Body* body = &inBody2;
    JPH::Vec3 normal = inManifold.mWorldSpaceNormal;
    uint32 pointCount = uint32(inManifold.mRelativeContactPointsOn2.size());

    if (firstIsRagdoll)
    {
        body = &inBody1;
        normal = -inManifold.mWorldSpaceNormal;
        pointCount = uint32(inManifold.mRelativeContactPointsOn1.size());
    }

    if (pointCount == 0)
    {
        return;
    }

    JPH::Vec3 pointSum = JPH::Vec3::sZero();

    for (uint32 i = 0; i < pointCount; i++)
    {
        if (firstIsRagdoll)
        {
            pointSum += JPH::Vec3(inManifold.GetWorldSpaceContactPointOn1(i));
        }
        else
        {
            pointSum += JPH::Vec3(inManifold.GetWorldSpaceContactPointOn2(i));
        }
    }

    const JPH::Vec3 point = pointSum / float(pointCount);
    const float speed = -body->GetPointVelocity(JPH::RVec3(point)).Dot(normal);

    Mutex::Guard guard(m_mutex);

    auto it = m_accums.Find(body->GetID().GetIndex());

    if (it == m_accums.End())
    {
        it = m_accums.Insert(body->GetID().GetIndex(), Accum {}).first;
    }

    Accum& accum = it->second;
    accum.count++;
    accum.pointSum += point;
    accum.normalSum += normal;
    accum.speed = MathUtil::Max(accum.speed, speed);
}

void RagdollContacts::Clear()
{
    Mutex::Guard guard(m_mutex);

    m_accums.Clear();
    m_generation.fetch_add(1, std::memory_order_acq_rel);
}

bool RagdollContacts::GetGroundContact(JPH::BodyID bodyId, RagdollGroundContact& outContact) const
{
    outContact = RagdollGroundContact {};

    Mutex::Guard guard(m_mutex);

    const auto it = m_accums.Find(bodyId.GetIndex());

    if (it == m_accums.End() || it->second.count == 0)
    {
        return false;
    }

    const Accum& accum = it->second;

    outContact.count = accum.count;
    outContact.point = accum.pointSum / float(accum.count);
    outContact.normal = accum.normalSum.NormalizedOr(JPH::Vec3::sAxisY());
    outContact.speed = accum.speed;

    return true;
}

} // namespace Hyperion
