/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Scene/System.hpp>
#include <Scene/Sprite.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/LightmapVolume.hpp>

#include <Scene/components/TransformComponent.hpp>

#include <Scene/camera/Camera.hpp>

namespace Hyperion {

HYP_CLASS(EditorOnly, NoScriptBindings)
class ENGINE_API EditorSpriteSystem final : public SystemBase
{
    HYP_OBJECT_BODY(EditorSpriteSystem);

public:
    ~EditorSpriteSystem() override = default;

    void OnEntityAdded(Entity* entity) override;
    void OnEntityRemoved(Entity* entity) override;

    void Process(float delta, Span<Handle<Scene>> scenes) override;

private:
    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<TransformComponent, ComponentAccess::READ> {}
        };
    }

    struct SpriteMapping
    {
        Handle<Sprite> sprite;
        Entity* trackedEntity;
    };

    Array<SpriteMapping, SceneAllocator> m_spriteMappings;
};

} // namespace Hyperion
