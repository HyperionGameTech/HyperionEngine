/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <ActiveRagdoll/Ragdoll.hpp>
#include <ActiveRagdoll/RagdollReactions.hpp>

namespace Hyperion {

class DebugDrawCommandList;

// senses the ragdoll each frame, sends events to the behaviours and mixes their poses into its motor targets
class RagdollBehaviours
{
public:
    static constexpr uint32 BehaviourCount = 9;

    RagdollBehaviours();
    RagdollBehaviours(const RagdollBehaviours& other) = delete;
    RagdollBehaviours& operator=(const RagdollBehaviours& other) = delete;

    bool Init(Ragdoll& ragdoll);
    void Reset();
    void ApplySettings(const RagdollBehaviourSettings& settings);

    void HandleEvent(Ragdoll& ragdoll, const RagdollEvent& event);
    void Update(Ragdoll& ragdoll, float deltaTime);

    void SetFreePose(bool isFreePose);
    void SetAlive(bool isAlive);

    RagdollBehaviour* Find(Name name) const;
    bool Trigger(Ragdoll& ragdoll, Name name);
    bool Stop(Name name);
    bool IsRunning(Name name) const;

    bool IsIdle() const;
    bool HadEvent(RagdollEventType type) const;

    void DebugDraw(DebugDrawCommandList& list) const;

    HYP_FORCE_INLINE bool IsValid() const
    {
        return m_rig != nullptr && m_rig->isValid;
    }

    HYP_FORCE_INLINE float GetStrength(uint32 body) const
    {
        if (body >= m_mixStrength.Size())
        {
            return 1.0f;
        }

        return m_mixStrength[body];
    }

    HYP_FORCE_INLINE RagdollBehaviourBalance GetBalance() const
    {
        return m_mixBalance;
    }

    HYP_FORCE_INLINE bool HasPendingEvents() const
    {
        return m_pendingEvents.Any();
    }

    HYP_FORCE_INLINE bool IsDying() const
    {
        return m_dying.IsActive();
    }

    HYP_FORCE_INLINE bool IsDeathComplete() const
    {
        return m_dying.IsFinished();
    }

    HYP_FORCE_INLINE bool IsFreePose() const
    {
        return m_context.isFreePose;
    }

    HYP_FORCE_INLINE const RagdollBehaviourContext& GetContext() const
    {
        return m_context;
    }

    HYP_FORCE_INLINE const RagdollBehaviour* GetBehaviour(uint32 index) const
    {
        return m_behaviours[index];
    }

private:
    bool CanRun(const Ragdoll& ragdoll) const;

    void Sense(Ragdoll& ragdoll, float deltaTime);
    void SenseChestFrame(const Ragdoll& ragdoll);
    void PredictImpact(const Ragdoll& ragdoll);
    void EmitSupportEvents(float deltaTime);
    void Dispatch(const RagdollEvent& event);
    void DeliverPendingEvents(float deltaTime);
    void SortByPriority();

    void Resolve(Ragdoll& ragdoll);
    void ClearMix();
    void Mix();
    void FoldOutput(const RagdollBehaviourOutput& output, float weight);
    void FoldOverride(uint32 body, JPH::QuatArg pose, float weight);
    void SolveArm(uint32 arm, const RagdollBehaviourGoal& goal, float weight);

    RagdollFallenBehaviour m_fallen;
    RagdollStaggerBehaviour m_stagger;
    RagdollArmBalanceBehaviour m_armBalance;
    RagdollShotBehaviour m_shot;
    RagdollBraceBehaviour m_brace;
    RagdollProtectHeadBehaviour m_protectHead;
    RagdollHighFallBehaviour m_highFall;
    RagdollWritheBehaviour m_writhe;
    RagdollDyingBehaviour m_dying;

    RagdollBehaviour* m_behaviours[BehaviourCount];
    RagdollBehaviour* m_order[BehaviourCount];

    const RagdollRig* m_rig = nullptr;
    RagdollBehaviourContext m_context;
    Array<Ragdoll::BodyState> m_samples;
    RagdollBehaviourOutput m_output;

    // the mixed pose layers, copied into the ragdoll by Resolve
    Array<Ragdoll::BodyLayer> m_mixLayers;
    Array<float> m_mixStrength;
    RagdollBehaviourBalance m_mixBalance = RagdollBehaviourBalance::Unset;
    float m_mixGravity = 1.0f;
    RagdollBehaviourGoal m_mixGoals[RagdollRig::LimbCount];

    // an event on its way to a behaviour with a reaction delay; points are kept on the body they hit, which moves meanwhile
    struct PendingEvent
    {
        RagdollEvent event;
        JPH::Vec3 localPoint = JPH::Vec3::sZero();
        JPH::Vec3 localDirection = JPH::Vec3::sZero();
        uint32 behaviour = 0;
        float timeLeft = 0.0f;
    };

    float m_unsupportedTime = 0.0f;
    float m_offGroundTime = 0.0f;
    bool m_sentLostSupport = false;
    bool m_sentOnGround = false;
    uint32 m_frameEvents = 0;

    Array<PendingEvent> m_pendingEvents;
    std::minstd_rand m_random;
};

} // namespace Hyperion
