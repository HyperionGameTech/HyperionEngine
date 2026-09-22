/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#ifdef HYP_EDITOR

#include <Scene/Systems/Editor/EditorSpriteSystem.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/LightmapVolume.hpp>

#include <Scene/Light/Light.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Rendering/Texture.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Core/Math/Ray.hpp>
#include <Core/Math/MathUtil.hpp>

#include <cmath>

#include <EditorSpriteSystem.generated.inl>

namespace Hyperion {

#pragma region EditorSpriteSystem

EditorSpriteSystem::EditorSpriteSystem()
    : EditorSpriteSystem(Handle<Scene>::Null())
{
}

EditorSpriteSystem::EditorSpriteSystem(const Handle<Scene>& spriteScene)
    : m_spriteScene(spriteScene)
{
}

void EditorSpriteSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    if (entity->IsA<Sprite>())
    {
        return;
    }

    Mutex::Guard guard(m_pendingEventsMutex);
    m_pendingEvents.PushBack(PendingEntityEvent { MakeWeakRef(entity), entity, /* isAdded */ true });
}

void EditorSpriteSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    if (entity->IsA<Sprite>())
    {
        return;
    }

    Mutex::Guard guard(m_pendingEventsMutex);
    m_pendingEvents.PushBack(PendingEntityEvent { WeakHandle<Entity>(), entity, /* isAdded */ false });
}

void EditorSpriteSystem::OnRemovedFromWorld(World* world)
{
    SystemBase::OnRemovedFromWorld(world);

    {
        Mutex::Guard guard(m_pendingEventsMutex);
        m_pendingEvents.Clear();
    }

    RemoveAllSprites();

    for (Handle<Texture>& iconTexture : m_iconTextures)
    {
        iconTexture.Reset();
    }
}

void EditorSpriteSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    ApplyPendingEvents();

    for (SpriteMapping& mapping : m_spriteMappings)
    {
        Handle<Entity> trackedEntity = mapping.trackedEntityWeak.Lock();

        if (!trackedEntity)
        {
            continue;
        }

        Vec3f spritePosition;

        if (trackedEntity->IsA<EnvProbe>() || trackedEntity->IsA<LightmapVolume>())
        {
            spritePosition = trackedEntity->GetWorldBounds().GetCenter();
        }
        else
        {
            spritePosition = trackedEntity->GetWorldTranslation();
        }

        if (mapping.sprite->GetWorldTranslation() != spritePosition)
        {
            mapping.sprite->SetWorldTranslation(spritePosition);
        }

        if (Light* light = DynamicCast<Light>(trackedEntity.Get()))
        {
            Color lightColor = light->GetColor();
            lightColor.SetAlpha(1.0f);

            if (mapping.sprite->color != lightColor)
            {
                mapping.sprite->color = lightColor;
            }
        }
    }
}

bool EditorSpriteSystem::TestRay(const Ray& ray, RayTestResults& outResults) const
{
    const Vec3f rayDirection = ray.direction.Normalized();

    bool hasHit = false;

    for (size_t mappingIndex = 0; mappingIndex < m_spriteMappings.Size(); mappingIndex++)
    {
        const SpriteMapping& mapping = m_spriteMappings[mappingIndex];

        Handle<Entity> trackedEntity = mapping.trackedEntityWeak.Lock();

        if (!trackedEntity)
        {
            continue;
        }

        // treat the camera-facing quad as a sphere; close enough for picking
        const Vec3f toCenter = mapping.sprite->GetWorldTranslation() - ray.position;
        const float radius = mapping.sprite->size * 0.5f;

        const float closestApproachDistance = toCenter.Dot(rayDirection);

        if (closestApproachDistance < 0.0f)
        {
            continue;
        }

        const float closestApproachSquared = toCenter.Dot(toCenter) - closestApproachDistance * closestApproachDistance;

        if (closestApproachSquared > radius * radius)
        {
            continue;
        }

        const float hitDistance = MathUtil::Max(closestApproachDistance - std::sqrt(radius * radius - closestApproachSquared), 0.0f);

        RayHit hit;
        hit.id = RayHitID(mappingIndex);
        hit.distance = hitDistance;
        hit.hitpoint = ray.position + rayDirection * hitDistance;
        hit.normal = -rayDirection;
        hit.node = trackedEntity.Get();

        if (outResults.AddHit(hit))
        {
            hasHit = true;
        }
    }

    return hasHit;
}

void EditorSpriteSystem::ApplyPendingEvents()
{
    Array<PendingEntityEvent> pendingEvents;

    {
        Mutex::Guard guard(m_pendingEventsMutex);
        pendingEvents = std::move(m_pendingEvents);
    }

    for (PendingEntityEvent& pendingEvent : pendingEvents)
    {
        if (!pendingEvent.isAdded)
        {
            RemoveSprite(pendingEvent.entity);

            continue;
        }

        // destroyed before we got to it; its removal event follows in the queue
        Handle<Entity> entity = pendingEvent.entityWeak.Lock();

        if (!entity)
        {
            continue;
        }

        CreateSprite(entity.Get());
    }
}

