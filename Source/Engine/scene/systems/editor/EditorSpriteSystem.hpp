/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <scene/System.hpp>

#include <scene/Sprite.hpp>

#include <scene/EnvProbe.hpp>
#include <scene/LightmapVolume.hpp>
#include <scene/camera/Camera.hpp>

namespace Hyperion {

HYP_CLASS(EditorOnly, NoScriptBindings)
class HYP_API EditorSpriteSystem final : public SystemBase
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
        return {};
    }

    struct SpriteMapping
    {
        Handle<Sprite> sprite;
        Entity* trackedEntity;
    };

    Array<SpriteMapping, SceneAllocator> m_spriteMappings;
};

} // namespace Hyperion