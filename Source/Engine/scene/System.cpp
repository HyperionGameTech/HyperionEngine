/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <ScenePch.hpp>

#include <scene/System.hpp>
#include <scene/Scene.hpp>
#include <scene/EntityManager.hpp>

#include <System.generated.inl>

namespace Hyperion {

Name SystemBase::GetName() const
{
    return InstanceClass()->GetName();
}

void SystemBase::InitComponentInfos_Internal()
{
    if (!m_componentInfos.Empty())
        // already init
        return;

    Array<ComponentInfo> componentDescriptorsArray = GetComponentDescriptors().ToArray();
    m_componentTypeIds.Reserve(componentDescriptorsArray.Size());
    m_componentInfos.Reserve(componentDescriptorsArray.Size());

    for (const ComponentInfo& componentInfo : componentDescriptorsArray)
    {
        m_componentTypeIds.PushBack(componentInfo.typeId);
        m_componentInfos.PushBack(componentInfo);
    }
}

} // namespace Hyperion
