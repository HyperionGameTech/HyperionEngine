/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/System.hpp>
#include <scene/EntityManager.hpp>

#include <scene/Scene.hpp>

#include <core/object/HypClass.hpp>

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

} // namespace hyperion