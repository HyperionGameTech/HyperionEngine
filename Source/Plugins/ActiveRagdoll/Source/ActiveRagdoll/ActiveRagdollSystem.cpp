/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <ActiveRagdoll/ActiveRagdollSystem.hpp>
#include <ActiveRagdoll/RagdollBehaviours.hpp>
#include <ActiveRagdoll/RagdollContacts.hpp>
#include <ActiveRagdoll/RagdollDefinition.hpp>

#include <Animation/AnimationComponent.hpp>
#include <Animation/AnimationPose.hpp>
#include <Animation/AnimationSystem.hpp>

#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Animation/Skeleton.hpp>
#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/TransformComponent.hpp>
#include <Scene/Components/CharacterControllerComponent.hpp>

#include <Physics/PhysicsWorld.hpp>

#include <Rendering/DebugDrawer.hpp>

#include <Framework/GameState.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>

#include <limits>

#include <ActiveRagdollSystem.generated.inl>

namespace Hyperion {

using namespace RagdollMath;

namespace {

static constexpr float AssistFadeTime = 0.5f;
static constexpr float MinPoweredTime = 1.0f;
static constexpr float BlendOutTime = 0.4f;
static constexpr float GroundProbe = 2.0f;

static constexpr uint32 HitChainLength = 3;
static constexpr float HitChainFalloff[HitChainLength] = { 1.0f, 0.6f, 0.3f };
static constexpr float HitChainMaxSpeed = 2.0f;

} // namespace

struct ActiveRagdollInstance
{
    Ragdoll ragdoll;
    RagdollBehaviours behaviours;

    Entity* entity = nullptr;
    Handle<RagdollDefinition> definition;
    WeakHandle<Skeleton> source;
    uint32 sourceRevision = 0;
    Array<RagdollResolvedBody> bodies;

    Array<Transform> animatedPose;
    Array<Mat4f> animatedModels;
    Array<Mat4f> animatedSkinning;
    Array<Transform> pose;

    float strength = 1.0f;
    float rootAssist = 1.0f;
    bool balance = true;
    RagdollBalanceAssist balanceAssist;
    ActiveRagdollShoveSettings shove;
    bool debugDraw = false;

    ActiveRagdollState state = ActiveRagdollState::Animated;
    bool isDying = false;
    bool isTeleported = true;
    float assistFade = 1.0f;
    float poweredTime = 0.0f;
    float physicsBlend = 1.0f;

    // the pelvis's horizontal offset from the entity, kept while the body is down so the entity follows it
    JPH::Vec3 pelvisOffset = JPH::Vec3::sZero();
    bool followsPelvis = false;

    // the skeleton shows the pose written last frame, which nothing else replaces when no animation poses the entity
    bool isPoseWritten = false;

    bool IsPowered() const
    {
        return ragdoll.IsCreated() && ragdoll.GetBodyCount() > 0 && ragdoll.GetBodyMode(0) == RagdollBodyMode::Powered;
    }

    bool IsAlive() const
    {
        return !isDying && state != ActiveRagdollState::Limp;
    }

    const Mat4f& GetEntityWorldMatrix() const
    {
        return entity->GetWorldMatrix();
    }

    void ApplyStrengths()
    {
        if (!ragdoll.IsCreated())
        {
            return;
        }

        for (uint32 i = 0; i < ragdoll.GetBodyCount(); i++)
        {
            ragdoll.SetBodyStrength(i, strength * behaviours.GetStrength(i));
        }

        float assist = 0.0f;

        if (state == ActiveRagdollState::Animated && !isDying)
        {
            assist = rootAssist * MathUtil::Min(behaviours.GetStrength(0), 1.0f) * assistFade;
        }

        ragdoll.SetRootAssist(assist);
        ragdoll.SetBalanceAssist(balanceAssist);
    }

    void SetPowered(bool powered)
    {
        if (!ragdoll.IsCreated() || IsPowered() == powered)
        {
            return;
        }

        if (powered)
        {
            ragdoll.SetAllBodyModes(RagdollBodyMode::Powered);
        }
        else
        {
            ragdoll.SetAllBodyModes(RagdollBodyMode::Kinematic);
        }

        physicsBlend = 1.0f;
        ragdoll.SetAllBodyBlends(physicsBlend);
    }

    bool WakePhysics()
    {
        if (!ragdoll.IsCreated())
        {
            return false;
        }

        SetPowered(true);
        poweredTime = 0.0f;

        return true;
    }

    void StartFollowingPelvis()
    {
        const JPH::Vec3 up = ragdoll.GetGravityFrame().up;
        const JPH::Vec3 offset = ragdoll.GetBodyOrigin(0) - ToJolt(entity->GetWorldTranslation());

        pelvisOffset = offset - up * offset.Dot(up);
        followsPelvis = !entity->TryGetComponent<CharacterControllerComponent>();
    }

