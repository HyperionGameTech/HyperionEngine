/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include "Node.hpp"
#include <ScenePch.hpp>

#include <Scene/Sprite.hpp>
#include <Scene/Scene.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/LightmapVolume.hpp>
#include <Scene/Camera/Camera.hpp>

#include <Rendering/RenderProxy.hpp>
#include <Rendering/Texture.hpp>

#include <Sprite.generated.inl>

namespace Hyperion {

Sprite::Sprite()
    : Sprite(Name::Invalid(), SpriteType::None)
{
}

Sprite::Sprite(Name name, SpriteType spriteType)
    : Entity(name),
      spriteType(spriteType)
{
}

Sprite::~Sprite() = default;

void Sprite::Init()
{
    Entity::Init();

    SetNodeFlags(m_nodeFlags | NodeFlags::ExcludeFromParentBounds | NodeFlags::ExcludeFromOctree);
}

void Sprite::UpdateRenderProxy(RenderProxySprite* proxy)
{
    proxy->sprite = this;
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
    Handle<Sprite> sprite = MakeHandle<Sprite>(NAME_FMT("{}_Sprite", envProbe->GetName()), SpriteType::EnvProbe);
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
    Handle<Sprite> sprite = MakeHandle<Sprite>(NAME_FMT("{}_Sprite", lightmapVolume->GetName()), SpriteType::LightmapVolume);
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
    Handle<Sprite> sprite = MakeHandle<Sprite>(NAME_FMT("{}_Sprite", camera->GetName()), SpriteType::Camera);
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
