/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Light.hpp>
#include <Scene/View.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/EntityTag.hpp>
#include <Scene/EntityManager.hpp>

#include <Scene/Camera/Camera.hpp>
#include <Scene/Camera/OrthoCamera.hpp>
#include <Scene/Camera/PerspectiveCamera.hpp>

#include <Rendering/Shadows/ShadowMap.hpp>
#include <Rendering/Shadows/ShadowCameraHelper.hpp>

#include <Rendering/MaterialInstance.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/RenderInterface.hpp>

#include <Rendering/Passes/ShadowsPass.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Core/Threading/Threads.hpp>

// for EnumToString
#include <Core/Reflection/Enum.hpp>

#include <Core/Utilities/Float16.hpp>

#include <Framework/EngineDriver.hpp>

#if HYP_EDITOR
#include <Baking/BakerSubsystem.hpp>
#include <Baking/ShadowMap/ShadowMapBakeData.hpp>
#endif

#include <Light.generated.inl>

namespace Hyperion {

#if HYP_EDITOR
EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);
#endif // HYP_EDITOR

static constexpr Vec2u DefaultShadowMapDimensions[NumLightTypes] = {
    Vec2u(1024, 1024), // LightType::Directional
    Vec2u(256, 256),   // LightType::Point
    Vec2u(256, 256),   // LightType::Spot
    Vec2u(256, 256)    // LightType::AreaRect
};

#pragma region Light

Light::Light()
    : Light(LightType::Directional, Vec3f::Zero(), Color::White(), 1.0f, 1.0f)
{
}

Light::Light(LightType type, const Vec3f& position, const Color& color, float intensity, float radius)
    : m_type(type),
      m_lightFlags(LightFlags::Default),
      m_color(color),
      m_intensity(intensity),
      m_radius(MathUtil::Max(radius, 0.001f)),
      m_falloff(1.0f),
      m_spotAngles(Vec2f::Zero()),
      m_shadowMapDimensions(DefaultShadowMapDimensions[uint32(type)]),
      m_numShadowMapCascades(1)
{
    m_entityInitInfo.canEverUpdate = true;
    m_entityInitInfo.receivesUpdate = true;
    m_entityInitInfo.bvhDepth = 0; // No BVH for lights
    m_entityInitInfo.initialTags = { EntityTag::Light };

    Entity::SetLocalTranslation(position);
}

Light::Light(LightType type, const Vec3f& position, const Vec3f& normal, const Vec2f& areaSize, const Color& color, float intensity, float radius)
    : m_type(type),
      m_lightFlags(LightFlags::Default),
      m_normal(normal),
      m_areaSize(areaSize),
      m_color(color),
      m_intensity(intensity),
      m_radius(MathUtil::Max(radius, 0.001f)),
      m_falloff(1.0f),
      m_spotAngles(Vec2f::Zero()),
      m_shadowMapDimensions(DefaultShadowMapDimensions[uint32(type)]),
      m_numShadowMapCascades(1)
{
    m_entityInitInfo.canEverUpdate = true;
    m_entityInitInfo.receivesUpdate = true;
    m_entityInitInfo.bvhDepth = 0; // No BVH for lights
    m_entityInitInfo.initialTags = { EntityTag::Light };

    Entity::SetLocalTranslation(position);
}

Light::~Light()
{
    if (m_material.IsValid())
    {
        EnqueueDeletion(std::move(m_material));
    }

    if (m_shadowMap.IsValid())
    {
        EnqueueDeletion(std::move(m_shadowMap));
    }
}

void Light::Init()
{
    HYP_SCOPE;

    Entity::Init();

    if (m_material.IsValid())
    {
        InitObject(m_material);
    }

    if (m_shadowMap.IsValid())
    {
        CheckResult(m_shadowMap->Create());
    }

    SetReady(true);
}

void Light::OnAttachedToNode(Node* node)
{
    Entity::OnAttachedToNode(node);
}

void Light::OnDetachedFromNode(Node* node)
{
    Entity::OnDetachedFromNode(node);
}

void Light::OnAddedToScene(Scene* scene)
{
    Entity::OnAddedToScene(scene);
}