void EditorSpriteSystem::CreateSprite(Entity* entity)
{
    if (!m_spriteScene.IsValid())
    {
        return;
    }

    Scene* scene = entity->GetScene();

    if (!scene)
    {
        return;
    }

    if (!(scene->GetSceneFlags() & SceneFlags::FOREGROUND)
        || (scene->GetSceneFlags() & (SceneFlags::UI | SceneFlags::EDITOR | SceneFlags::BACKDROP)))
    {
        return;
    }

    if (m_spriteMappings.FindIf([entity](const SpriteMapping& mapping)
            {
                return mapping.trackedEntity == entity;
            })
        != m_spriteMappings.End())
    {
        return;
    }

    EditorIcons::Icon icon;

    SpriteType spriteType = SpriteType::None;
    Color spriteColor = Color::White();

    if (Light* light = DynamicCast<Light>(entity))
    {
        spriteType = SpriteType::Editor_Light;
        spriteColor = light->GetColor();
        spriteColor.SetAlpha(1.0f);

        switch (light->GetLightType())
        {
        case LightType::Directional:
            icon = EditorIcons::Icon::DirectionalLight;
            break;
        case LightType::Point:
            icon = EditorIcons::Icon::PointLight;
            break;
        case LightType::Spot:
            icon = EditorIcons::Icon::SpotLight;
            break;
        case LightType::AreaRect:
            icon = EditorIcons::Icon::AreaLight;
            break;
        default:
            return;
        }
    }
    else if (Camera* camera = DynamicCast<Camera>(entity))
    {
        if (camera->HasTag<EntityTag::EditorCamera>())
        {
            return;
        }

        // env probes parent their capture camera
        if (camera->GetParent() && camera->GetParent()->IsA<EnvProbe>())
        {
            return;
        }

        icon = EditorIcons::Icon::Camera;
        spriteType = SpriteType::Editor_Camera;
    }
    else if (EnvProbe* envProbe = DynamicCast<EnvProbe>(entity))
    {
        if (envProbe->IsA<SkyProbe>())
        {
            return;
        }

        icon = EditorIcons::Icon::EnvProbe;
        spriteType = SpriteType::Editor_EnvProbe;
        spriteColor = Color(0.6f, 1.0f, 1.0f, 1.0f);
    }
    else if (DynamicCast<LightmapVolume>(entity))
    {
        icon = EditorIcons::Icon::LightmapVolume;
        spriteType = SpriteType::Editor_LightmapVolume;
        spriteColor = Color(1.0f, 0.6f, 1.0f, 1.0f);
    }
    else
    {
        return;
    }

    Handle<Sprite> sprite = MakeHandle<Sprite>(NAME_FMT("{}_EditorSprite", entity->GetName()), spriteType);
    sprite->size = 1.0f;
    sprite->color = spriteColor;
    sprite->opacity = 1.0f;
    sprite->alwaysFaceCamera = true;
    sprite->texture = GetIconTexture(icon, EditorIcons::SpriteTextureSize);
    sprite->SetNodeFlags(sprite->GetNodeFlags() | NodeFlags::HideInSceneOutline);
    InitObject(sprite);

    m_spriteScene->GetRoot()->AddChild(sprite);

    SpriteMapping mapping;
    mapping.sprite = sprite;
    mapping.trackedEntityWeak = MakeWeakRef(entity);
    mapping.trackedEntity = entity;
    m_spriteMappings.PushBack(std::move(mapping));
}

void EditorSpriteSystem::RemoveSprite(Entity* entity)
{
    for (size_t mappingIndex = 0; mappingIndex < m_spriteMappings.Size(); mappingIndex++)
    {
        if (m_spriteMappings[mappingIndex].trackedEntity == entity)
        {
            m_spriteMappings[mappingIndex].sprite->Remove();
            m_spriteMappings.EraseAt(mappingIndex);

            break;
        }
    }
}

void EditorSpriteSystem::RemoveAllSprites()
{
    for (SpriteMapping& mapping : m_spriteMappings)
    {
        mapping.sprite->Remove();
    }

    m_spriteMappings.Clear();
}

const Handle<Texture>& EditorSpriteSystem::GetIconTexture(EditorIcons::Icon icon, uint32 textureSize)
{
    Assert(uint32(icon) < m_iconTextures.Size());

    Handle<Texture>& iconTexture = m_iconTextures[uint32(icon)];

    if (iconTexture.IsValid())
    {
        return iconTexture;
    }

    if (Handle<AssetRegistry> editorRegistry = GetEditorAssetRegistry(); editorRegistry.IsValid())
    {
        iconTexture = DynamicCast<Texture>(editorRegistry->GetAsset(AssetBuckets::Textures, EditorIcons::GetSpriteTextureName(icon)));
    }

    if (!iconTexture.IsValid())
    {
        HYP_LOG(Scene, Warning, "Editor icon '{}' not baked, will be rasterized at runtime.", EditorIcons::GetSpriteTextureName(icon));

        iconTexture = EditorIcons::CreateSpriteTexture(icon, textureSize);
        iconTexture->SetIsTransient(true);

        InitObject(iconTexture);
    }

    return iconTexture;
}

#pragma endregion EditorSpriteSystem

} // namespace Hyperion

#endif // HYP_EDITOR
