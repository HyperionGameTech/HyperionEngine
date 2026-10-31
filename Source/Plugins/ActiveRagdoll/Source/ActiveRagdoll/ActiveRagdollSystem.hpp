/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Scene/System.hpp>

#include <ActiveRagdoll/ActiveRagdollComponent.hpp>
#include <ActiveRagdoll/RagdollContacts.hpp>

#include <Core/Containers/Map.hpp>
#include <Core/Memory/UniquePtr.hpp>

namespace JPH {
class PhysicsSystem;
} // namespace JPH

namespace Hyperion {

class Entity;
class RagdollContacts;
struct ActiveRagdollInstance;
struct MeshComponent;
struct TransformComponent;

struct ActiveRagdollRayHit
{
    Entity* entity = nullptr;
    uint32 body = ~0u;
    Name boneName;
    Vec3f position;
    Vec3f normal;
    float distance = 0.0f;
};

HYP_CLASS(NoScriptBindings, Serialize = false)
class ACTIVERAGDOLL_API ActiveRagdollSystem final : public SystemBase
{
    HYP_OBJECT_BODY(ActiveRagdollSystem);

public:
    ActiveRagdollSystem();
    ~ActiveRagdollSystem() override;

    bool ShouldProcessScene(Scene* scene) const override;

    bool RequiresSimThread() const override
    {
        return true;
    }

    bool AllowParallelExecution() const override
    {
        return false;
    }

    void OnEntityAdded(Entity* entity) override;
    void OnEntityRemoved(Entity* entity) override;
    void OnRemovedFromWorld(World* world) override;

    void Process(float delta, Span<Handle<Scene>> scenes) override;

    // a shot or blow on a body of the ragdoll, or the one nearest `point` when `body` is ~0u
    void Hit(Entity* entity, uint32 body, const Vec3f& point, const Vec3f& impulse, float damage);

    // a shove without damage, at the chest
    void Push(Entity* entity, const Vec3f& impulse);

    // fades the body's strength out with the dying behaviour, then goes limp
    void Kill(Entity* entity, uint32 body = ~0u);

    void GoLimp(Entity* entity);

    bool TriggerBehaviour(Entity* entity, Name behaviourName);
    bool StopBehaviour(Entity* entity, Name behaviourName);

    // the nearest ragdoll body along the ray
    bool CastRay(const Vec3f& origin, const Vec3f& direction, float maxDistance, ActiveRagdollRayHit& outHit) const;

    // the ray's hit, if any, takes `impulse` along the ray and `damage`
    bool Shoot(const Vec3f& origin, const Vec3f& direction, float maxDistance, float impulse, float damage);

    uint32 GetBodyCount(Entity* entity) const;
    uint32 FindBody(Entity* entity, Name boneName) const;
    uint32 GetChestBody(Entity* entity) const;
    bool GetBodyPosition(Entity* entity, uint32 body, Vec3f& outPosition) const;

private:
    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<ActiveRagdollComponent, ComponentAccess::READ_WRITE> {},
            ComponentDescriptor<MeshComponent, ComponentAccess::READ_WRITE> {},
            ComponentDescriptor<TransformComponent, ComponentAccess::READ_WRITE> {}
        };
    }

    ActiveRagdollInstance* FindInstance(Entity* entity) const;
    ActiveRagdollInstance* FindInstanceBySerial(uint32 serial) const;
    ActiveRagdollInstance* CreateInstance(Entity* entity, ActiveRagdollComponent& component, MeshComponent& meshComponent);
    void DestroyInstance(Entity* entity);
    void DestroyAllInstances();

    bool EnsureContacts();

    void UpdateInstance(ActiveRagdollInstance& instance, MeshComponent& meshComponent, bool isAnimated, float delta);

    Map<Entity*, UniquePtr<ActiveRagdollInstance>> m_instances;

    // entities whose ragdoll couldn't be built, by the skeleton revision they had then
    Map<Entity*, uint32> m_failedEntities;

    RagdollContacts* m_contacts = nullptr;
    Array<RagdollImpact> m_impacts;
    Array<RagdollBump> m_bumps;
    JPH::PhysicsSystem* m_contactsPhysicsSystem = nullptr;
};

} // namespace Hyperion
