/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <scene/Scene.hpp>
#include <scene/EntityManager.hpp>
#include <scene/Entity.hpp>
#include <scene/EntityTag.hpp>
#include <scene/InstancedMeshProxy.hpp>

#include <scene/systems/MeshSystem.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/AssetObject.hpp>

#include <rendering/InstancedMeshData.hpp>

#include <MeshSystem.generated.inl>

namespace Hyperion {

void DestroyInstancedMeshData(Entity& entity, MeshComponent& meshComponent, bool removeFromPackage = false);

void UpdateInstancedMeshData(Entity& entity, MeshComponent& meshComponent)
{
    Array<InstancedMeshProxy*, SceneAllocator> instancedMeshProxies;

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
    }
    
    const Handle<InstancedMeshData>& imd = DynamicCast<InstancedMeshData>(meshComponent.instanceData.Resolve());

    if (!imd.IsValid())
    {
        HYP_LOG(Scene, Error, "Failed to load instanced mesh data for Entity {}", entity.GetName());

        DestroyInstancedMeshData(entity, meshComponent, /* removeFromPackage */ false);

        return;
    }

    auto scope = imd->GetWriteScope();

    Array<Mat4f, SceneAllocator> transforms;
    transforms.Resize(instancedMeshProxies.Size());

    Array<Mat4f, SceneAllocator> previousTransforms;
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
                meshComponent.instanceData = AssetReference(meshComponent.instanceData.GetAssetPath());
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

#if HYP_EDITOR
    m_cachedStates[entity] = CachedInstancedMeshDataState {
        meshComponent.enableAutoInstancing
    };
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

#if HYP_EDITOR
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

        HashSet<Entity*, SceneAllocator> updatedEntities;

#if HYP_EDITOR
        for (auto [entity, meshComponent] : scene->GetEntityManager()->GetEntitySet<MeshComponent>().GetScopedView(GetComponentInfos()))
        {
            if (updatedEntities.Find(entity) != updatedEntities.End())
                continue;

            auto cachedIt = m_cachedStates.Find(entity);
            if (cachedIt != m_cachedStates.End())
            {
                if (cachedIt->second.enableAutoInstancing != meshComponent.enableAutoInstancing)
                {
                    UpdateInstancedMeshData(*entity, meshComponent);

                    cachedIt->second.enableAutoInstancing = meshComponent.enableAutoInstancing;

                    updatedEntities.Add(entity);
                }
            }
        }
#endif // HYP_EDITOR

        for (auto [entity, meshComponent, _] : scene->GetEntityManager()->GetEntitySet<MeshComponent, TagComponent<EntityTag::UpdateInstancedMeshData>>().GetScopedView(GetComponentInfos()))
        {
            if (updatedEntities.Find(entity) != updatedEntities.End())
                continue;

            UpdateInstancedMeshData(*entity, meshComponent);

            updatedEntities.Add(entity);
        }

        if (updatedEntities.Any())
        {
            AfterProcess([updatedEntities = std::move(updatedEntities)]()
                {
                    for (Entity* entity : updatedEntities)
                    {
                        entity->SetNeedsRenderProxyUpdate();
                    }
                });
        }
    }
}

} // namespace Hyperion
