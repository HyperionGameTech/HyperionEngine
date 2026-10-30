/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <ActiveRagdoll/RagdollMath.hpp>

namespace Hyperion {

struct RagdollRigArm
{
    uint32 clavicle = 0;
    uint32 upper = 0;
    uint32 lower = 0;
    uint32 hand = 0;
    float upperLength = 0.0f;
    float lowerLength = 0.0f;
    float sideSign = 1.0f;
};

struct RagdollRigLeg
{
    uint32 thigh = 0;
    uint32 shin = 0;
    uint32 foot = 0;
    float upper = 0.0f;
    float lower = 0.0f;
    float hingeSign = 1.0f;
    float sideSign = 1.0f;

    // the hip joint in pelvis space
    JPH::Vec3 hipOffset = JPH::Vec3::sZero();

    // sole points in foot body space, set for box feet
    JPH::Vec3 soleCenter = JPH::Vec3::sZero();
    JPH::Vec3 soleCorners[RagdollMath::SoleCorners];

    JPH::Quat restFoot = JPH::Quat::sIdentity();
};

// named bodies and rest geometry of a humanoid ragdoll, which balance and the behaviours share
struct RagdollRig
{
    static constexpr uint32 LimbCount = 2;
    static constexpr uint32 MaxSpine = 4;

    RagdollRigArm arms[LimbCount];
    RagdollRigLeg legs[LimbCount];

    // the chest first, down towards (not including) the pelvis
    uint32 spine[MaxSpine] = {};
    uint32 spineCount = 0;
    uint32 chest = 0;
    uint32 neck = 0;

    JPH::Vec3 restForward = JPH::Vec3::sAxisZ();
    float footHalfWidth = 0.045f;
    float footHalfLength = 0.1f;

    // two box feet on hinged shins off the pelvis, and a chest
    bool canBalance = false;

    // two arms, two legs and a head on a spine
    bool isValid = false;
};

} // namespace Hyperion
