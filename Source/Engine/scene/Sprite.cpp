/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <scene/Sprite.hpp>
#include <scene/Scene.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/LightmapVolume.hpp>
#include <scene/camera/Camera.hpp>

#include <rendering/RenderProxy.hpp>
#include <rendering/Texture.hpp>

#include <Sprite.generated.inl>

namespace Hyperion {

Sprite::Sprite()
    : Sprite(SpriteType::None)
{
}

Sprite::Sprite(SpriteType spriteType)
    : Entity(NAME("Sprite"))
{
    spriteType = spriteType;
}

Sprite::~Sprite() = default;

void Sprite::Init()
{
    Entity::Init();
}

void Sprite::UpdateRenderProxy(RenderProxySprite* proxy)
{
    proxy->sprite = WeakHandleFromThis();
    proxy->texture = texture.Get();

    SpriteShaderData& bufferData = proxy->bufferData;
    bufferData.positionSize = Vec4f(GetWorldTranslation(), size);
    bufferData.color = Vec4f(color);
    bufferData.spriteType = uint32(spriteType);
    bufferData.opacity = opacity;
    bufferData.alwaysFaceCamera = alwaysFaceCamera ? 1u : 0u;
    bufferData.textureIndex = ~0u;
}

Handle<Sprite> Sprite::CreateEnvProbeSprite(Scene* scene, EnvProbe* envProbe)
{
    Handle<Sprite> sprite = MakeHandle<Sprite>(SpriteType::EnvProbe);
    InitObject(sprite);

    sprite->size = 2.0f;
    sprite->color = Color::Cyan();
    sprite->opacity = 0.5f;
    sprite->alwaysFaceCamera = true;
    sprite->m_envProbe = MakeStrongRef(envProbe);

    if (envProbe)
    {
        sprite->SetWorldTranslation(envProbe->GetWorldBounds().GetCenter());
    }

    scene->GetRoot()->AddChild(sprite);

    return sprite;
}

Handle<Sprite> Sprite::CreateLightmapVolumeSprite(Scene* scene, LightmapVolume* lightmapVolume)
{
    Handle<Sprite> sprite = MakeHandle<Sprite>(SpriteType::LightmapVolume);
    InitObject(sprite);

    sprite->size = 2.0f;
    sprite->color = Color::Magenta();
    sprite->opacity = 0.5f;
    sprite->alwaysFaceCamera = true;
    sprite->m_lightmapVolume = MakeStrongRef(lightmapVolume);

    if (lightmapVolume)
    {
        sprite->SetWorldTranslation(lightmapVolume->GetWorldBounds().GetCenter());
    }

    scene->GetRoot()->AddChild(sprite);

    return sprite;
}

Handle<Sprite> Sprite::CreateCameraSprite(Scene* scene, Camera* camera)
{
    Handle<Sprite> sprite = MakeHandle<Sprite>(SpriteType::Camera);
    InitObject(sprite);

    sprite->size = 1.5f;
    sprite->color = Color::Yellow();
    sprite->opacity = 0.5f;
    sprite->alwaysFaceCamera = true;
    sprite->m_camera = MakeStrongRef(camera);

    if (camera)
    {
        sprite->SetWorldTranslation(camera->GetWorldTranslation());
    }

    scene->GetRoot()->AddChild(sprite);

    return sprite;
}

} // namespace Hyperion