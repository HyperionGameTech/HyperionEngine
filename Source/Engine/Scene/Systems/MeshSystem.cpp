/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Scene.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Entity.hpp>
#include <Scene/EntityTag.hpp>
#include <Scene/InstancedMeshProxy.hpp>

#include <Scene/Systems/MeshSystem.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetObject.hpp>

#include <Rendering/InstancedMeshData.hpp>

#include <Core/Memory/Allocator/ThreadAllocator.hpp>

#include <MeshSystem.generated.inl>

namespace Hyperion {

void DestroyInstancedMeshData(Entity& entity, MeshComponent& meshComponent, bool removeFromPackage = false);

void UpdateInstancedMeshData(Entity& entity, MeshComponent& meshComponent)
{
    FatArray<InstancedMeshProxy*, InlineAllocator<4, ThreadAllocator>> instancedMeshProxies;

    for (const Handle<Node>& childNode : entity.GetChildren())
    {
        if (childNode->IsA(InstancedMeshProxy::StaticClass()))
        {
            instancedMeshProxies.PushBack(static_cast<InstancedMeshProxy*>(childNode.Get()));
        }
    }

    meshComponent.numInstances = uint32(instancedMeshProxies.Size());

    if (!meshComponent.enableAutoInstancing && instancedMeshProxies.Empty())
    {
        if (meshComponent.instanceData.IsValid())
        {
            DestroyInstancedMeshData(entity, meshComponent, /* removeFromPackage */ true);
        }

        return;
    }

    if (!meshComponent.instanceData.IsValid())
    {
        Handle<InstancedMeshData> imd = MakeHandle<InstancedMeshData>(NAME_FMT("IMD_{}", entity.GetName()));

        GetCurrentAssetRegistry()->PutAssetUnique(imd);

        meshComponent.instanceData = AssetReference(imd);

        Assert(imd->IsRegistered());
    }

    const Handle<InstancedMeshData>& imd = DynamicCast<InstancedMeshData>(meshComponent.instanceData.Resolve());

    if (!imd.IsValid())
    {
        HYP_LOG(Scene, Error, "Failed to load instanced mesh data for Entity {}", entity.GetName());

        DestroyInstancedMeshData(entity, meshComponent, /* removeFromPackage */ false);

        return;
    }

    auto writeScope = imd->GetWriteScope();

    Array<Mat4f, ThreadAllocator> transforms;
    transforms.Resize(instancedMeshProxies.Size());

    Array<Mat4f, ThreadAllocator> previousTransforms;
    previousTransforms.Resize(instancedMeshProxies.Size());

    for (size_t i = 0; i < instancedMeshProxies.Size(); i++)
    {
        InstancedMeshProxy* imp = instancedMeshProxies[i];

        transforms[i] = imp->GetLocalTransform().GetMatrix();
        previousTransforms[i] = imp->prevTransformMatrix;

        imp->prevTransformMatrix = transforms[i];
    }

    // Update transforms etc. based on the InstancedMeshProxy child objects
    imd->SetBufferData(0, transforms.Data(), transforms.Size());
    imd->SetBufferData(1, previousTransforms.Data(), previousTransforms.Size());
}

void DestroyInstancedMeshData(Entity& entity, MeshComponent& meshComponent, bool removeFromPackage)
{
    if (meshComponent.instanceData.IsValid())
    {
        if (!removeFromPackage)
        {
            if (meshComponent.instanceData.IsLoaded())
            {
                // Unload and set to path
                meshComponent.instanceData = AssetReference();
                meshComponent.instanceData.SetAssetPath(meshComponent.instanceData.GetAssetPath());
            }

            return;
        }

        Handle<AssetObject> obj = meshComponent.instanceData.Resolve();
        meshComponent.instanceData = AssetReference();

        if (obj.IsValid())
        {
            GetCurrentAssetRegistry()->RemoveAsset(obj);
        }
    }
}

void MeshSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    if (!ShouldProcessScene(entity->GetScene()))
    {
        return;
    }

    MeshComponent& meshComponent = entity->GetComponent<MeshComponent>();

    UpdateInstancedMeshData(*entity, meshComponent);

    entity->AddTag<EntityTag::UpdateRenderProxy>();
    entity->RemoveTag<EntityTag::UpdateInstancedMeshData>();

#ifdef HYP_EDITOR
    m_cachedStates[entity] = CachedInstancedMeshDataState { meshComponent.enableAutoInstancing };
#endif // HYP_EDITOR
}

void MeshSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    if (!ShouldProcessScene(entity->GetScene()))
    {
        return;
    }

    DestroyInstancedMeshData(*entity, entity->GetComponent<MeshComponent>(), /* removeFromPackage */ false);

    entity->AddTag<EntityTag::UpdateRenderProxy>();
    entity->RemoveTag<EntityTag::UpdateInstancedMeshData>();

#ifdef HYP_EDITOR
    m_cachedStates.Erase(entity);
#endif // HYP_EDITOR
}

bool MeshSystem::ShouldProcessScene(Scene* scene) const
{
    return !(scene->GetSceneFlags() & SceneFlags::UI);
}

void MeshSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    // Update instanced mesh data for entities that need it.

    for (Scene* scene : scenes)
    {
        if (!ShouldProcessScene(scene))
        {
            continue;
        }

        m_updatedEntities.Resize(0);

#ifdef HYP_EDITOR
        for (auto [entity, meshComponent] : scene->GetEntityManager()->GetEntitySet<MeshComponent>().GetScopedView(GetComponentInfos()))
        {
            if (std::find(m_updatedEntities.Begin(), m_updatedEntities.End(), entity) != m_updatedEntities.End())
            {
                continue;
            }

            auto cachedIt = m_cachedStates.Find(entity);
            if (cachedIt != m_cachedStates.End())
            {
                if (cachedIt->second.enableAutoInstancing != meshComponent.enableAutoInstancing)
                {
                    UpdateInstancedMeshData(*entity, meshComponent);

                    cachedIt->second.enableAutoInstancing = meshComponent.enableAutoInstancing;

                    m_updatedEntities.PushBack(entity);
                }
            }
        }
#endif // HYP_EDITOR

        for (auto [entity, meshComponent, _] : scene->GetEntityManager()->GetEntitySet<MeshComponent, TagComponent<EntityTag::UpdateInstancedMeshData>>().GetScopedView(GetComponentInfos()))
        {
            if (std::find(m_updatedEntities.Begin(), m_updatedEntities.End(), entity) != m_updatedEntities.End())
            {
                continue;
            }

            UpdateInstancedMeshData(*entity, meshComponent);

            m_updatedEntities.PushBack(entity);
        }

        if (m_updatedEntities.Any())
        {
            AfterProcess(
                [updatedEntities = m_updatedEntities]()
                {
                    for (Entity* entity : updatedEntities)
                    {
                        //entity->RemoveTag<EntityTag::UpdateInstancedMeshData>();
                        entity->SetNeedsRenderProxyUpdate();
                    }
                });
        }
    }
}

} // namespace Hyperion
