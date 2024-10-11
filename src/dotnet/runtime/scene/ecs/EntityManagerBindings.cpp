/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/EntityManager.hpp>

// Components
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/LightComponent.hpp>
#include <scene/ecs/components/ShadowMapComponent.hpp>
#include <scene/ecs/components/UIComponent.hpp>
#include <scene/ecs/components/NodeLinkComponent.hpp>

#include <scene/animation/Skeleton.hpp>

#include <dotnet/runtime/ManagedHandle.hpp>
#include <dotnet/runtime/scene/ManagedSceneTypes.hpp>
#include <dotnet/core/RefCountedPtrBindings.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C" {
HYP_EXPORT ManagedEntity EntityManager_AddEntity(EntityManager *manager)
{
    AssertThrow(manager != nullptr);

    return manager->AddEntity();
}

HYP_EXPORT void EntityManager_RemoveEntity(EntityManager *manager, ManagedEntity entity)
{
    AssertThrow(manager != nullptr);
    
    manager->RemoveEntity(entity);
}

HYP_EXPORT bool EntityManager_HasEntity(EntityManager *manager, ManagedEntity entity)
{
    AssertThrow(manager != nullptr);
    
    return manager->HasEntity(entity);
}

HYP_EXPORT bool EntityManager_HasComponent(EntityManager *manager, uint32 component_type_id, ManagedEntity entity)
{
    AssertThrow(manager != nullptr);
    
    return manager->HasComponent(TypeID { component_type_id }, entity);
}

HYP_EXPORT void *EntityManager_GetComponent(EntityManager *manager, uint32 component_type_id, ManagedEntity entity)
{
    AssertThrow(manager != nullptr);
    
    return manager->TryGetComponent(TypeID { component_type_id }, entity).GetPointer();
}

HYP_EXPORT ComponentID EntityManager_AddComponent(EntityManager *manager, ManagedEntity entity, uint32 component_type_id, void *component_ptr)
{
    AssertThrow(manager != nullptr);
    AssertThrow(component_ptr != nullptr);

    if (!manager->IsValidComponentType(TypeID { component_type_id })) {
        return EntityManager::invalid_component_id;
    }

    return manager->AddComponent(entity, AnyRef(TypeID { component_type_id }, component_ptr));
}


// MeshComponent
// struct ManagedMeshComponent
// {
//     ManagedHandle           mesh_handle;
//     ManagedHandle           material_handle;
//     ManagedHandle           skeleton_handle;
//     uint32                  num_instances;
//     ManagedRefCountedPtr    proxy_rc;
//     uint32                  mesh_component_flags;
//     Matrix4                 previous_model_matrix;
//     ubyte                   user_data[16];
// };

// static_assert(sizeof(ManagedMeshComponent) == sizeof(MeshComponent), "ManagedMeshComponent should equal MeshComponent size");
// static_assert(alignof(ManagedMeshComponent) == alignof(MeshComponent), "ManagedMeshComponent should have the same alignment as MeshComponent");
// static_assert(std::is_trivially_copyable_v<ManagedMeshComponent> && std::is_standard_layout_v<ManagedMeshComponent>, "ManagedMeshComponent should be trivially copyable and standard layout");
// static_assert(sizeof(ManagedMeshComponent) == 112, "ManagedMeshComponent should equal 96 bytes to match C# struct size");

// HYP_EXPORT uint32 MeshComponent_GetNativeTypeID()
// {
//     return TypeID::ForType<MeshComponent>().Value();
// }

// HYP_EXPORT ComponentID MeshComponent_AddComponent(EntityManager *manager, ManagedEntity entity, ManagedMeshComponent *component)
// {
//     Handle<Mesh> mesh = CreateHandleFromManagedHandle<Mesh>(component->mesh_handle);
//     Handle<Material> material = CreateHandleFromManagedHandle<Material>(component->material_handle);

//     RC<RenderProxy> proxy = MarshalRefCountedPtr<RenderProxy>(component->proxy_rc);

//     return manager->AddComponent<MeshComponent>(entity, MeshComponent {
//         std::move(mesh),
//         std::move(material),
//         Handle<Skeleton> { },
//         component->num_instances,
//         std::move(proxy),
//         component->mesh_component_flags,
//         component->previous_model_matrix,
//         MeshComponentUserData::InternFromBytes(component->user_data)
//     });
// }

} // extern "C"