    void FollowPelvis()
    {
        if (!followsPelvis)
        {
            return;
        }

        const JPH::Vec3 up = ragdoll.GetGravityFrame().up;
        const JPH::Vec3 pelvis = ragdoll.GetBodyOrigin(0);
        const JPH::Vec3 current = ToJolt(entity->GetWorldTranslation());

        JPH::Vec3 position = pelvis - pelvisOffset;
        position = position - up * position.Dot(up) + up * current.Dot(up);

        JPH::Vec3 groundPoint;
        JPH::Vec3 groundNormal;

        if (ragdoll.CastRayStatic(position - up * position.Dot(up) + up * pelvis.Dot(up), -up * GroundProbe, groundPoint, groundNormal))
        {
            position = position - up * position.Dot(up) + up * groundPoint.Dot(up);
        }

        entity->SetWorldTranslation(FromJolt(position), TransformChangeType::Simulation);
    }

    // moves the entity to where balancing left the pelvis, turned to match, so the animation picks up from there
    void PlaceRootUnderPelvis()
    {
        if (!ragdoll.IsCreated() || entity->TryGetComponent<CharacterControllerComponent>())
        {
            return;
        }

        const GravityFrame frame = ragdoll.GetGravityFrame();
        const JPH::Vec3 pelvis = ragdoll.GetBodyOrigin(0);
        const JPH::Vec3 rootPosition = ToJolt(entity->GetWorldTranslation());
        JPH::Vec3 target = ragdoll.GetBodyTargetOrigin(0);

        // balancing lets the body turn as it stumbles, so turn the root to match or the animation twists it back
        const JPH::Vec3 facing = ragdoll.GetPelvisHeading(false);
        const JPH::Vec3 animatedFacing = ragdoll.GetPelvisHeading(true);

        if (facing.LengthSq() > 0.0f && animatedFacing.LengthSq() > 0.0f)
        {
            const float turn = std::atan2(animatedFacing.Cross(facing).Dot(frame.up), animatedFacing.Dot(facing));
            const JPH::Quat turnRotation = JPH::Quat::sRotation(frame.up, turn);

            target = rootPosition + turnRotation * (target - rootPosition);

            entity->SetWorldRotation(FromJolt((turnRotation * ToJolt(entity->GetWorldRotation())).Normalized()), TransformChangeType::Simulation);
        }

        const JPH::Vec3 offset = frame.Horizontal(pelvis - target);
        JPH::Vec3 position = rootPosition + offset;

        JPH::Vec3 groundPoint;
        JPH::Vec3 groundNormal;

        if (ragdoll.CastRayStatic(frame.Horizontal(position) + frame.up * pelvis.Dot(frame.up), -frame.up * GroundProbe, groundPoint, groundNormal))
        {
            position = frame.Horizontal(position) + frame.up * groundPoint.Dot(frame.up);
        }

        entity->SetWorldTranslation(FromJolt(position), TransformChangeType::Simulation);
    }

    void ActivateBalance()
    {
        if (!balance || state != ActiveRagdollState::Animated || !ragdoll.IsCreated())
        {
            return;
        }

        ragdoll.SetBalancing(true);

        if (ragdoll.IsBalancing())
        {
            assistFade = 0.0f;
        }
    }

    void ReturnToAnimation()
    {
        PlaceRootUnderPelvis();

        ragdoll.RebaseTargets();
        ragdoll.SetBalancing(false);
        assistFade = 0.0f;
        isTeleported = true;
    }

    void EnterFallen()
    {
        if (state != ActiveRagdollState::Animated || !ragdoll.IsCreated())
        {
            return;
        }

        ragdoll.SetBalancing(false);
        ragdoll.SetRootAssist(0.0f);

        if (behaviours.IsValid())
        {
            ragdoll.SetAllBodyModes(RagdollBodyMode::Powered);
        }
        else
        {
            ragdoll.SetAllBodyModes(RagdollBodyMode::Limp);
        }

        ragdoll.SetAllBodyBlends(1.0f);

        state = ActiveRagdollState::Fallen;
        StartFollowingPelvis();

        const RagdollBehaviourContext& ctx = behaviours.GetContext();

        behaviours.SetFreePose(true);
        behaviours.Trigger(ragdoll, NAME("Fallen"));

        RagdollEvent event;
        event.type = RagdollEventType::LostSupport;
        event.magnitude = -ctx.velocity.Dot(ctx.up);

        behaviours.HandleEvent(ragdoll, event);

        ApplyStrengths();
    }

    void EnterLimp()
    {
        if (state == ActiveRagdollState::Limp || !ragdoll.IsCreated())
        {
            return;
        }

        behaviours.Reset();
        ragdoll.ClearBodyLayers();
        ragdoll.GoLimp(GetEntityWorldMatrix());

        if (state == ActiveRagdollState::Animated)
        {
            StartFollowingPelvis();
        }

        state = ActiveRagdollState::Limp;
    }

