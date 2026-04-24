/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#if HYP_EDITOR

#include <scene/systems/editor/EditorSpriteSystem.hpp>

#include <scene/EntityManager.hpp>
#include <scene/Scene.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/LightmapVolume.hpp>
#include <scene/camera/Camera.hpp>

#include <EditorSpriteSystem.generated.inl>

namespace Hyperion {

void EditorSpriteSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    Scene* scene = entity->GetScene();
    if (!scene)
    {
        return;
    }

    Handle<Sprite> sprite;
    
    if (EnvProbe* envProbe = DynamicCast<EnvProbe>(entity))
    {
        if (envProbe->IsSkyProbe())
        {
            return;
        }
        
        sprite = Sprite::CreateEnvProbeSprite(scene, envProbe);
    }
    else if (LightmapVolume* lightmapVolume = DynamicCast<LightmapVolume>(entity))
    {
        sprite = Sprite::CreateLightmapVolumeSprite(scene, lightmapVolume);
    }
    else if (Camera* camera = DynamicCast<Camera>(entity))
    {
        sprite = Sprite::CreateCameraSprite(scene, camera);
    }

    if (sprite.IsValid())
    {
        SpriteMapping mapping;
        mapping.sprite = sprite;
        mapping.trackedEntity = entity;
        m_spriteMappings.PushBack(mapping);
    }
}

void EditorSpriteSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    for (size_t i = 0; i < m_spriteMappings.Size(); ++i)
    {
        if (m_spriteMappings[i].trackedEntity == entity)
        {
            m_spriteMappings[i].sprite->Remove();
            m_spriteMappings.EraseAt(i);
            break;
        }
    }
}

void EditorSpriteSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    for (auto& mapping : m_spriteMappings)
    {
        if (mapping.sprite.IsValid() && mapping.trackedEntity)
        {
            if (EnvProbe* envProbe = DynamicCast<EnvProbe>(mapping.trackedEntity))
            {
                mapping.sprite->SetWorldTranslation(envProbe->GetWorldBounds().GetCenter());
            }
            else if (LightmapVolume* lightmapVolume = DynamicCast<LightmapVolume>(mapping.trackedEntity))
            {
                mapping.sprite->SetWorldTranslation(lightmapVolume->GetWorldBounds().GetCenter());
            }
            else if (Camera* camera = DynamicCast<Camera>(mapping.trackedEntity))
            {
                mapping.sprite->SetWorldTranslation(camera->GetWorldTranslation());
            }
        }
    }
}

} // namespace Hyperion

#endif // HYP_EDITOR