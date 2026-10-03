/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/System.hpp>

#include <Scene/Components/WeaponComponent.hpp>
#include <Scene/Components/MeshComponent.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class WeaponSystem final : public SystemBase
{
    HYP_OBJECT_BODY(WeaponSystem);

public:
    ~WeaponSystem() override = default;

    bool AllowUpdate() const override
    {
        return true;
    }

    bool RequiresSimThread() const override
    {
        return true;
    }

    bool ShouldProcessScene(Scene* scene) const override;

    void OnEntityAdded(Entity* entity) override;
    void OnEntityRemoved(Entity* entity) override;

    void Process(float delta, Span<Handle<Scene>> scenes) override;

private:
    void SetWeaponModel(Entity& entity, Weapon& weapon) const;

    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<WeaponComponent, ComponentAccess::READ_WRITE> {},

            // adds or sets the mesh/material
            ComponentDescriptor<MeshComponent, ComponentAccess::READ_WRITE, false> {}
        };
    }
};

} // namespace Hyperion