    void UpdateBalance(float deltaTime)
    {
        if (state != ActiveRagdollState::Animated)
        {
            return;
        }

        if (!balance)
        {
            ragdoll.SetBalancing(false);
            assistFade = 1.0f;

            return;
        }

        const RagdollBehaviourBalance requested = behaviours.GetBalance();
        const bool wantsBalance = requested == RagdollBehaviourBalance::On || (requested == RagdollBehaviourBalance::Unset && behaviours.HasPendingEvents());

        if (!ragdoll.IsBalancing())
        {
            assistFade = MathUtil::Min(assistFade + deltaTime / AssistFadeTime, 1.0f);

            if (wantsBalance)
            {
                ActivateBalance();
            }

            return;
        }

        if (ragdoll.GetBalanceStatus().state == RagdollBalanceState::Fallen)
        {
            // dying bodies skip the fallen behaviours and keep tracking their held pose while their strength fades
            if (isDying)
            {
                ReturnToAnimation();
            }
            else
            {
                EnterFallen();
            }

            return;
        }

        if (!wantsBalance)
        {
            ReturnToAnimation();
        }
    }

    void UpdatePhysicsBlend(float deltaTime)
    {
        if (state != ActiveRagdollState::Animated || !IsPowered())
        {
            return;
        }

        poweredTime += deltaTime;

        const bool isSettled = !ragdoll.IsBalancing() && !isDying && poweredTime >= MinPoweredTime && behaviours.IsIdle();
        const float blendStep = deltaTime / BlendOutTime;

        if (isSettled)
        {
            physicsBlend = MathUtil::Max(physicsBlend - blendStep, 0.0f);
        }
        else
        {
            physicsBlend = MathUtil::Min(physicsBlend + blendStep, 1.0f);
        }

        if (physicsBlend <= 0.0f)
        {
            SetPowered(false);

            return;
        }

        // eased, so the hand-back to the animation starts and lands gently
        ragdoll.SetAllBodyBlends(SmoothStep(physicsBlend));
    }

    void UpdateBehaviours(float deltaTime)
    {
        behaviours.Update(ragdoll, deltaTime);

        if (state == ActiveRagdollState::Animated && IsPowered() && behaviours.HadEvent(RagdollEventType::LostSupport))
        {
            EnterFallen();
        }

        if (isDying && (behaviours.IsDeathComplete() || !behaviours.IsDying()))
        {
            EnterLimp();
        }
    }

    // NPC-side updates run after the physics step, so push the new balance request and strengths now or the step that
    // takes the impulse would still use the old ones
    void ApplyBehaviourOutput()
    {
        UpdateBalance(0.0f);
        ApplyStrengths();
    }

    void PushHitChain(uint32 body, JPH::Vec3Arg impulse, JPH::Vec3Arg point)
    {
        uint32 chain[HitChainLength];
        uint32 chainLength = 0;
        float weightedMass = 0.0f;

        for (int32 i = int32(body); i >= 0 && chainLength < HitChainLength; i = ragdoll.GetParentBody(uint32(i)))
        {
            chain[chainLength] = uint32(i);
            weightedMass += HitChainFalloff[chainLength] * ragdoll.GetBodyMass(uint32(i));
            chainLength++;
        }

        const float length = impulse.Length();

        if (weightedMass <= 0.0f || length <= 1e-5f)
        {
            return;
        }

        // the struck body leads and its parents follow, so a light hand isn't flicked away and snapped back by its own
        // muscles; only the struck body takes the hit off centre and spins
        const float speed = MathUtil::Min(length / weightedMass, HitChainMaxSpeed);
        const JPH::Vec3 velocity = impulse * (speed / length);

        for (uint32 i = 0; i < chainLength; i++)
        {
            const JPH::Vec3 chainImpulse = velocity * (HitChainFalloff[i] * ragdoll.GetBodyMass(chain[i]));

            if (i == 0)
            {
                ragdoll.AddImpulse(ragdoll.GetBodyID(chain[i]), chainImpulse, point);
            }
            else
            {
                ragdoll.AddImpulse(ragdoll.GetBodyID(chain[i]), chainImpulse, ragdoll.GetBodyPosition(chain[i]));
            }
        }
    }

