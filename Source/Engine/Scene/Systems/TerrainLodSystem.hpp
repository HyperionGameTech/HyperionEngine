/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/System.hpp>

#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/TerrainPatchComponent.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings, Serialize = false)
class TerrainLodSystem final : public SystemBase
{
    HYP_OBJECT_BODY(TerrainLodSystem);

public:
    ~TerrainLodSystem() override = default;

    bool RequiresSimThread() const override
    {
        return true;
    }

    void Process(float delta, Span<Handle<Scene>> scenes) override;

private:
    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<MeshComponent, ComponentAccess::READ_WRITE> {},
            ComponentDescriptor<TerrainPatchComponent, ComponentAccess::READ_WRITE> {}
        };
    }
};

} // namespace Hyperion
