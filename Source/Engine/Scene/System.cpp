/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/System.hpp>
#include <Scene/Scene.hpp>
#include <Scene/EntityManager.hpp>

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
