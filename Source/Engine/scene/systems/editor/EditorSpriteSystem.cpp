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
#include <scene/World.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/LightmapVolume.hpp>
#include <scene/TextSprite.hpp>

#include <scene/camera/Camera.hpp>

#include <editor/EditorSubsystem.hpp>

#include <EditorSpriteSystem.generated.inl>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

void EditorSpriteSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    Scene* scene = entity->GetScene();
    if (!scene)
    {
        return;
    }

    if (!(scene->GetSceneFlags() & SceneFlags::FOREGROUND)
        || (scene->GetSceneFlags() & (SceneFlags::UI | SceneFlags::EDITOR)))
    {
        return;
    }

    //Scene* editorScene = nullptr;

    //auto editorSceneIt = GetWorld()->GetScenes().FindIf([](const Handle<Scene>& scene)
    //    {
    //        return scene->GetSceneFlags() & SceneFlags::EDITOR;
    //    });

    //if (editorSceneIt == GetWorld()->GetScenes().End())
    //{
    //    HYP_LOG(Editor, Error, "No Editor scene found! Cannot add editor sprite");

    //    return;
    //}

    //editorScene = *editorSceneIt;

    Handle<Sprite> sprite;

    if (EnvProbe* envProbe = DynamicCast<EnvProbe>(entity))
    {
        if (envProbe->IsA<SkyProbe>())
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
        if (!camera->HasTag<EntityTag::EditorCamera>())
        {
            sprite = MakeHandle<TextSprite>(NAME_FMT("{}_Sprite_Text", camera->GetName()), *camera->GetName());
            scene->GetRoot()->AddChild(sprite);
        }
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
#if 0
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
#endif
}

} // namespace Hyperion

#endif // HYP_EDITOR
