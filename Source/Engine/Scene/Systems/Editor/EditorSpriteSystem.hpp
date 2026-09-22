/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Scene/System.hpp>
#include <Scene/Sprite.hpp>

#include <Scene/Components/TransformComponent.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Core/Math/Color.hpp>

namespace Hyperion {

class Texture;
class Scene;
struct Ray;
class RayTestResults;

enum class EditorSpriteIcon : uint32
{
    PointLight = 0,
    SpotLight,
    DirectionalLight,
    AreaLight,
    Camera,
    EnvProbe,
    LightmapVolume,

    Max
};

HYP_CLASS(EditorOnly, NoScriptBindings, Serialize = false)
class ENGINE_API EditorSpriteSystem final : public SystemBase
{
    HYP_OBJECT_BODY(EditorSpriteSystem);

public:
    EditorSpriteSystem();
    explicit EditorSpriteSystem(const Handle<Scene>& spriteScene);
    ~EditorSpriteSystem() override = default;

    bool RequiresSimThread() const override
    {
        return true;
    }

    bool AllowParallelExecution() const override
    {
        return false;
    }

    void OnEntityAdded(Entity* entity) override;
    void OnEntityRemoved(Entity* entity) override;

    void OnRemovedFromWorld(World* world) override;

    void Process(float delta, Span<Handle<Scene>> scenes) override;

    /*! \brief Tests the ray against each sprite, adding a hit for the entity the sprite represents. Must be called on the sim thread. */
    bool TestRay(const Ray& ray, RayTestResults& outResults) const;

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
        WeakHandle<Entity> trackedEntityWeak;
        Entity* trackedEntity = nullptr;
    };

    struct PendingEntityEvent
    {
        WeakHandle<Entity> entityWeak;
        Entity* entity = nullptr;
        bool isAdded = false;
    };

    void ApplyPendingEvents();

    void CreateSprite(Entity* entity);
    void RemoveSprite(Entity* entity);
    void RemoveAllSprites();

    const Handle<Texture>& GetIconTexture(EditorSpriteIcon icon);

    Handle<Scene> m_spriteScene;

    Array<SpriteMapping, SceneAllocator> m_spriteMappings;
    FixedArray<Handle<Texture>, uint32(EditorSpriteIcon::Max)> m_iconTextures;

    // entity add/remove notifications can come from any thread; sprites are only touched on the sim thread
    Mutex m_pendingEventsMutex;
    Array<PendingEntityEvent> m_pendingEvents;
};

} // namespace Hyperion
