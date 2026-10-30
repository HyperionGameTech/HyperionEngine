/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <ActiveRagdoll/RagdollBalance.hpp>
#include <ActiveRagdoll/RagdollDefinition.hpp>
#include <ActiveRagdoll/RagdollRig.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Name/Name.hpp>

namespace Hyperion {

class Ragdoll;

HYP_ENUM()
enum class RagdollEventType : uint8
{
    Hit = 0,
    Impact,
    LostSupport,
    OnGround,
    Died,
    // a shove without damage, such as a character walking into the body
    Push
};

struct RagdollEvent
{
    RagdollEventType type = RagdollEventType::Hit;
    int32 body = -1;
    JPH::Vec3 point = JPH::Vec3::sZero();
    JPH::Vec3 direction = JPH::Vec3::sZero();
    float magnitude = 0.0f;
    float damage = 0.0f;
};

enum class RagdollBehaviourBalance : uint8
{
    Unset,
    Off,
    On
};

struct RagdollBehaviourBody
{
    JPH::Vec3 origin = JPH::Vec3::sZero();
    JPH::Vec3 center = JPH::Vec3::sZero();
    JPH::Vec3 velocity = JPH::Vec3::sZero();
    JPH::Quat orientation = JPH::Quat::sIdentity();
    bool hasContact = false;
    bool isOnGround = false;
};

struct RagdollBehaviourContext
{
    const Ragdoll* ragdoll = nullptr;
    const RagdollRig* rig = nullptr;
    const RagdollBalanceStatus* balance = nullptr;

    float deltaTime = 0.0f;
    float time = 0.0f;

    Array<RagdollBehaviourBody> bodies;

    JPH::Vec3 up = JPH::Vec3::sAxisY();
    JPH::Vec3 centerOfMass = JPH::Vec3::sZero();
    JPH::Vec3 velocity = JPH::Vec3::sZero();
    JPH::Vec3 forward = JPH::Vec3::sAxisZ();
    JPH::Vec3 right = JPH::Vec3::sAxisX();
    float gravity = 9.81f;

    bool areFeetSupported = false;
    bool isBodyOnGround = false;
    bool isAirborne = false;
    float airTime = 0.0f;
    float groundTime = 0.0f;
    float dropHeight = 0.0f;

    bool isImpactPredicted = false;
    JPH::Vec3 impactPoint = JPH::Vec3::sZero();
    JPH::Vec3 impactNormal = JPH::Vec3::sAxisY();
    float impactTime = 0.0f;

    bool isFreePose = false;
    bool isBalancing = false;
    bool isAlive = true;

    HYP_FORCE_INLINE uint32 GetBodyCount() const
    {
        return uint32(bodies.Size());
    }

    HYP_FORCE_INLINE JPH::Vec3 Horizontal(JPH::Vec3Arg v) const
    {
        return v - up * v.Dot(up);
    }

    HYP_FORCE_INLINE JPH::Vec3 GetChestUp() const
    {
        return forward.Cross(right);
    }

    JPH::Vec3 ToBodyLocal(uint32 body, JPH::Vec3Arg point) const;
    JPH::Vec3 FromBodyLocal(uint32 body, JPH::Vec3Arg local) const;
    JPH::Vec3 ToBodyDirection(uint32 body, JPH::Vec3Arg localDirection) const;
    JPH::Vec3 ToLocalDirection(uint32 body, JPH::Vec3Arg direction) const;
    bool IsArmBody(uint32 arm, uint32 body) const;
    int32 FindArm(uint32 body) const;
};

struct RagdollBehaviourGoal
{
    JPH::Vec3 target = JPH::Vec3::sZero();
    float weight = 0.0f;
};

struct RagdollBehaviourOutput
{
    Array<JPH::Quat> pose;
    Array<float> mask;
    Array<float> strength;
    RagdollBehaviourGoal hands[RagdollRig::LimbCount];
    RagdollBehaviourBalance balance = RagdollBehaviourBalance::Unset;
    float gravityScale = 1.0f;

