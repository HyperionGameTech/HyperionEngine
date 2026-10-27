/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Scene/System.hpp>

#include <PluginTest/PluginTestComponent.hpp>

namespace Hyperion {

HYP_CLASS(Serialize = false)
class PLUGINTEST_API PluginTestSystem final : public SystemBase
{
    HYP_OBJECT_BODY(PluginTestSystem);

public:
    ~PluginTestSystem() override = default;

    // component updates since the system was added; read from C# to test a plugin's generated bindings
    HYP_METHOD()
    uint32 GetTotalTicks() const
    {
        return m_totalTicks;
    }

    bool ShouldProcessScene(Scene* scene) const override;

    bool AllowParallelExecution() const override
    {
        return false;
    }

    bool RequiresSimThread() const override
    {
        return true;
    }

    void Process(float delta, Span<Handle<Scene>> scenes) override;

private:
    bool m_hasLoggedFirstUpdate = false;
    uint32 m_totalTicks = 0;

    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<PluginTestComponent, ComponentAccess::READ_WRITE> {}
        };
    }
};

} // namespace Hyperion
