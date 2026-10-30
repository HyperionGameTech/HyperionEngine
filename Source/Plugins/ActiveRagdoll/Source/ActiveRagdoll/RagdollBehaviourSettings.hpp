/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/ObjectMacros.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

// behaviours mix in ascending priority, so a higher priority overrides a lower one where they overlap; angles are
// degrees and times seconds. A ReactionDelay holds events back from a behaviour that long (give or take a third)

// holds the pose the body falls into, loosely, once it's down
HYP_STRUCT()
struct RagdollFallenSettings
{
    HYP_STRUCT_BODY(RagdollFallenSettings);

    HYP_FIELD(Property = "Enabled", Serialize = true, Editor = true)
    bool enabled = true;

    HYP_FIELD(Property = "Priority", Serialize = true, Editor = true)
    float priority = 0.0f;

    HYP_FIELD(Property = "FadeIn", Serialize = true, Editor = true)
    float fadeIn = 0.4f;

    HYP_FIELD(Property = "FadeOut", Serialize = true, Editor = true)
    float fadeOut = 0.4f;

    HYP_FIELD(Property = "Strength", Serialize = true, Editor = true)
    float strength = 0.25f;

    HYP_FIELD(Property = "FollowTime", Serialize = true, Editor = true)
    float followTime = 0.3f;

    // how much of the limbs' weight the muscles hold up while down; near 0 lets them rest on the ground
    HYP_FIELD(Property = "GravityComp", Serialize = true, Editor = true)
    float gravityComp = 0.05f;
};

// a shove turns balance on and spreads the arms
HYP_STRUCT()
struct RagdollStaggerSettings
{
    HYP_STRUCT_BODY(RagdollStaggerSettings);

    HYP_FIELD(Property = "Enabled", Serialize = true, Editor = true)
    bool enabled = true;

    HYP_FIELD(Property = "Priority", Serialize = true, Editor = true)
    float priority = 20.0f;

    HYP_FIELD(Property = "FadeIn", Serialize = true, Editor = true)
    float fadeIn = 0.1f;

    HYP_FIELD(Property = "FadeOut", Serialize = true, Editor = true)
    float fadeOut = 0.15f;

    // time from the shove to the response, like a person's automatic postural reaction
    HYP_FIELD(Property = "ReactionDelay", Serialize = true, Editor = true)
    float reactionDelay = 0.08f;

    HYP_FIELD(Property = "MinImpulse", Serialize = true, Editor = true)
    float minImpulse = 1.0f;

    HYP_FIELD(Property = "MinTime", Serialize = true, Editor = true)
    float minTime = 0.75f;

    HYP_FIELD(Property = "ArmSpread", Serialize = true, Editor = true)
    float armSpread = 65.0f;

    HYP_FIELD(Property = "SpreadSpeed", Serialize = true, Editor = true)
    float spreadSpeed = 0.4f;
};

// arms reach out to catch a capture point that has left the feet
HYP_STRUCT()
struct RagdollArmBalanceSettings
{
    HYP_STRUCT_BODY(RagdollArmBalanceSettings);

    HYP_FIELD(Property = "Enabled", Serialize = true, Editor = true)
    bool enabled = true;

    HYP_FIELD(Property = "Priority", Serialize = true, Editor = true)
    float priority = 25.0f;

    HYP_FIELD(Property = "FadeIn", Serialize = true, Editor = true)
    float fadeIn = 0.2f;

    HYP_FIELD(Property = "FadeOut", Serialize = true, Editor = true)
    float fadeOut = 0.3f;

    HYP_FIELD(Property = "ForeReach", Serialize = true, Editor = true)
    float foreReach = 0.12f;

    HYP_FIELD(Property = "SideReach", Serialize = true, Editor = true)
    float sideReach = 0.15f;

    HYP_FIELD(Property = "RaiseReach", Serialize = true, Editor = true)
    float raiseReach = 0.1f;

    HYP_FIELD(Property = "ErrorScale", Serialize = true, Editor = true)
    float errorScale = 0.1f;

    HYP_FIELD(Property = "DeadZone", Serialize = true, Editor = true)
    float deadZone = 0.03f;

    HYP_FIELD(Property = "SmoothTime", Serialize = true, Editor = true)
    float smoothTime = 0.2f;

    HYP_FIELD(Property = "Strength", Serialize = true, Editor = true)
    float strength = 0.4f;
};

// a hit loosens the struck limb, flinches, looks at the wound and grabs it with the other hand
HYP_STRUCT()
struct RagdollShotSettings
{
    HYP_STRUCT_BODY(RagdollShotSettings);

    HYP_FIELD(Property = "Enabled", Serialize = true, Editor = true)
    bool enabled = true;

    HYP_FIELD(Property = "Priority", Serialize = true, Editor = true)
    float priority = 30.0f;

