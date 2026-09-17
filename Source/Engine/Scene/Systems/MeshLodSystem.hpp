/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Scene/System.hpp>
#include <Scene/EntityTag.hpp>

#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/BoundingBoxComponent.hpp>
#include <Scene/Components/TerrainPatchComponent.hpp>

#include <Framework/CVarManager.hpp>

namespace Hyperion {

extern ENGINE_API CVar<int32> g_cvMeshLodForceLod;

HYP_CLASS(NoScriptBindings, Serialize = false)
class MeshLodSystem final : public SystemBase
{
    HYP_OBJECT_BODY(MeshLodSystem);

public:
    ~MeshLodSystem() override = default;

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
            ComponentDescriptor<BoundingBoxComponent, ComponentAccess::READ> {},

            ///Terrain patches use TerrainLodSystem instead of this
            ComponentDescriptor<TerrainPatchComponent, ComponentAccess::READ, false> {},
            ComponentDescriptor<TagComponent<EntityTag::MeshLodPinned>, ComponentAccess::READ, false> {}
        };
    }
};

} // namespace Hyperion
