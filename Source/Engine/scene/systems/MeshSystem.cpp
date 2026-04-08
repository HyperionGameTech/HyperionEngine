/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <ScenePch.hpp>

#include <scene/Scene.hpp>
#include <scene/EntityManager.hpp>
#include <scene/Entity.hpp>
#include <scene/EntityTag.hpp>

#include <scene/systems/MeshSystem.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/AssetObject.hpp>

#include <rendering/InstancedMeshData.hpp>

#include <MeshSystem.generated.inl>

namespace Hyperion {

void DestroyInstancedMeshData(Entity& entity, MeshComponent& meshComponent, bool removeFromPackage = false);

void InitInstancedMeshData(Entity& entity, MeshComponent& meshComponent)
{
    if (!meshComponent.enableAutoInstancing && meshComponent.numInstances <= 1)
    {
        if (meshComponent.instanceData.IsValid())
        {
            DestroyInstancedMeshData(entity, meshComponent, /* removeFromPackage */ true);
        }
        
        return;
    }
    
    if (!meshComponent.instanceData.IsValid())
    {
        Handle<InstancedMeshData> instancedMesh = MakeHandle<InstancedMeshData>(NAME_FMT("IMD_{}", entity.GetName()));

        Result registerResult = instancedMesh->Register("$Memory/Objects/Types/InstancedMeshData", AddAssetConflictMode::GenerateNewName);

        if (registerResult.HasError())
        {
            HYP_LOG(Scene, Error, "Failed to register InstancedMeshData: {}", registerResult.GetError().GetMessage());
        }

        meshComponent.instanceData = AssetReference(instancedMesh);
    }
    
    const Handle<InstancedMeshData>& instancedMesh = ObjCast<InstancedMeshData>(meshComponent.instanceData.Resolve());

    if (!instancedMesh.IsValid())
    {
        HYP_LOG(Scene, Error, "Failed to load instanced mesh data for Entity {}", entity.GetName());

        DestroyInstancedMeshData(entity, meshComponent, /* removeFromPackage */ false);

        return;
    }

    entity.RemoveTag<EntityTag::UpdateInstancedMeshData>();
}

void DestroyInstancedMeshData(Entity& entity, MeshComponent& meshComponent, bool removeFromPackage)
{
    entity.RemoveTag<EntityTag::UpdateInstancedMeshData>();

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
            Handle<AssetPackage> package = obj->GetPackage();
            if (package.IsValid())
            {
                Result removeAssetResult = package->RemoveAssetObject(obj);

                if (removeAssetResult.HasError())
                {
                    HYP_LOG(Scene, Error, "Failed to remove InstancedMeshData asset from package. Error was: {}",
                        removeAssetResult.GetError().GetMessage());

                    return;
                }
            }
        }
    }
}

void MeshSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    MeshComponent& meshComponent = entity->GetComponent<MeshComponent>();

    InitInstancedMeshData(*entity, meshComponent);

#if HYP_EDITOR
    m_cachedStates[entity] = CachedInstancedMeshDataState {
        meshComponent.numInstances,
        meshComponent.enableAutoInstancing
    };
#endif // HYP_EDITOR
}

void MeshSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);
    
    DestroyInstancedMeshData(*entity, entity->GetComponent<MeshComponent>(), /* removeFromPackage */ false);

#if HYP_EDITOR
    m_cachedStates.Erase(entity);
#endif // HYP_EDITOR
}

void MeshSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    // Update instanced mesh data for entities that need it.

    for (Scene* scene : scenes)
    {
        HashSet<WeakHandle<Entity>, SceneAllocator> updatedEntities;

#if HYP_EDITOR
        for (auto [entity, meshComponent] : scene->GetEntityManager()->GetEntitySet<MeshComponent>().GetScopedView(GetComponentInfos()))
        {
            auto cachedIt = m_cachedStates.Find(entity);
            if (cachedIt != m_cachedStates.End())
            {
                if ((cachedIt->second.numInstances > 1) != (meshComponent.numInstances > 1)
                    || cachedIt->second.enableAutoInstancing != meshComponent.enableAutoInstancing)
                {
                    InitInstancedMeshData(*entity, meshComponent);

                    cachedIt->second.numInstances = meshComponent.numInstances;
                    cachedIt->second.enableAutoInstancing = meshComponent.enableAutoInstancing;

                    updatedEntities.Add(MakeWeakRef(entity));
                }
            }
        }
#endif // HYP_EDITOR

        for (auto [entity, meshComponent, _] : scene->GetEntityManager()->GetEntitySet<MeshComponent, TagComponent<EntityTag::UpdateInstancedMeshData>>().GetScopedView(GetComponentInfos()))
        {
            InitInstancedMeshData(*entity, meshComponent);

            updatedEntities.Add(MakeWeakRef(entity));
        }

        if (updatedEntities.Any())
        {
            AfterProcess([updatedEntities = std::move(updatedEntities)]()
                {
                    for (const WeakHandle<Entity>& entityWeak : updatedEntities)
                    {
                        if (Handle<Entity> entity = entityWeak.Lock(); entity.IsValid())
                        {
                            entity->RemoveTag<EntityTag::UpdateInstancedMeshData>();
                        }
                    }
                });
        }
    }
}

} // namespace Hyperion