    void Shove(JPH::Vec3Arg impulse, float minSpeed)
    {
        const RagdollRig& rig = ragdoll.GetRig();
        const GravityFrame frame = ragdoll.GetGravityFrame();

        const JPH::Vec3 horizontal = frame.Horizontal(impulse);
        const float length = horizontal.Length();
        const float totalMass = ragdoll.GetTotalMass();

        if (!rig.canBalance || length <= 1e-5f || totalMass <= 0.0f)
        {
            return;
        }

        float movedMass = 0.0f;

        for (uint32 i = 0; i < ragdoll.GetBodyCount(); i++)
        {
            if (!ragdoll.IsFoot(i))
            {
                movedMass += ragdoll.GetBodyMass(i);
            }
        }

        if (movedMass <= 0.0f)
        {
            return;
        }

        const float speed = MathUtil::Clamp(length / totalMass, minSpeed, shove.maxShoveSpeed);
        const JPH::Vec3 direction = horizontal / length;

        JPH::BodyInterface& bodyInterface = ragdoll.GetPhysicsSystem()->GetBodyInterface();
        float momentum = 0.0f;

        for (uint32 i = 0; i < ragdoll.GetBodyCount(); i++)
        {
            if (!ragdoll.IsFoot(i))
            {
                momentum += ragdoll.GetBodyMass(i) * bodyInterface.GetLinearVelocity(ragdoll.GetBodyID(i)).Dot(direction);
            }
        }

        // a hit while the body still moves from the last one only tops its speed back up, so a burst knocks harder than
        // one shot without each hit adding a full shove
        const float targetSpeed = speed * totalMass / movedMass;
        const float currentSpeed = MathUtil::Max(momentum / movedMass, 0.0f);
        const float addedSpeed = MathUtil::Max(targetSpeed - currentSpeed, targetSpeed * shove.repeatShoveFraction);

        // everything above the feet moves together, so the body tips over its planted feet without bending at the hips
        const JPH::Vec3 velocity = direction * addedSpeed;

        for (uint32 i = 0; i < ragdoll.GetBodyCount(); i++)
        {
            if (!ragdoll.IsFoot(i))
            {
                ragdoll.AddImpulse(ragdoll.GetBodyID(i), velocity * ragdoll.GetBodyMass(i), ragdoll.GetBodyPosition(i));
            }
        }

        // balance lets the body keep drifting with the shove for a moment, so it steps with it rather than stopping dead
        JPH::Vec3 stagger = ragdoll.GetStaggerVelocity() + horizontal * (speed / length);
        const float staggerSpeed = stagger.Length();

        if (staggerSpeed > shove.maxStaggerSpeed)
        {
            stagger *= shove.maxStaggerSpeed / staggerSpeed;
        }

        ragdoll.SetStaggerVelocity(stagger);
    }

    uint32 FindNearestBody(JPH::Vec3Arg point) const
    {
        uint32 nearest = 0;
        float nearestDistance = std::numeric_limits<float>::max();

        for (uint32 i = 0; i < ragdoll.GetBodyCount(); i++)
        {
            const float distance = (ragdoll.GetBodyPosition(i) - point).LengthSq();

            if (distance < nearestDistance)
            {
                nearest = i;
                nearestDistance = distance;
            }
        }

        return nearest;
    }

    void Hit(uint32 body, JPH::Vec3Arg point, JPH::Vec3Arg impulse, float damage)
    {
        if (!ragdoll.IsCreated() || body >= ragdoll.GetBodyCount())
        {
            return;
        }

        const JPH::BodyID bodyId = ragdoll.GetBodyID(body);

        if (state == ActiveRagdollState::Limp)
        {
            ragdoll.AddImpulse(bodyId, impulse, point);

            return;
        }

        if (!WakePhysics())
        {
            return;
        }

        const float length = impulse.Length();
        JPH::Vec3 direction = JPH::Vec3::sZero();

        if (length > 1e-5f)
        {
            direction = impulse / length;
        }

        RagdollEvent event;
        event.type = RagdollEventType::Hit;
        event.body = int32(body);
        event.point = point;
        event.direction = direction;
        event.magnitude = length;
        event.damage = damage;

        behaviours.HandleEvent(ragdoll, event);
        ApplyBehaviourOutput();

        if (length <= 1e-5f)
        {
            return;
        }

        if (state != ActiveRagdollState::Animated)
        {
            ragdoll.AddImpulse(bodyId, impulse, point);

            return;
        }

        // shove first, so the speed it tops up to isn't already counted from the struck limbs' push
        Shove(impulse * shove.hitShoveScale, shove.minShoveSpeed);

        PushHitChain(body, impulse, point);
    }

    void OnImpact(const RagdollImpact& impact)
    {
        if (!ragdoll.IsCreated() || state == ActiveRagdollState::Limp)
        {
            return;
        }

        const int32 body = ragdoll.FindBodyIndex(impact.body);

        if (body < 0)
        {
            return;
        }

        if (impact.IsStruck(shove.minImpactSpeed))
        {
            float knockMass = shove.maxKnockMass;

            if (impact.otherMass > 0.0f)
            {
                knockMass = MathUtil::Min(impact.otherMass, shove.maxKnockMass);
            }

            Hit(uint32(body), impact.point, impact.normal * (knockMass * impact.speed), 0.0f);

            return;
        }

        if (!impact.isOtherStatic || impact.speed < shove.minImpactSpeed)
        {
            return;
        }

        RagdollEvent event;
        event.type = RagdollEventType::Impact;
        event.body = body;
        event.point = impact.point;
        event.direction = impact.normal;
        event.magnitude = impact.speed;

        behaviours.HandleEvent(ragdoll, event);
    }

