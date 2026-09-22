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

#include <Core/Math/Ray.hpp>
#include <Core/Math/MathUtil.hpp>

#include <Core/Memory/ByteBuffer.hpp>

#include <cmath>

#include <EditorSpriteSystem.generated.inl>

namespace Hyperion {

#pragma region Icon rasterization

static constexpr uint32 g_iconSize = 128;
static constexpr float g_iconOutlineWidth = 0.09f;
static constexpr float g_iconOutlineOpacity = 0.85f;
static constexpr float g_iconOutlineShade = 0.08f;

static float SdCircle(float x, float y, float centerX, float centerY, float radius)
{
    return std::sqrt((x - centerX) * (x - centerX) + (y - centerY) * (y - centerY)) - radius;
}

static float SdRing(float x, float y, float centerX, float centerY, float radius, float halfWidth)
{
    return MathUtil::Abs(SdCircle(x, y, centerX, centerY, radius)) - halfWidth;
}

static float SdEllipseRing(float x, float y, float radiusX, float radiusY, float halfWidth)
{
    const float scaledLength = std::sqrt((x / radiusX) * (x / radiusX) + (y / radiusY) * (y / radiusY));

    if (scaledLength < 1e-5f)
    {
        return MathUtil::Min(radiusX, radiusY) - halfWidth;
    }

    // first order distance estimate: implicit value over its gradient length
    const float gradientX = x / (radiusX * radiusX);
    const float gradientY = y / (radiusY * radiusY);
    const float gradientLength = std::sqrt(gradientX * gradientX + gradientY * gradientY) / scaledLength;

    return MathUtil::Abs(scaledLength - 1.0f) / gradientLength - halfWidth;
}

static float SdRoundedBox(float x, float y, float centerX, float centerY, float halfWidth, float halfHeight, float cornerRadius)
{
    const float qx = MathUtil::Abs(x - centerX) - halfWidth + cornerRadius;
    const float qy = MathUtil::Abs(y - centerY) - halfHeight + cornerRadius;

    const float outsideX = MathUtil::Max(qx, 0.0f);
    const float outsideY = MathUtil::Max(qy, 0.0f);

    return std::sqrt(outsideX * outsideX + outsideY * outsideY) + MathUtil::Min(MathUtil::Max(qx, qy), 0.0f) - cornerRadius;
}

static float SdSegment(float x, float y, float startX, float startY, float endX, float endY, float radius)
{
    const float toPointX = x - startX;
    const float toPointY = y - startY;
    const float segmentX = endX - startX;
    const float segmentY = endY - startY;

    const float t = MathUtil::Clamp((toPointX * segmentX + toPointY * segmentY) / (segmentX * segmentX + segmentY * segmentY), 0.0f, 1.0f);

    const float deltaX = toPointX - segmentX * t;
    const float deltaY = toPointY - segmentY * t;

    return std::sqrt(deltaX * deltaX + deltaY * deltaY) - radius;
}

static float SdConvexPolygon(float x, float y, Span<const Vec2f> points)
{
    float signedArea = 0.0f;

    for (size_t i = 0; i < points.Size(); i++)
    {
        const Vec2f& a = points[i];
        const Vec2f& b = points[(i + 1) % points.Size()];

        signedArea += a.x * b.y - b.x * a.y;
    }

    const float windingSign = signedArea > 0.0f ? 1.0f : -1.0f;

    float distance = -MathUtil::Infinity<float>();

    for (size_t i = 0; i < points.Size(); i++)
    {
        const Vec2f& a = points[i];
        const Vec2f& b = points[(i + 1) % points.Size()];

        const float edgeX = b.x - a.x;
        const float edgeY = b.y - a.y;
        const float edgeLength = std::sqrt(edgeX * edgeX + edgeY * edgeY);

        const float normalX = windingSign * edgeY / edgeLength;
        const float normalY = windingSign * -edgeX / edgeLength;

        distance = MathUtil::Max(distance, (x - a.x) * normalX + (y - a.y) * normalY);
    }

    return distance;
}

static float SdPointLightIcon(float x, float y)
{
    static const Vec2f s_neck[] = { Vec2f(-0.3f, -0.05f), Vec2f(0.3f, -0.05f), Vec2f(0.2f, -0.42f), Vec2f(-0.2f, -0.42f) };

    float distance = SdCircle(x, y, 0.0f, 0.25f, 0.5f);
    distance = MathUtil::Min(distance, SdConvexPolygon(x, y, s_neck));
    distance = MathUtil::Min(distance, SdSegment(x, y, -0.18f, -0.56f, 0.18f, -0.56f, 0.07f));
    distance = MathUtil::Min(distance, SdSegment(x, y, -0.1f, -0.73f, 0.1f, -0.73f, 0.07f));

    return distance;
}

static float SdSpotLightIcon(float x, float y)
{
    static const Vec2f s_housing[] = { Vec2f(-0.2f, 0.82f), Vec2f(0.2f, 0.82f), Vec2f(0.46f, 0.22f), Vec2f(-0.46f, 0.22f) };

    float distance = SdConvexPolygon(x, y, s_housing);
    distance = MathUtil::Min(distance, SdSegment(x, y, -0.34f, -0.02f, -0.6f, -0.55f, 0.07f));
    distance = MathUtil::Min(distance, SdSegment(x, y, 0.0f, -0.06f, 0.0f, -0.62f, 0.07f));
    distance = MathUtil::Min(distance, SdSegment(x, y, 0.34f, -0.02f, 0.6f, -0.55f, 0.07f));

    return distance;
}

static float SdDirectionalLightIcon(float x, float y)
{
    float distance = SdCircle(x, y, 0.0f, 0.0f, 0.34f);

    for (int rayIndex = 0; rayIndex < 8; rayIndex++)
    {
        const float angle = float(rayIndex) * MathUtil::pi<float> * 0.25f;
        const float directionX = std::cos(angle);
        const float directionY = std::sin(angle);

        distance = MathUtil::Min(distance, SdSegment(x, y, directionX * 0.52f, directionY * 0.52f, directionX * 0.8f, directionY * 0.8f, 0.07f));
    }

    return distance;
}

static float SdAreaLightIcon(float x, float y)
{
    float distance = SdRoundedBox(x, y, 0.0f, 0.4f, 0.78f, 0.22f, 0.06f);

    for (float rayX : { -0.5f, 0.0f, 0.5f })
    {
        distance = MathUtil::Min(distance, SdSegment(x, y, rayX, -0.04f, rayX, -0.55f, 0.07f));
    }

    return distance;
}

static float SdCameraIcon(float x, float y)
{
    static const Vec2f s_lens[] = { Vec2f(0.3f, -0.08f), Vec2f(0.88f, 0.18f), Vec2f(0.88f, -0.58f), Vec2f(0.3f, -0.32f) };

    float distance = SdRoundedBox(x, y, -0.18f, -0.2f, 0.52f, 0.34f, 0.08f);
    distance = MathUtil::Min(distance, SdConvexPolygon(x, y, s_lens));
    distance = MathUtil::Min(distance, SdCircle(x, y, -0.45f, 0.42f, 0.22f));
    distance = MathUtil::Min(distance, SdCircle(x, y, 0.08f, 0.42f, 0.22f));

    return distance;
}

static float SdEnvProbeIcon(float x, float y)
{
    float distance = SdRing(x, y, 0.0f, 0.0f, 0.72f, 0.07f);
    distance = MathUtil::Min(distance, SdEllipseRing(x, y, 0.72f, 0.26f, 0.055f));
    distance = MathUtil::Min(distance, SdEllipseRing(x, y, 0.26f, 0.72f, 0.055f));

    return distance;
}

static float SdLightmapVolumeIcon(float x, float y)
{
    static const Vec2f s_front[] = { Vec2f(-0.62f, -0.7f), Vec2f(0.28f, -0.7f), Vec2f(0.28f, 0.2f), Vec2f(-0.62f, 0.2f) };
    static const Vec2f s_backOffset = Vec2f(0.34f, 0.42f);

    static constexpr float s_edgeRadius = 0.055f;

    float distance = MathUtil::Infinity<float>();

    for (int cornerIndex = 0; cornerIndex < 4; cornerIndex++)
    {
        const Vec2f& front = s_front[cornerIndex];
        const Vec2f& frontNext = s_front[(cornerIndex + 1) % 4];

        const Vec2f back = front + s_backOffset;
        const Vec2f backNext = frontNext + s_backOffset;

        distance = MathUtil::Min(distance, SdSegment(x, y, front.x, front.y, frontNext.x, frontNext.y, s_edgeRadius));
        distance = MathUtil::Min(distance, SdSegment(x, y, back.x, back.y, backNext.x, backNext.y, s_edgeRadius));
        distance = MathUtil::Min(distance, SdSegment(x, y, front.x, front.y, back.x, back.y, s_edgeRadius));
    }

    return distance;
}

static float SdIcon(EditorSpriteIcon icon, float x, float y)
{
    switch (icon)
    {
    case EditorSpriteIcon::PointLight:
        return SdPointLightIcon(x, y);
    case EditorSpriteIcon::SpotLight:
        return SdSpotLightIcon(x, y);
    case EditorSpriteIcon::DirectionalLight:
        return SdDirectionalLightIcon(x, y);
    case EditorSpriteIcon::AreaLight:
        return SdAreaLightIcon(x, y);
    case EditorSpriteIcon::Camera:
        return SdCameraIcon(x, y);
    case EditorSpriteIcon::EnvProbe:
        return SdEnvProbeIcon(x, y);
    case EditorSpriteIcon::LightmapVolume:
        return SdLightmapVolumeIcon(x, y);
    default:
        return MathUtil::Infinity<float>();
    }
}

static Handle<Texture> CreateIconTexture(EditorSpriteIcon icon)
{
    ByteBuffer imageBytes(size_t(g_iconSize) * size_t(g_iconSize) * 4u);
    ubyte* pixels = imageBytes.Data();

    const float pixelWidth = 2.0f / float(g_iconSize);

    for (uint32 row = 0; row < g_iconSize; row++)
    {
        const float y = 1.0f - (float(row) + 0.5f) * pixelWidth;

        for (uint32 column = 0; column < g_iconSize; column++)
        {
            const float x = (float(column) + 0.5f) * pixelWidth - 1.0f;

            const float distance = SdIcon(icon, x, y);

            const float fill = MathUtil::Clamp(0.5f - distance / pixelWidth, 0.0f, 1.0f);
            const float outline = MathUtil::Clamp(0.5f - (distance - g_iconOutlineWidth) / pixelWidth, 0.0f, 1.0f) * g_iconOutlineOpacity;

            const float outlineContribution = outline * (1.0f - fill);
            const float alpha = fill + outlineContribution;

            // transparent texels take the outline shade so mip filtering doesn't bleed white into the edge
            const float shade = alpha > 0.0f
                ? (fill + outlineContribution * g_iconOutlineShade) / alpha
                : g_iconOutlineShade;

            ubyte* pixel = pixels + (size_t(row) * g_iconSize + column) * 4u;
            pixel[0] = ubyte(shade * 255.0f + 0.5f);
            pixel[1] = ubyte(shade * 255.0f + 0.5f);
            pixel[2] = ubyte(shade * 255.0f + 0.5f);
            pixel[3] = ubyte(alpha * 255.0f + 0.5f);
        }
    }

    TextureDesc textureDesc {
        TextureType::Texture2D,
        TextureFormat::RGBA8_SRGB,
        Vec3u { g_iconSize, g_iconSize, 1 },
        TextureFilterMode::LinearMipmap,
        TextureFilterMode::Linear,
        TextureWrapMode::ClampToEdge
    };

    Texture::GenerateMipmaps(textureDesc, imageBytes);

    Handle<Texture> texture = MakeHandle<Texture>(textureDesc, imageBytes.ToByteView());
    texture->SetName(NAME_FMT("EditorSpriteIcon_{}", uint32(icon)));
    texture->SetIsTransient(true);
    InitObject(texture);

    return texture;
}

#pragma endregion Icon rasterization

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

