/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <ActiveRagdoll/RagdollBehaviour.hpp>
#include <ActiveRagdoll/RagdollBehaviourSettings.hpp>

#include <random>

namespace Hyperion {

// time since a falling body touched down, for behaviours that hold briefly after landing
struct RagdollLandingHold
{
    bool hasLanded = false;
    float time = 0.0f;

    HYP_FORCE_INLINE void Reset()
    {
        *this = RagdollLandingHold {};
    }

    // true once `hold` seconds have passed since landing
    HYP_FORCE_INLINE bool Advance(float deltaTime, float hold)
    {
        time += deltaTime;

        return time >= hold;
    }
};

class RagdollFallenBehaviour final : public TRagdollBehaviour<RagdollFallenSettings>
{
public:
    RagdollFallenBehaviour();

    void Init(const RagdollRig& rig, uint32 bodyCount) override;
    void OnStart(const RagdollBehaviourContext& ctx) override;
    void Update(const RagdollBehaviourContext& ctx) override;
    void Contribute(const RagdollBehaviourContext& ctx, RagdollBehaviourOutput& out) const override;

    bool NeedsFreePose() const override
    {
        return true;
    }

private:
    void CaptureFollowPose(const RagdollBehaviourContext& ctx, float blend);

    Array<JPH::Quat> m_follow;
};

class RagdollStaggerBehaviour final : public TRagdollBehaviour<RagdollStaggerSettings>
{
public:
    RagdollStaggerBehaviour();

    void OnEvent(const RagdollBehaviourContext& ctx, const RagdollEvent& event) override;
    void Update(const RagdollBehaviourContext& ctx) override;
    void Contribute(const RagdollBehaviourContext& ctx, RagdollBehaviourOutput& out) const override;
};

class RagdollArmBalanceBehaviour final : public TRagdollBehaviour<RagdollArmBalanceSettings>
{
public:
    RagdollArmBalanceBehaviour();

    void Update(const RagdollBehaviourContext& ctx) override;
    void Contribute(const RagdollBehaviourContext& ctx, RagdollBehaviourOutput& out) const override;

private:
    float m_fore = 0.0f;
    float m_side = 0.0f;
};

class RagdollShotBehaviour final : public TRagdollBehaviour<RagdollShotSettings>
{
public:
    RagdollShotBehaviour();

    void Init(const RagdollRig& rig, uint32 bodyCount) override;
    void OnEvent(const RagdollBehaviourContext& ctx, const RagdollEvent& event) override;
    void Update(const RagdollBehaviourContext& ctx) override;
    void Contribute(const RagdollBehaviourContext& ctx, RagdollBehaviourOutput& out) const override;

private:
    struct Wound
    {
        uint32 body = 0;
        JPH::Vec3 localPoint = JPH::Vec3::sZero();
        JPH::Vec3 localNormal = JPH::Vec3::sAxisY();
        int32 arm = -1;
        float time = 0.0f;
    };

    void LoosenAround(const RagdollBehaviourContext& ctx, uint32 body);
    void StartGrab(const RagdollBehaviourContext& ctx, const RagdollEvent& event);
    void LookAtWound(const RagdollBehaviourContext& ctx, RagdollBehaviourOutput& out) const;

    Array<float> m_loosen;
    Wound m_wound;
    float m_flinchTime = -1.0f;
    float m_flinchSide = 0.0f;
    float m_flinchPitch = 0.0f;
    int32 m_lookBody = -1;
    JPH::Vec3 m_lookLocal = JPH::Vec3::sZero();
    float m_lookTime = 0.0f;
};

class RagdollBraceBehaviour final : public TRagdollBehaviour<RagdollBraceSettings>
{
public:
    RagdollBraceBehaviour();

    void OnEvent(const RagdollBehaviourContext& ctx, const RagdollEvent& event) override;
    void OnStart(const RagdollBehaviourContext& ctx) override;
    void Update(const RagdollBehaviourContext& ctx) override;
    void Contribute(const RagdollBehaviourContext& ctx, RagdollBehaviourOutput& out) const override;

private:
    RagdollLandingHold m_landing;
    bool m_hasTarget = false;
    JPH::Vec3 m_point = JPH::Vec3::sZero();
    JPH::Vec3 m_normal = JPH::Vec3::sAxisY();
};

class RagdollProtectHeadBehaviour final : public TRagdollBehaviour<RagdollProtectHeadSettings>
{
public:
    RagdollProtectHeadBehaviour();

    void OnEvent(const RagdollBehaviourContext& ctx, const RagdollEvent& event) override;
    void OnStart(const RagdollBehaviourContext& ctx) override;
    void Update(const RagdollBehaviourContext& ctx) override;
    void Contribute(const RagdollBehaviourContext& ctx, RagdollBehaviourOutput& out) const override;

private:
    bool IsFallingBackward(const RagdollBehaviourContext& ctx) const;

    float m_duration = -1.0f;
    RagdollLandingHold m_landing;
};

class RagdollHighFallBehaviour final : public TRagdollBehaviour<RagdollHighFallSettings>
{
public:
    RagdollHighFallBehaviour();

    void OnEvent(const RagdollBehaviourContext& ctx, const RagdollEvent& event) override;
    void OnStart(const RagdollBehaviourContext& ctx) override;
    void Update(const RagdollBehaviourContext& ctx) override;
    void Contribute(const RagdollBehaviourContext& ctx, RagdollBehaviourOutput& out) const override;

    bool NeedsFreePose() const override
    {
        return true;
    }

private:
    bool IsHighFall(const RagdollBehaviourContext& ctx) const;

    float m_groundedTime = 0.0f;
};

class RagdollWritheBehaviour final : public TRagdollBehaviour<RagdollWritheSettings>
{
public:
    RagdollWritheBehaviour();

    void OnEvent(const RagdollBehaviourContext& ctx, const RagdollEvent& event) override;
    void Update(const RagdollBehaviourContext& ctx) override;
    void Contribute(const RagdollBehaviourContext& ctx, RagdollBehaviourOutput& out) const override;

    bool NeedsFreePose() const override
    {
        return true;
    }
};

class RagdollDyingBehaviour final : public TRagdollBehaviour<RagdollDyingSettings>
{
public:
    RagdollDyingBehaviour();

    void OnEvent(const RagdollBehaviourContext& ctx, const RagdollEvent& event) override;
    void OnStart(const RagdollBehaviourContext& ctx) override;
    void Update(const RagdollBehaviourContext& ctx) override;
    void Contribute(const RagdollBehaviourContext& ctx, RagdollBehaviourOutput& out) const override;

    float GetStrengthScale() const;

    HYP_FORCE_INLINE bool IsFinished() const
    {
        return m_isFinished;
    }

private:
    std::minstd_rand m_random;
    float m_duration = 1.0f;
    bool m_isHeadShot = false;
    bool m_isFinished = false;
};

} // namespace Hyperion