    void Push(JPH::Vec3Arg impulse)
    {
        const float length = impulse.Length();

        if (!IsAlive() || state != ActiveRagdollState::Animated || length <= 1e-5f || !behaviours.IsValid() || !WakePhysics())
        {
            return;
        }

        // pushed at the chest, like a shove, rather than wherever the push touched
        const uint32 chest = ragdoll.GetRig().chest;
        const JPH::Vec3 point = ragdoll.GetBodyPosition(chest);

        RagdollEvent event;
        event.type = RagdollEventType::Push;
        event.body = int32(chest);
        event.point = point;
        event.direction = impulse / length;
        event.magnitude = length;

        behaviours.HandleEvent(ragdoll, event);
        ApplyBehaviourOutput();

        ragdoll.AddImpulse(ragdoll.GetBodyID(chest), impulse, point);

        Shove(impulse, 0.0f);
    }

    void Kill(int32 body)
    {
        if (!ragdoll.IsCreated() || !IsAlive())
        {
            return;
        }

        if (!behaviours.IsValid() || !WakePhysics())
        {
            EnterLimp();

            return;
        }

        isDying = true;
        behaviours.SetAlive(false);

        RagdollEvent event;
        event.type = RagdollEventType::Died;
        event.body = body;

        behaviours.HandleEvent(ragdoll, event);

        if (!behaviours.IsDying())
        {
            EnterLimp();

            return;
        }

        ApplyStrengths();
    }

    bool TriggerBehaviour(Name behaviourName)
    {
        if (!ragdoll.IsCreated() || state == ActiveRagdollState::Limp || !behaviours.IsValid())
        {
            return false;
        }

        if (behaviourName == NAME("Dying"))
        {
            Kill(-1);

            return isDying || state == ActiveRagdollState::Limp;
        }

        const RagdollBehaviour* behaviour = behaviours.Find(behaviourName);

        if (behaviour == nullptr || !behaviour->IsEnabled())
        {
            return false;
        }

        if (state == ActiveRagdollState::Animated)
        {
            if (!WakePhysics())
            {
                return false;
            }

            if (behaviour->NeedsFreePose())
            {
                EnterFallen();
            }
        }

        const bool isStarted = behaviours.Trigger(ragdoll, behaviourName);

        ApplyBehaviourOutput();

        return isStarted;
    }
};

ActiveRagdollSystem::ActiveRagdollSystem() = default;

ActiveRagdollSystem::~ActiveRagdollSystem()
{
    DestroyAllInstances();
}

bool ActiveRagdollSystem::ShouldProcessScene(Scene* scene) const
{
    return !(scene->GetSceneFlags() & (SceneFlags::UI | SceneFlags::DETACHED));
}

void ActiveRagdollSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);
}

void ActiveRagdollSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    DestroyInstance(entity);
    m_failedEntities.Erase(entity);
}

void ActiveRagdollSystem::OnRemovedFromWorld(World* world)
{
    DestroyAllInstances();

    SystemBase::OnRemovedFromWorld(world);
}

ActiveRagdollInstance* ActiveRagdollSystem::FindInstance(Entity* entity) const
{
    const auto it = m_instances.Find(entity);

    if (it == m_instances.End())
    {
        return nullptr;
    }

    return it->second.Get();
}

void ActiveRagdollSystem::DestroyInstance(Entity* entity)
{
    const auto it = m_instances.Find(entity);

    if (it == m_instances.End())
    {
        return;
    }

    if (MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>(); meshComponent && !entity->TryGetComponent<AnimationComponent>())
    {
        ReleaseSkeletonInstance(*entity, *meshComponent);
    }

    m_instances.Erase(it);
}

void ActiveRagdollSystem::DestroyAllInstances()
{
    m_instances.Clear();
    m_failedEntities.Clear();

    if (m_contacts != nullptr)
    {
        World* world = GetWorld();

        if (world != nullptr && world->GetPhysicsWorld().IsValid())
        {
            PhysicsWorld& physicsWorld = static_cast<PhysicsWorld&>(*world->GetPhysicsWorld());

            if (physicsWorld.GetAdapter().GetPhysicsSystem() == m_contactsPhysicsSystem)
            {
                physicsWorld.GetAdapter().RemoveContactListener(m_contacts);
            }
        }

        delete m_contacts;
        m_contacts = nullptr;
        m_contactsPhysicsSystem = nullptr;
    }
}

bool ActiveRagdollSystem::EnsureContacts()
{
    World* world = GetWorld();

    if (world == nullptr || !world->GetPhysicsWorld().IsValid())
    {
        return false;
    }

    PhysicsWorld& physicsWorld = static_cast<PhysicsWorld&>(*world->GetPhysicsWorld());
    JPH::PhysicsSystem* physicsSystem = physicsWorld.GetAdapter().GetPhysicsSystem();

    if (physicsSystem == nullptr)
    {
        return false;
    }

    // a new physics system means the old bodies went with the old one
    if (m_contacts != nullptr && m_contactsPhysicsSystem != physicsSystem)
    {
        DestroyAllInstances();
    }

    if (m_contacts == nullptr)
    {
        m_contacts = new RagdollContacts();
        m_contactsPhysicsSystem = physicsSystem;

        physicsWorld.GetAdapter().AddContactListener(m_contacts);
    }

    return true;
}