    // additive poses rotate on top of the tracked target, override poses replace it
    bool isAdditive = true;

    void Init(uint32 bodyCount);
    void Reset();

    void AddWorldRotation(const RagdollBehaviourContext& ctx, uint32 body, JPH::Vec3Arg worldAxis, float angle, float maskValue = 1.0f);
    void ScaleStrength(uint32 body, float scale);
    void ScaleAllStrengths(float scale);
    void ScaleArmStrengths(const RagdollRig& rig, float scale);
};

struct RagdollBehaviourTiming
{
    bool enabled = true;
    float priority = 0.0f;
    float fadeIn = 0.1f;
    float fadeOut = 0.3f;
};

class RagdollBehaviour
{
public:
    explicit RagdollBehaviour(Name name);
    RagdollBehaviour(const RagdollBehaviour& other) = delete;
    RagdollBehaviour& operator=(const RagdollBehaviour& other) = delete;
    virtual ~RagdollBehaviour() = default;

    virtual RagdollBehaviourTiming GetTiming() const = 0;

    // how long events take to reach this behaviour, as a person's reaction time
    virtual float GetReactionDelay() const
    {
        return 0.0f;
    }

    virtual void OnEvent(const RagdollBehaviourContext& ctx, const RagdollEvent& event)
    {
    }

    virtual void Update(const RagdollBehaviourContext& ctx)
    {
    }

    virtual void Contribute(const RagdollBehaviourContext& ctx, RagdollBehaviourOutput& out) const = 0;

    virtual void Init(const RagdollRig& rig, uint32 bodyCount)
    {
    }

    virtual void OnStart(const RagdollBehaviourContext& ctx)
    {
    }

    virtual bool NeedsFreePose() const
    {
        return false;
    }

    void Start(const RagdollBehaviourContext& ctx);
    void Stop();
    void Reset();
    void Advance(float deltaTime);

    HYP_FORCE_INLINE Name GetName() const
    {
        return m_name;
    }

    HYP_FORCE_INLINE bool IsEnabled() const
    {
        return GetTiming().enabled;
    }

    HYP_FORCE_INLINE float GetPriority() const
    {
        return GetTiming().priority;
    }

    HYP_FORCE_INLINE bool IsActive() const
    {
        return m_isActive;
    }

    HYP_FORCE_INLINE bool IsRunning() const
    {
        return m_isActive || m_weight > 0.0f;
    }

    HYP_FORCE_INLINE float GetWeight() const
    {
        return m_weight;
    }

    HYP_FORCE_INLINE float GetTime() const
    {
        return m_time;
    }

protected:
    void StartIfIdle(const RagdollBehaviourContext& ctx);

    float m_time = 0.0f;

private:
    Name m_name;
    float m_weight = 0.0f;
    bool m_isActive = false;
};

// a behaviour whose settings live on the ragdoll component
template <class SettingsType>
class TRagdollBehaviour : public RagdollBehaviour
{
public:
    explicit TRagdollBehaviour(Name name)
        : RagdollBehaviour(name)
    {
    }

    RagdollBehaviourTiming GetTiming() const override
    {
        return RagdollBehaviourTiming { settings.enabled, settings.priority, settings.fadeIn, settings.fadeOut };
    }

    float GetReactionDelay() const override
    {
        if constexpr (requires { settings.reactionDelay; })
        {
            return settings.reactionDelay;
        }
        else
        {
            return 0.0f;
        }
    }

    void SetSettings(const SettingsType& newSettings)
    {
        const bool wasEnabled = settings.enabled;

        settings = newSettings;

        if (wasEnabled && !settings.enabled)
        {
            Reset();
        }
    }

    SettingsType settings;
};

float GetBehaviourNoise(float time, float seed);

} // namespace Hyperion
