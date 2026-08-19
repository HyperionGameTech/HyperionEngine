/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/System.hpp>

#include <Core/Containers/Map.hpp>

namespace Hyperion {

enum class NetId : uint32;

HYP_CLASS(NoScriptBindings)
class ReplicationApplySystem final : public SystemBase
{
    HYP_OBJECT_BODY(ReplicationApplySystem);

public:
    ~ReplicationApplySystem() override = default;

    bool RequiresSimThread() const override
    {
        return true;
    }

    void Process(float delta, Span<Handle<Scene>> scenes) override;

    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {};
    }

private:
    Map<NetId, Handle<Entity>, SceneAllocator> m_netIdToEntity;
};

} // namespace Hyperion