void Light::OnRemovedFromScene(Scene* scene)
{
    Entity::OnRemovedFromScene(scene);
}

void Light::OnTransformUpdated()
{
    Entity::OnTransformUpdated();
    Entity::SetLocalBounds(CalculateLightBounds());
}

void Light::Update(float delta)
{
    if (m_lightFlags & LightFlags::ShadowCaster)
    {
        SetNeedsRenderProxyUpdate();
    }
}

void Light::SetLightFlags(EnumFlags<LightFlags> flags)
{
    if (m_lightFlags == flags)
    {
        return;
    }

    if (!(flags & LightFlags::BakeStaticShadows) && m_shadowMap.IsValid())
    {
        EnqueueDeletion(std::move(m_shadowMap));
    }

    m_lightFlags = flags;

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void Light::SetNormal(const Vec3f& normal)
{
    if (m_normal == normal)
    {
        return;
    }

    m_normal = normal;

    Entity::SetLocalBounds(CalculateLightBounds());

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void Light::SetAreaSize(const Vec2f& areaSize)
{
    if (m_areaSize == areaSize)
    {
        return;
    }

    m_areaSize = areaSize;

    Entity::SetLocalBounds(CalculateLightBounds());

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void Light::SetColor(const Color& color)
{
    if (m_color == color)
    {
        return;
    }

    m_color = color;

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void Light::SetIntensity(float intensity)
{
    if (m_intensity == intensity)
    {
        return;
    }

    m_intensity = intensity;

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void Light::SetRadius(float radius)
{
    if (m_radius == radius)
    {
        return;
    }

    m_radius = radius;

    Entity::SetLocalBounds(CalculateLightBounds());

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void Light::SetFalloff(float falloff)
{
    if (m_falloff == falloff)
    {
        return;
    }

    m_falloff = falloff;

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void Light::SetSpotAngles(const Vec2f& spotAngles)
{
    if (m_spotAngles == spotAngles)
    {
        return;
    }

    m_spotAngles = spotAngles;

    Entity::SetLocalBounds(CalculateLightBounds());

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void Light::SetMaterial(Handle<MaterialInstance> material)
{
    if (material == m_material)
    {
        return;
    }

    if (m_material)
    {
        EnqueueDeletion(std::move(m_material));
    }

    m_material = std::move(material);
    InitObject(m_material);

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

Pair<Vec3f, Vec3f> Light::CalculateAreaLightRect() const
{
    Vec3f tangent;
    Vec3f bitangent;
    MathUtil::ComputeOrthonormalBasis(m_normal, tangent, bitangent);

    const float halfWidth = m_areaSize.x * 0.5f;
    const float halfHeight = m_areaSize.y * 0.5f;

    const Vec3f center = GetLocalTranslation();

    const Vec3f p0 = center - tangent * halfWidth - bitangent * halfHeight;
    const Vec3f p1 = center + tangent * halfWidth - bitangent * halfHeight;
    const Vec3f p2 = center + tangent * halfWidth + bitangent * halfHeight;
    const Vec3f p3 = center - tangent * halfWidth + bitangent * halfHeight;

    return { p0, p2 };
}

void Light::SetShadowMapDimensions(Vec2u shadowMapDimensions)
{
    shadowMapDimensions = MathUtil::Max(shadowMapDimensions, Vec2u::One());

    if (shadowMapDimensions == m_shadowMapDimensions)
    {
        return;
    }

    if (m_shadowMap.IsValid())
    {
        EnqueueDeletion(std::move(m_shadowMap));
    }

    m_shadowMapDimensions = shadowMapDimensions;

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void Light::SetNumShadowMapCascades(uint32 numShadowMapCascades)
{
    numShadowMapCascades = MathUtil::Clamp(numShadowMapCascades, 1u, MaxShadowMapCascades);

    if (numShadowMapCascades == m_numShadowMapCascades)
    {
        return;
    }

    if (m_shadowMap.IsValid())
    {
        EnqueueDeletion(std::move(m_shadowMap));
    }

    m_numShadowMapCascades = numShadowMapCascades;

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void Light::SetShadowMapFilter(ShadowMapFilter shadowMapFilter)
{
    if (shadowMapFilter == GetShadowMapFilter())
    {
        return;
    }

    m_lightFlags &= ~LightFlags::ShadowFilterMask;

    // ShadowMapFilter enum members are sequentially ordered so turn it into a flag
    m_lightFlags |= EnumFlags<LightFlags>(1u << shadowMapFilter);

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void Light::SetBakedShadowMap(const Handle<Texture>& shadowMap)
{
    if (!CanBakeStaticShadows())
    {
        return;
    }

    if (m_shadowMap == shadowMap)
    {
        return;
    }

    if (m_shadowMap.IsValid())
    {
        EnqueueDeletion(std::move(m_shadowMap));
    }

    m_shadowMap = shadowMap;

    if (m_shadowMap.IsValid())
    {
        m_lightFlags |= LightFlags::BakeStaticShadows | LightFlags::ShadowCaster;

        if (IsInitCalled())
        {
            CheckResult(m_shadowMap->Create());
        }
    }

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void Light::SetLocalBounds(const BoundingBox& localBounds)
{
    switch (m_type)
    {
    case LightType::Directional:
        // for directional we ignore the local bounds and just set it to infinite since the light affects everything in the scene
        m_localBounds = BoundingBox::Infinity();
        break;
    case LightType::Point:
    {
        // use the new localBounds to determine the radius of the point light
        const float newRadius = localBounds.GetExtent().Length() * 0.5f;
        m_radius = newRadius;

        Entity::SetLocalBounds(CalculateLightBounds());

        break;
    }
    case LightType::Spot:
    {
        // for spot lights we use the local bounds to determine the radius and spot angles. The local bounds should be a cone shape with the tip at the origin and pointing down the negative Z axis. The radius is determined by the distance from the origin to the center of the base of the cone, and the spot angles are determined by the angle between the negative Z axis and the corners of the base of the cone.
        const Vec3f extent = localBounds.GetExtent();
        const float newRadius = extent.Length() * 0.5f;

        const Vec3f center = localBounds.GetCenter();
        const float angleX = std::atan2(extent.x * 0.5f, center.z);
        const float angleY = std::atan2(extent.y * 0.5f, center.z);

        m_radius = newRadius;
        m_spotAngles = Vec2f(angleX, angleY);

        Entity::SetLocalBounds(CalculateLightBounds());

        break;
    }
    case LightType::AreaRect:
    {
        // for area rect lights we use the local bounds to determine the area size. The local bounds should be a box shape with the center at the origin and facing down the negative Z axis. The area size is determined by the X and Y extent of the box.
        const Vec3f extent = localBounds.GetExtent();
        m_areaSize = Vec2f(extent.x, extent.y);

        Entity::SetLocalBounds(CalculateLightBounds());

        break;
    }
    default:
        HYP_UNREACHABLE();
    }
}

// Local space
BoundingBox Light::CalculateLightBounds() const
{
    if (m_type == LightType::Directional)
    {
        return BoundingBox::Infinity();
    }

    if (m_type == LightType::AreaRect)
    {
        const Pair<Vec3f, Vec3f> rect = CalculateAreaLightRect();

        return BoundingBox::Empty()
            .Union(rect.first)
            .Union(rect.second)
            .Union(Vec3f::Zero() + m_normal * m_radius);
    }

    if (m_type == LightType::Point)
    {
        return BoundingBox(GetBoundingSphere(false));
    }

    return BoundingBox::Empty();
}

BoundingSphere Light::GetBoundingSphere(bool worldSpace) const
{
    if (m_type == LightType::Directional)
    {
        return BoundingSphere::infinity;
    }

    return BoundingSphere(worldSpace ? GetWorldTranslation() : Vec3f::Zero(), m_radius);
}

void Light::UpdateRenderProxy(RenderProxyLight* proxy)
{
    proxy->light = MakeWeakRef(this);
    proxy->lightMaterial = m_material.Get();
    proxy->bakedShadowMap = m_shadowMap.Get();
    proxy->numCascades = m_numShadowMapCascades;

    Vec3f lightPosition = GetWorldTranslation();

    if (m_type == LightType::Directional)
    {
        lightPosition = lightPosition.Normalize();
    }

    LightShaderData& bufferData = proxy->bufferData;
    bufferData.lightType = uint32(m_type);
    bufferData.color = Vec4f(m_color);
    bufferData.radiusFalloffPacked = (uint32(Float16(m_falloff).Raw()) << 16) | Float16(m_radius).Raw();
    bufferData.positionIntensity = Vec4f(lightPosition, m_intensity);
    bufferData.materialIndex = ~0u; // materialIndex gets set in WriteBufferData_Light()
    bufferData.flags = m_lightFlags;

    switch (GetLightType())
    {
    case LightType::AreaRect:
        bufferData.areaSize = m_areaSize;
        bufferData.areaNormal = m_normal;
        break;
    case LightType::Spot:
        bufferData.areaSize = m_spotAngles;
        bufferData.spotLightDir = m_normal;
        break;
    case LightType::Point:
        break;
    default:
        break;
    }
}

#if HYP_EDITOR

bool Light::CanBakeStaticShadows() const
{
    return !IsA(DirectionalLight::StaticClass());
}

void Light::BakeStaticShadows()
{
    if (!CanBakeStaticShadows())
    {
        HYP_LOG(Editor, Warning, "Light {} cannot have static shadow maps baked", GetName());
        return;
    }

    World* world = GetWorld();
    AssertDebug(world != nullptr);

    if (!world)
    {
        HYP_LOG(Editor, Error, "Cannot bake  Light {}: not attached to a World", GetName());

        return;
    }

    BakerSubsystem* bakerSubsystem = world->GetSubsystem<BakerSubsystem>();

    if (!bakerSubsystem)
    {
        bakerSubsystem = world->AddSubsystem<BakerSubsystem>();
    }

    bakerSubsystem->EnqueueBake(MakeStrongRef(this));
}

#endif

#pragma endregion Light

#pragma region DirectionalLight

DirectionalLight::DirectionalLight()
    : DirectionalLight(Vec3f(0.0f, 1.0f, 0.0f).Normalized(), Color::White(), 1.0f)
{
}

DirectionalLight::DirectionalLight(const Vec3f& direction, const Color& color, float intensity)
    : Light(LightType::Directional, direction.Normalized(), color, intensity, 0.0f)
{
    m_lightFlags |= LightFlags::CacheStaticShadowMaps;
    m_numShadowMapCascades = 2;
}


#pragma endregion DirectionalLight

#pragma region PointLight

PointLight::PointLight()
    : PointLight(Vec3f(0.0f), Color::White(), 5.0f, 25.0f)
{
}

PointLight::PointLight(const Vec3f& position, const Color& color, float intensity, float radius)
    : Light(LightType::Point, position, color, intensity, radius)
{
    m_lightFlags |= LightFlags::CacheStaticShadowMaps;
}

#pragma endregion PointLight

#pragma region SpotLight

SpotLight::SpotLight()
    : Light(LightType::Spot, Vec3f(0.0f), Vec3f(0.0f, 0.0f, -1.0f), Vec2f(30.0f, 15.0f), Color::White(), 1.0f, 10.0f)
{
}

SpotLight::SpotLight(const Vec3f& position, const Vec3f& direction, const Vec2f& angles, const Color& color, float intensity, float radius)
    : Light(LightType::Spot, position, direction, angles, color, intensity, radius)
{
}

#pragma endregion SpotLight

#pragma region AreaRectLight

AreaRectLight::AreaRectLight()
    : Light(LightType::AreaRect, Vec3f(0.0f), Vec3f(0.0f, 0.0f, -1.0f), Vec2f(1.0f, 1.0f), Color::White(), 1.0f, 10.0f)
{
}

AreaRectLight::AreaRectLight(const Vec3f& position, const Vec3f& normal, const Vec2f& areaSize, const Color& color, float intensity, float radius)
    : Light(LightType::AreaRect, position, normal, areaSize, color, intensity, radius)
{
}

#pragma endregion AreaRectLight

} // namespace Hyperion