    HYP_FIELD(Property = "FadeIn", Serialize = true, Editor = true)
    float fadeIn = 0.3f;

    HYP_FIELD(Property = "FadeOut", Serialize = true, Editor = true)
    float fadeOut = 0.2f;

    HYP_FIELD(Property = "ReactionDelay", Serialize = true, Editor = true)
    float reactionDelay = 0.07f;

    HYP_FIELD(Property = "Loosen", Serialize = true, Editor = true)
    float loosen = 0.25f;

    HYP_FIELD(Property = "RecoverTime", Serialize = true, Editor = true)
    float recoverTime = 0.6f;

    HYP_FIELD(Property = "Flinch", Serialize = true, Editor = true)
    float flinch = 15.0f;

    HYP_FIELD(Property = "FlinchTime", Serialize = true, Editor = true)
    float flinchTime = 0.35f;

    HYP_FIELD(Property = "GrabDelay", Serialize = true, Editor = true)
    float grabDelay = 0.15f;

    HYP_FIELD(Property = "GrabTime", Serialize = true, Editor = true)
    float grabTime = 2.5f;

    HYP_FIELD(Property = "GrabReach", Serialize = true, Editor = true)
    float grabReach = 1.15f;

    HYP_FIELD(Property = "HandOffset", Serialize = true, Editor = true)
    float handOffset = 0.07f;

    HYP_FIELD(Property = "LookTime", Serialize = true, Editor = true)
    float lookTime = 1.5f;

    HYP_FIELD(Property = "LookYaw", Serialize = true, Editor = true)
    float lookYaw = 80.0f;

    HYP_FIELD(Property = "LookPitch", Serialize = true, Editor = true)
    float lookPitch = 35.0f;
};

// hands go out towards where a fall will land
HYP_STRUCT()
struct RagdollBraceSettings
{
    HYP_STRUCT_BODY(RagdollBraceSettings);

    HYP_FIELD(Property = "Enabled", Serialize = true, Editor = true)
    bool enabled = true;

    HYP_FIELD(Property = "Priority", Serialize = true, Editor = true)
    float priority = 50.0f;

    HYP_FIELD(Property = "FadeIn", Serialize = true, Editor = true)
    float fadeIn = 0.35f;

    HYP_FIELD(Property = "FadeOut", Serialize = true, Editor = true)
    float fadeOut = 0.4f;

    HYP_FIELD(Property = "ReactionDelay", Serialize = true, Editor = true)
    float reactionDelay = 0.12f;

    HYP_FIELD(Property = "Horizon", Serialize = true, Editor = true)
    float horizon = 0.7f;

    HYP_FIELD(Property = "Spread", Serialize = true, Editor = true)
    float spread = 0.2f;

    HYP_FIELD(Property = "Strength", Serialize = true, Editor = true)
    float strength = 2.5f;

    HYP_FIELD(Property = "Hold", Serialize = true, Editor = true)
    float hold = 0.4f;

    HYP_FIELD(Property = "MinSpeed", Serialize = true, Editor = true)
    float minSpeed = 0.8f;
};

// falling backwards, or hit in the head, the hands cover the head and the chin tucks
HYP_STRUCT()
struct RagdollProtectHeadSettings
{
    HYP_STRUCT_BODY(RagdollProtectHeadSettings);

    HYP_FIELD(Property = "Enabled", Serialize = true, Editor = true)
    bool enabled = true;

    HYP_FIELD(Property = "Priority", Serialize = true, Editor = true)
    float priority = 60.0f;

    HYP_FIELD(Property = "FadeIn", Serialize = true, Editor = true)
    float fadeIn = 0.7f;

    HYP_FIELD(Property = "FadeOut", Serialize = true, Editor = true)
    float fadeOut = 0.8f;

    HYP_FIELD(Property = "ReactionDelay", Serialize = true, Editor = true)
    float reactionDelay = 0.1f;

    HYP_FIELD(Property = "Strength", Serialize = true, Editor = true)
    float strength = 2.5f;

    HYP_FIELD(Property = "Tuck", Serialize = true, Editor = true)
    float tuck = 25.0f;

    HYP_FIELD(Property = "MinBackward", Serialize = true, Editor = true)
    float minBackward = 0.3f;

    HYP_FIELD(Property = "Horizon", Serialize = true, Editor = true)
    float horizon = 0.5f;

    HYP_FIELD(Property = "Hold", Serialize = true, Editor = true)
    float hold = 0.6f;

    HYP_FIELD(Property = "HitTime", Serialize = true, Editor = true)
    float hitTime = 0.8f;
};

// a long fall windmills the arms and pedals the legs
HYP_STRUCT()
struct RagdollHighFallSettings
{
    HYP_STRUCT_BODY(RagdollHighFallSettings);

    HYP_FIELD(Property = "Enabled", Serialize = true, Editor = true)
    bool enabled = true;

