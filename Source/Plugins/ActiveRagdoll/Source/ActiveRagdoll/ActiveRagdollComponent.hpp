/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <ActiveRagdoll/RagdollBalance.hpp>
#include <ActiveRagdoll/RagdollBehaviourSettings.hpp>

#include <Core/Reflection/Handle.hpp>
#include <Core/Reflection/ObjectMacros.hpp>

namespace Hyperion {

class RagdollDefinition;

HYP_ENUM()
enum class ActiveRagdollState : uint8
{
    Animated = 0,
    Fallen,
    Limp
};

HYP_STRUCT()
struct ActiveRagdollShoveSettings
{
    HYP_STRUCT_BODY(ActiveRagdollShoveSettings);

    HYP_FIELD(Property = "HitShoveScale", Serialize = true, Editor = true)
    float hitShoveScale = 10.0f;

    HYP_FIELD(Property = "MinShoveSpeed", Serialize = true, Editor = true)
    float minShoveSpeed = 0.3f;

    HYP_FIELD(Property = "MaxShoveSpeed", Serialize = true, Editor = true)
    float maxShoveSpeed = 4.0f;

    // least a shove adds while the body is already moving its way, as a fraction of a full shove
    HYP_FIELD(Property = "RepeatShoveFraction", Serialize = true, Editor = true)
    float repeatShoveFraction = 0.3f;

    // fastest drift balance lets a shoved body stagger at, however many hits land (m/s)
    HYP_FIELD(Property = "MaxStaggerSpeed", Serialize = true, Editor = true)
    float maxStaggerSpeed = 0.4f;

    // a body (a thrown prop, another ragdoll) running into this one at least this fast knocks it (m/s)
    HYP_FIELD(Property = "MinImpactSpeed", Serialize = true, Editor = true)
    float minImpactSpeed = 1.5f;

    // the knock is the other body's momentum, its mass capped at this (kg)
    HYP_FIELD(Property = "MaxKnockMass", Serialize = true, Editor = true)
    float maxKnockMass = 80.0f;
};

HYP_STRUCT(Component, Label = "Active Ragdoll Component", Editor = true)
struct ACTIVERAGDOLL_API ActiveRagdollComponent
{
    HYP_STRUCT_BODY(ActiveRagdollComponent);

    // the plugin's humanoid ragdoll when unset
    HYP_FIELD(Property = "Definition", Serialize = true, Editor = true)
    Handle<RagdollDefinition> definition;

    HYP_FIELD(Property = "Strength", Serialize = true, Editor = true)
    float strength = 1.0f;

    // how hard the pelvis is held to the animation while the body tracks it
    HYP_FIELD(Property = "RootAssist", Serialize = true, Editor = true)
    float rootAssist = 1.0f;

    HYP_FIELD(Property = "Balance", Serialize = true, Editor = true)
    bool balance = true;

    HYP_FIELD(Property = "BalanceAssist", Serialize = true, Editor = true)
    RagdollBalanceAssist balanceAssist;

    HYP_FIELD(Property = "Shove", Serialize = true, Editor = true)
    ActiveRagdollShoveSettings shove;

    HYP_FIELD(Property = "Behaviours", Serialize = true, Editor = true)
    RagdollBehaviourSettings behaviours;

    HYP_FIELD(Property = "DebugDraw", Serialize = true, Editor = true)
    bool debugDraw = false;

    HYP_FIELD(Property = "State", Transient, Editor = false)
    ActiveRagdollState state = ActiveRagdollState::Animated;

    HYP_FIELD(Property = "IsDying", Transient, Editor = false)
    bool isDying = false;
};

} // namespace Hyperion
