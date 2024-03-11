#include <scene/ecs/EntityManager.hpp>

// Components
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/LightComponent.hpp>
#include <scene/ecs/components/ShadowMapComponent.hpp>

#include <scene/animation/Skeleton.hpp>

#include <dotnet/runtime/ManagedHandle.hpp>
#include <dotnet/runtime/scene/ManagedSceneTypes.hpp>
#include <dotnet/runtime/math/ManagedMathTypes.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
    ManagedEntity EntityManager_AddEntity(EntityManager *manager)
    {
        return manager->AddEntity();
    }

    void EntityManager_RemoveEntity(EntityManager *manager, ManagedEntity entity)
    {
        manager->RemoveEntity(entity);
    }

    bool EntityManager_HasEntity(EntityManager *manager, ManagedEntity entity)
    {
        return manager->HasEntity(entity);
    }

    bool EntityManager_HasComponent(EntityManager *manager, uint32 component_type_id, ManagedEntity entity)
    {
        return manager->HasComponent(TypeID { component_type_id }, entity);
    }

    void *EntityManager_GetComponent(EntityManager *manager, uint32 component_type_id, ManagedEntity entity)
    {
        return manager->TryGetComponent(TypeID { component_type_id }, entity);
    }

    // Components
    // TransformComponent
    uint32 TransformComponent_GetNativeTypeID()
    {
        return TypeID::ForType<TransformComponent>().Value();
    }

    ComponentID TransformComponent_AddComponent(EntityManager *manager, ManagedEntity entity, TransformComponent *component)
    {
        return manager->AddComponent(entity, std::move(*component));
    }

    // MeshComponent
    struct ManagedMeshComponent
    {
        ManagedHandle   mesh_handle;
        ManagedHandle   material_handle;
        ManagedMatrix4  previous_model_matrix;
        uint32          mesh_component_flags;
    };

    static_assert(std::is_trivial_v<ManagedMeshComponent> && std::is_standard_layout_v<ManagedMeshComponent>, "ManagedMeshComponent should be a POD type");
    static_assert(sizeof(ManagedMeshComponent) == 76, "ManagedMeshComponent should equal 84 bytes to match C# struct size");

    uint32 MeshComponent_GetNativeTypeID()
    {
        return TypeID::ForType<MeshComponent>().Value();
    }

    ComponentID MeshComponent_AddComponent(EntityManager *manager, ManagedEntity entity, ManagedMeshComponent *component)
    {
        Handle<Mesh> mesh = CreateHandleFromManagedHandle<Mesh>(component->mesh_handle);
        Handle<Material> material = CreateHandleFromManagedHandle<Material>(component->material_handle);

        return manager->AddComponent(entity, MeshComponent {
            std::move(mesh),
            std::move(material),
            Handle<Skeleton> { },
            component->previous_model_matrix,
            component->mesh_component_flags
        });
    }

    // BoundingBoxComponent
    uint32 BoundingBoxComponent_GetNativeTypeID()
    {
        return TypeID::ForType<BoundingBoxComponent>().Value();
    }

    ComponentID BoundingBoxComponent_AddComponent(EntityManager *manager, ManagedEntity entity, BoundingBoxComponent *component)
    {
        return manager->AddComponent(entity, std::move(*component));
    }

    // VisibilityStateComponent
    uint32 VisibilityStateComponent_GetNativeTypeID()
    {
        return TypeID::ForType<VisibilityStateComponent>().Value();
    }

    ComponentID VisibilityStateComponent_AddComponent(EntityManager *manager, ManagedEntity entity, VisibilityStateComponent *component)
    {
        return manager->AddComponent(entity, std::move(*component));
    }

    // LightComponent
    uint32 LightComponent_GetNativeTypeID()
    {
        return TypeID::ForType<LightComponent>().Value();
    }

    ComponentID LightComponent_AddComponent(EntityManager *manager, ManagedEntity entity, LightComponent *component)
    {
        return manager->AddComponent(entity, std::move(*component));
    }

    // ShadowMapComponent
    uint32 ShadowMapComponent_GetNativeTypeID()
    {
        return TypeID::ForType<ShadowMapComponent>().Value();
    }

    ComponentID ShadowMapComponent_AddComponent(EntityManager *manager, ManagedEntity entity, ShadowMapComponent *component)
    {
        return manager->AddComponent(entity, std::move(*component));
    }
}