    HYP_FIELD(Property = "Priority", Serialize = true, Editor = true)
    float priority = 40.0f;

    HYP_FIELD(Property = "FadeIn", Serialize = true, Editor = true)
    float fadeIn = 0.5f;

    HYP_FIELD(Property = "FadeOut", Serialize = true, Editor = true)
    float fadeOut = 0.6f;

    HYP_FIELD(Property = "ReactionDelay", Serialize = true, Editor = true)
    float reactionDelay = 0.15f;

    HYP_FIELD(Property = "MinHeight", Serialize = true, Editor = true)
    float minHeight = 1.5f;

    HYP_FIELD(Property = "MinSpeed", Serialize = true, Editor = true)
    float minSpeed = 4.0f;

    HYP_FIELD(Property = "MinAirTime", Serialize = true, Editor = true)
    float minAirTime = 0.25f;

    HYP_FIELD(Property = "ArmSwing", Serialize = true, Editor = true)
    float armSwing = 70.0f;

    HYP_FIELD(Property = "LegSwing", Serialize = true, Editor = true)
    float legSwing = 40.0f;

    HYP_FIELD(Property = "Frequency", Serialize = true, Editor = true)
    float frequency = 1.6f;

    HYP_FIELD(Property = "Strength", Serialize = true, Editor = true)
    float strength = 2.0f;
};

// a grounded body curls and squirms
HYP_STRUCT()
struct RagdollWritheSettings
{
    HYP_STRUCT_BODY(RagdollWritheSettings);

    HYP_FIELD(Property = "Enabled", Serialize = true, Editor = true)
    bool enabled = true;

    HYP_FIELD(Property = "Priority", Serialize = true, Editor = true)
    float priority = 10.0f;

    HYP_FIELD(Property = "FadeIn", Serialize = true, Editor = true)
    float fadeIn = 0.6f;

    HYP_FIELD(Property = "FadeOut", Serialize = true, Editor = true)
    float fadeOut = 0.5f;

    HYP_FIELD(Property = "Strength", Serialize = true, Editor = true)
    float strength = 0.6f;

    HYP_FIELD(Property = "Amplitude", Serialize = true, Editor = true)
    float amplitude = 25.0f;

    HYP_FIELD(Property = "Frequency", Serialize = true, Editor = true)
    float frequency = 0.5f;

    HYP_FIELD(Property = "Curl", Serialize = true, Editor = true)
    float curl = 15.0f;
};

// strength fades over a few seconds after death, then the body goes limp
HYP_STRUCT()
struct RagdollDyingSettings
{
    HYP_STRUCT_BODY(RagdollDyingSettings);

    HYP_FIELD(Property = "Enabled", Serialize = true, Editor = true)
    bool enabled = true;

    HYP_FIELD(Property = "Priority", Serialize = true, Editor = true)
    float priority = 100.0f;

    HYP_FIELD(Property = "FadeIn", Serialize = true, Editor = true)
    float fadeIn = 0.0f;

    HYP_FIELD(Property = "FadeOut", Serialize = true, Editor = true)
    float fadeOut = 0.0f;

    HYP_FIELD(Property = "MinTime", Serialize = true, Editor = true)
    float minTime = 1.0f;

    HYP_FIELD(Property = "MaxTime", Serialize = true, Editor = true)
    float maxTime = 3.0f;

    HYP_FIELD(Property = "HeadTime", Serialize = true, Editor = true)
    float headTime = 0.25f;

    HYP_FIELD(Property = "BalanceStrength", Serialize = true, Editor = true)
    float balanceStrength = 0.5f;
};

HYP_STRUCT()
struct RagdollBehaviourSettings
{
    HYP_STRUCT_BODY(RagdollBehaviourSettings);

    HYP_FIELD(Property = "Fallen", Serialize = true, Editor = true)
    RagdollFallenSettings fallen;

    HYP_FIELD(Property = "Stagger", Serialize = true, Editor = true)
    RagdollStaggerSettings stagger;

    HYP_FIELD(Property = "ArmBalance", Serialize = true, Editor = true)
    RagdollArmBalanceSettings armBalance;

    HYP_FIELD(Property = "Shot", Serialize = true, Editor = true)
    RagdollShotSettings shot;

    HYP_FIELD(Property = "Brace", Serialize = true, Editor = true)
    RagdollBraceSettings brace;

    HYP_FIELD(Property = "ProtectHead", Serialize = true, Editor = true)
    RagdollProtectHeadSettings protectHead;

    HYP_FIELD(Property = "HighFall", Serialize = true, Editor = true)
    RagdollHighFallSettings highFall;

    HYP_FIELD(Property = "Writhe", Serialize = true, Editor = true)
    RagdollWritheSettings writhe;

    HYP_FIELD(Property = "Dying", Serialize = true, Editor = true)
    RagdollDyingSettings dying;
};

} // namespace Hyperion
