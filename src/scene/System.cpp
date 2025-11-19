/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <scene/System.hpp>
#include <scene/EntityManager.hpp>

#include <scene/Scene.hpp>

#include <core/reflection/Class.hpp>

#include <System.generated.inl>

namespace hyperion {

Name SystemBase::GetName() const
{
    return InstanceClass()->GetName();
}

Scene* SystemBase::GetScene() const
{
    return GetEntityManager().GetScene();
}

World* SystemBase::GetWorld() const
{
    return GetEntityManager().GetWorld();
}

void SystemBase::InitComponentInfos_Internal()
{
    m_componentTypeIds.Clear();
    m_componentInfos.Clear();

    Array<ComponentInfo> componentDescriptorsArray = GetComponentDescriptors().ToArray();
    m_componentTypeIds.Reserve(componentDescriptorsArray.Size());
    m_componentInfos.Reserve(componentDescriptorsArray.Size());

    for (const ComponentInfo& componentInfo : componentDescriptorsArray)
    {
        m_componentTypeIds.PushBack(componentInfo.typeId);
        m_componentInfos.PushBack(componentInfo);
    }
}

bool SystemBase::NeedsUpdateThisFrame() const
{
    if (!AllowUpdate())
    {
        return false;
    }

    EntityManager& entityManager = GetEntityManager();

    // if init'd dynamically, we can't rely on the EntitySetId being valid
    const EntitySetId entitySetId = GetComponentDescriptors().entitySetId;
    if (uint64(entitySetId) != 0)
    {
        const EntitySetBase* entitySet = entityManager.TryGetEntitySet(entitySetId);
        return entitySet != nullptr && entitySet->Size() != 0;
    }

    // just return true for now, this could be changed later
    return true;
}

} // namespace hyperion