    EditorSpriteIcon icon = EditorSpriteIcon::Max;
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
            icon = EditorSpriteIcon::DirectionalLight;
            break;
        case LightType::Point:
            icon = EditorSpriteIcon::PointLight;
            break;
        case LightType::Spot:
            icon = EditorSpriteIcon::SpotLight;
            break;
        case LightType::AreaRect:
            icon = EditorSpriteIcon::AreaLight;
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

        icon = EditorSpriteIcon::Camera;
        spriteType = SpriteType::Editor_Camera;
    }
    else if (EnvProbe* envProbe = DynamicCast<EnvProbe>(entity))
    {
        if (envProbe->IsA<SkyProbe>())
        {
            return;
        }

        icon = EditorSpriteIcon::EnvProbe;
        spriteType = SpriteType::Editor_EnvProbe;
        spriteColor = Color(0.6f, 1.0f, 1.0f, 1.0f);
    }
    else if (DynamicCast<LightmapVolume>(entity))
    {
        icon = EditorSpriteIcon::LightmapVolume;
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
    sprite->texture = GetIconTexture(icon);
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

const Handle<Texture>& EditorSpriteSystem::GetIconTexture(EditorSpriteIcon icon)
{
    Handle<Texture>& iconTexture = m_iconTextures[uint32(icon)];

    if (!iconTexture.IsValid())
    {
        iconTexture = CreateIconTexture(icon);
    }

    return iconTexture;
}

#pragma endregion EditorSpriteSystem

} // namespace Hyperion

#endif // HYP_EDITOR