ActiveRagdollInstance* ActiveRagdollSystem::CreateInstance(Entity* entity, ActiveRagdollComponent& component, MeshComponent& meshComponent)
{
    DestroyInstance(entity);

    Handle<RagdollDefinition> definition = component.definition;

    if (!definition.IsValid())
    {
        definition = RagdollDefinition::GetDefaultHumanoid();
    }

    if (!definition.IsValid())
    {
        HYP_LOG_ONCE(Physics, Warning, "Active ragdoll: no ragdoll definition, and the plugin's HumanoidRagdoll couldn't be loaded");

        return nullptr;
    }

    const Skeleton& skeleton = *meshComponent.skeleton;
    const SkeletonBindData& bindData = skeleton.GetBindData();

    UniquePtr<ActiveRagdollInstance> instance = MakeUnique<ActiveRagdollInstance>();
    instance->entity = entity;
    instance->definition = definition;
    instance->source = meshComponent.skeleton;
    instance->sourceRevision = skeleton.GetRevision();

    if (!definition->Resolve(skeleton, instance->bodies))
    {
        return nullptr;
    }

    instance->animatedPose.Resize(bindData.NumSlots());
    instance->animatedModels.Resize(bindData.NumSlots());
    instance->animatedSkinning.Resize(bindData.NumSlots());

    const Span<const Transform> restPose = PoseOps::GetRestPose(bindData);

    for (uint32 slot = 0; slot < bindData.NumSlots(); slot++)
    {
        instance->animatedPose[slot] = restPose[slot];
    }

    if (AnimationSystem* animationSystem = GetWorld()->GetSystem<AnimationSystem>())
    {
        const Span<const Transform> renderPose = animationSystem->GetRenderPose(entity);

        if (renderPose.Size() == bindData.NumSlots())
        {
            for (uint32 slot = 0; slot < bindData.NumSlots(); slot++)
            {
                instance->animatedPose[slot] = renderPose[slot];
            }
        }
    }

    ComputeSkeletonPoseMatrices(bindData, instance->animatedPose.ToSpan(), instance->animatedModels.ToSpan(), instance->animatedSkinning.ToSpan());

    if (!instance->ragdoll.Create(*GetWorld()->GetPhysicsWorld(), *m_contacts, bindData, instance->bodies.ToSpan(), definition->GetTotalMass(), instance->animatedModels.ToSpan(), entity->GetWorldMatrix()))
    {
        HYP_LOG(Physics, Warning, "Active ragdoll: couldn't build a ragdoll for '{}'", entity->GetName());

        return nullptr;
    }

    instance->behaviours.ApplySettings(component.behaviours);

    if (instance->ragdoll.GetRig().isValid)
    {
        instance->behaviours.Init(instance->ragdoll);
    }

    // kinematic until something hits it, so the animated feet don't drag on the floor
    instance->ragdoll.SetAllBodyModes(RagdollBodyMode::Kinematic);
    instance->ragdoll.SetRootAssist(component.rootAssist);
    instance->ragdoll.Drive(instance->animatedModels.ToSpan(), entity->GetWorldMatrix(), 0.0f, true);

    component.state = ActiveRagdollState::Animated;
    component.isDying = false;

    ActiveRagdollInstance* instancePtr = instance.Get();
    m_instances.Set(entity, std::move(instance));

    return instancePtr;
}

void ActiveRagdollSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    HYP_SCOPE;

    World* world = GetWorld();

    if (!world->GetGameState().IsSimulating() || !EnsureContacts())
    {
        if (m_instances.Any())
        {
            DestroyAllInstances();
        }

        return;
    }

    AnimationSystem* animationSystem = world->GetSystem<AnimationSystem>();
    DebugDrawCommandList* debugDraw = nullptr;

    for (Scene* scene : scenes)
    {
        if (!ShouldProcessScene(scene))
        {
            continue;
        }

        for (auto [entity, component, meshComponent, transformComponent] : scene->GetEntityManager()->GetEntitySet<ActiveRagdollComponent, MeshComponent, TransformComponent>().GetScopedView(GetComponentInfos()))
        {
            if (!meshComponent.skeleton.IsValid())
            {
                continue;
            }

            ActiveRagdollInstance* instance = FindInstance(entity);

            const bool isStale = instance != nullptr
                && (instance->source.GetUnsafe() != meshComponent.skeleton.Get() || instance->sourceRevision != meshComponent.skeleton->GetRevision());

            if (instance == nullptr || isStale)
            {
                const auto failedIt = m_failedEntities.Find(entity);

                if (failedIt != m_failedEntities.End() && failedIt->second == meshComponent.skeleton->GetRevision())
                {
                    continue;
                }

                instance = CreateInstance(entity, component, meshComponent);

                if (instance == nullptr)
                {
                    m_failedEntities.Set(entity, meshComponent.skeleton->GetRevision());

                    continue;
                }

                m_failedEntities.Erase(entity);
            }

            const SkeletonBindData& bindData = meshComponent.skeleton->GetBindData();

            Span<const Transform> renderPose;

            if (animationSystem != nullptr)
            {
                renderPose = animationSystem->GetRenderPose(entity);
            }

            const bool isAnimated = renderPose.Size() == bindData.NumSlots();

            if (isAnimated)
            {
                for (uint32 slot = 0; slot < bindData.NumSlots(); slot++)
                {
                    instance->animatedPose[slot] = renderPose[slot];
                }
            }

            ComputeSkeletonPoseMatrices(bindData, instance->animatedPose.ToSpan(), instance->animatedModels.ToSpan(), instance->animatedSkinning.ToSpan());

            instance->strength = component.strength;
            instance->rootAssist = component.rootAssist;
            instance->balance = component.balance;
            instance->balanceAssist = component.balanceAssist;
            instance->shove = component.shove;
            instance->debugDraw = component.debugDraw;
            instance->behaviours.ApplySettings(component.behaviours);

            UpdateInstance(*instance, meshComponent, isAnimated, delta);

            component.state = instance->state;
            component.isDying = instance->isDying;

            if (instance->debugDraw)
            {
                if (debugDraw == nullptr)
                {
                    debugDraw = &DebugDrawer::GetInstance().CreateCommandList();
                }

                instance->ragdoll.DebugDraw(*debugDraw);
                instance->behaviours.DebugDraw(*debugDraw);
            }
        }
    }

    m_contacts->DrainImpacts(m_impacts);

    for (const RagdollImpact& impact : m_impacts)
    {
        for (const auto& it : m_instances)
        {
            if (it.second->ragdoll.IsCreated() && it.second->ragdoll.GetSerial() == impact.serial)
            {
                it.second->OnImpact(impact);
                break;
            }
        }
    }

    // contacts gather again over the next update, as Ragdoll::OnStep expects
    m_contacts->Clear();
}

void ActiveRagdollSystem::UpdateInstance(ActiveRagdollInstance& instance, MeshComponent& meshComponent, bool isAnimated, float delta)
{
    Ragdoll& ragdoll = instance.ragdoll;

    if (instance.state == ActiveRagdollState::Limp)
    {
        instance.FollowPelvis();
    }
    else
    {
        instance.UpdateBehaviours(delta);

        if (instance.state == ActiveRagdollState::Animated)
        {
            instance.UpdateBalance(delta);
            instance.UpdatePhysicsBlend(delta);
        }

        if (instance.state != ActiveRagdollState::Limp)
        {
            instance.ApplyStrengths();
        }

        if (instance.state == ActiveRagdollState::Animated)
        {
            ragdoll.Drive(instance.animatedModels.ToSpan(), instance.GetEntityWorldMatrix(), delta, instance.isTeleported);
            instance.isTeleported = false;
        }
        else
        {
            instance.FollowPelvis();
        }
    }

    const Handle<Skeleton>& skeletonInstance = AcquireSkeletonInstance(*instance.entity, meshComponent);

    if (!skeletonInstance.IsValid())
    {
        return;
    }

    const SkeletonBindData& bindData = meshComponent.skeleton->GetBindData();

    if (ragdoll.WritePose(instance.animatedPose.ToSpan(), bindData, instance.GetEntityWorldMatrix(), instance.pose))
    {
        skeletonInstance->SetPose(instance.pose.ToSpan());
        instance.isPoseWritten = true;

        return;
    }

    if (instance.isPoseWritten && !isAnimated)
    {
        skeletonInstance->SetPose(instance.animatedPose.ToSpan());
    }

    instance.isPoseWritten = false;
}

void ActiveRagdollSystem::Hit(Entity* entity, uint32 body, const Vec3f& point, const Vec3f& impulse, float damage)
{
    ActiveRagdollInstance* instance = FindInstance(entity);

    if (instance == nullptr || !instance->ragdoll.IsCreated())
    {
        return;
    }

    if (body >= instance->ragdoll.GetBodyCount())
    {
        body = instance->FindNearestBody(ToJolt(point));
    }

    instance->Hit(body, ToJolt(point), ToJolt(impulse), damage);
}

void ActiveRagdollSystem::Push(Entity* entity, const Vec3f& impulse)
{
    if (ActiveRagdollInstance* instance = FindInstance(entity))
    {
        instance->Push(ToJolt(impulse));
    }
}

void ActiveRagdollSystem::Kill(Entity* entity, uint32 body)
{
    ActiveRagdollInstance* instance = FindInstance(entity);

    if (instance == nullptr)
    {
        return;
    }

    int32 bodyIndex = -1;

    if (body < instance->ragdoll.GetBodyCount())
    {
        bodyIndex = int32(body);
    }

    instance->Kill(bodyIndex);
}

void ActiveRagdollSystem::GoLimp(Entity* entity)
{
    if (ActiveRagdollInstance* instance = FindInstance(entity))
    {
        instance->EnterLimp();
    }
}

bool ActiveRagdollSystem::TriggerBehaviour(Entity* entity, Name behaviourName)
{
    ActiveRagdollInstance* instance = FindInstance(entity);

    return instance != nullptr && instance->TriggerBehaviour(behaviourName);
}

bool ActiveRagdollSystem::StopBehaviour(Entity* entity, Name behaviourName)
{
    ActiveRagdollInstance* instance = FindInstance(entity);

    return instance != nullptr && instance->behaviours.Stop(behaviourName);
}

uint32 ActiveRagdollSystem::GetBodyCount(Entity* entity) const
{
    const ActiveRagdollInstance* instance = FindInstance(entity);

    if (instance == nullptr)
    {
        return 0;
    }

    return instance->ragdoll.GetBodyCount();
}

uint32 ActiveRagdollSystem::FindBody(Entity* entity, Name boneName) const
{
    const ActiveRagdollInstance* instance = FindInstance(entity);

    if (instance == nullptr)
    {
        return ~0u;
    }

    const Handle<Skeleton> skeleton = instance->source.Lock();

    if (!skeleton.IsValid())
    {
        return ~0u;
    }

    const uint32 slot = skeleton->GetBindData().FindSlot(boneName);

    for (uint32 i = 0; i < instance->ragdoll.GetBodyCount(); i++)
    {
        if (instance->ragdoll.GetBodySlot(i) == slot)
        {
            return i;
        }
    }

    return ~0u;
}

uint32 ActiveRagdollSystem::GetChestBody(Entity* entity) const
{
    const ActiveRagdollInstance* instance = FindInstance(entity);

    if (instance == nullptr || !instance->ragdoll.IsCreated())
    {
        return ~0u;
    }

    return instance->ragdoll.GetRig().chest;
}

bool ActiveRagdollSystem::GetBodyPosition(Entity* entity, uint32 body, Vec3f& outPosition) const
{
    const ActiveRagdollInstance* instance = FindInstance(entity);

    if (instance == nullptr || !instance->ragdoll.IsCreated() || body >= instance->ragdoll.GetBodyCount())
    {
        return false;
    }

    outPosition = FromJolt(instance->ragdoll.GetBodyPosition(body));

    return true;
}

bool ActiveRagdollSystem::CastRay(const Vec3f& origin, const Vec3f& direction, float maxDistance, ActiveRagdollRayHit& outHit) const
{
    if (m_contactsPhysicsSystem == nullptr || maxDistance <= 0.0f || direction.LengthSquared() <= 1e-12f)
    {
        return false;
    }

    const JPH::RRayCast ray { JPH::RVec3(ToJolt(origin)), ToJolt(direction.Normalized() * maxDistance) };

    const JPH::SpecifiedObjectLayerFilter objectLayerFilter(JoltLayers::RAGDOLL);

    JPH::ClosestHitCollisionCollector<JPH::CastRayCollector> collector;
    m_contactsPhysicsSystem->GetNarrowPhaseQuery().CastRay(ray, JPH::RayCastSettings(), collector, JPH::BroadPhaseLayerFilter(), objectLayerFilter);

    if (!collector.HadHit())
    {
        return false;
    }

    uint64 userData = 0;
    JPH::Vec3 normal = -ToJolt(direction.Normalized());
    const JPH::RVec3 hitPosition = ray.GetPointOnRay(collector.mHit.mFraction);

    {
        JPH::BodyLockRead lock(m_contactsPhysicsSystem->GetBodyLockInterface(), collector.mHit.mBodyID);

        if (!lock.Succeeded())
        {
            return false;
        }

        userData = lock.GetBody().GetUserData();
        normal = lock.GetBody().GetWorldSpaceSurfaceNormal(collector.mHit.mSubShapeID2, hitPosition);
    }

    if (!RagdollUserData::Matches(userData))
    {
        return false;
    }

    const uint32 serial = RagdollUserData::GetSerial(userData);

    for (const auto& it : m_instances)
    {
        const ActiveRagdollInstance& instance = *it.second;

        if (!instance.ragdoll.IsCreated() || instance.ragdoll.GetSerial() != serial)
        {
            continue;
        }

        const int32 body = instance.ragdoll.FindBodyIndex(collector.mHit.mBodyID);

        if (body < 0)
        {
            return false;
        }

        outHit.entity = instance.entity;
        outHit.body = uint32(body);
        outHit.position = FromJolt(JPH::Vec3(hitPosition));
        outHit.normal = FromJolt(normal);
        outHit.distance = collector.mHit.mFraction * maxDistance;

        if (const Handle<Skeleton> skeleton = instance.source.Lock())
        {
            outHit.boneName = skeleton->GetBindData().boneNames[instance.ragdoll.GetBodySlot(uint32(body))];
        }

        return true;
    }

    return false;
}

bool ActiveRagdollSystem::Shoot(const Vec3f& origin, const Vec3f& direction, float maxDistance, float impulse, float damage)
{
    ActiveRagdollRayHit hit;

    if (!CastRay(origin, direction, maxDistance, hit))
    {
        return false;
    }

    Hit(hit.entity, hit.body, hit.position, direction.Normalized() * impulse, damage);

    return true;
}

} // namespace Hyperion
