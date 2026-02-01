/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ScenePch.hpp>

#include <scene/Light.hpp>
#include <scene/View.hpp>
#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/EntityTag.hpp>
#include <scene/EntityManager.hpp>

#include <scene/camera/Camera.hpp>
#include <scene/camera/OrthoCamera.hpp>
#include <scene/camera/PerspectiveCamera.hpp>

#include <rendering/shadows/ShadowMap.hpp>
#include <rendering/shadows/ShadowMapAllocator.hpp>
#include <rendering/shadows/ShadowCameraHelper.hpp>

#include <rendering/Material.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/RenderInterface.hpp>

#include <rendering/renderers/ShadowRenderer.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <core/threading/Threads.hpp>

// for EnumToString
#include <core/reflection/Enum.hpp>

#include <core/utilities/Float16.hpp>

#include <engine/EngineDriver.hpp>

#include <Light.generated.inl>

namespace Hyperion {

static constexpr TextureFormat PointLightShadowFormat = TF_RG16F;
static constexpr TextureFormat DirectionalLightShadowFormats[SMF_MAX] = {
    TF_RGBA8, // STANDARD
    TF_RGBA8, // PCF
    TF_RGBA8, // CONTACT_HARDENING
    TF_RG16F  // VSM
};

static const ShaderPropertyId s_shadowMapFilterProperties[SMF_MAX] = {
    InternShaderProperty(ShaderProperty(NAME("MODE"), NAME("STANDARD"))),
    InternShaderProperty(ShaderProperty(NAME("MODE"), NAME("PCF"))),
    InternShaderProperty(ShaderProperty(NAME("MODE"), NAME("CONTACT_HARDENED"))),
    InternShaderProperty(ShaderProperty(NAME("MODE"), NAME("VSM")))
};

static const ShaderPropertyId s_propModeShadows = InternShaderProperty(ShaderProperty(NAME("MODE_SHADOWS")));

static constexpr EnumFlags<ViewFlags> DefaultShadowViewFlags = ViewFlags::SKIP_LIGHTS
    | ViewFlags::SKIP_LIGHTMAP_VOLUMES | ViewFlags::SKIP_PARTICLE_VOLUMES | ViewFlags::SKIP_FOG_VOLUMES
    | ViewFlags::SKIP_ENV_PROBES | ViewFlags::SKIP_ENV_GRIDS
    | ViewFlags::SKIP_CAMERAS;

static constexpr Vec2u DefaultShadowMapDimensions[LT_MAX] = {
    Vec2u(1024, 1024), // LT_DIRECTIONAL
    Vec2u(256, 256),   // LT_POINT
    Vec2u(256, 256),   // LT_SPOT
    Vec2u(256, 256)    // LT_AREA_RECT
};

#pragma region Light

Light::Light()
    : Light(LT_DIRECTIONAL, Vec3f::Zero(), Color::White(), 1.0f, 1.0f)
{
}

Light::Light(LightType type, const Vec3f& position, const Color& color, float intensity, float radius)
    : m_type(type),
      m_flags(LF_DEFAULT),
      m_position(position),
      m_color(color),
      m_intensity(intensity),
      m_radius(MathUtil::Max(radius, 0.001f)),
      m_falloff(1.0f),
      m_spotAngles(Vec2f::Zero()),
      m_shadowMapDimensions(DefaultShadowMapDimensions[type])
{
    m_entityInitInfo.canEverUpdate = true;
    m_entityInitInfo.receivesUpdate = true;
    m_entityInitInfo.bvhDepth = 0; // No BVH for lights
    m_entityInitInfo.initialTags = { EntityTag::Light };
}

Light::Light(LightType type, const Vec3f& position, const Vec3f& normal, const Vec2f& areaSize, const Color& color, float intensity, float radius)
    : m_type(type),
      m_flags(LF_DEFAULT),
      m_position(position),
      m_normal(normal),
      m_areaSize(areaSize),
      m_color(color),
      m_intensity(intensity),
      m_radius(MathUtil::Max(radius, 0.001f)),
      m_falloff(1.0f),
      m_spotAngles(Vec2f::Zero()),
      m_shadowMapDimensions(DefaultShadowMapDimensions[type])
{
    m_entityInitInfo.canEverUpdate = true;
    m_entityInitInfo.receivesUpdate = true;
    m_entityInitInfo.bvhDepth = 0; // No BVH for lights
    m_entityInitInfo.initialTags = { EntityTag::Light };
}

Light::~Light()
{
    if (m_shadowViews.Any())
    {
        SafeDelete(std::move(m_shadowViews));
    }

    if (m_material != nullptr)
    {
        SafeDelete(std::move(m_material));
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

    if (m_flags & LF_SHADOW)
    {
        CreateShadowViews();
        UpdateShadowViews();
    }

    SetReady(true);
}

void Light::CreateShadowViews()
{
    HYP_SCOPE;

    for (Handle<View>& shadowView : m_shadowViews)
    {
        if (!shadowView.IsValid())
        {
            continue;
        }

        const Handle<Camera>& shadowCamera = shadowView->GetCamera();

        if (!shadowCamera.IsValid())
        {
            continue;
        }

        RemoveChild(shadowCamera);
    }

    SafeDelete(std::move(m_shadowViews));

    if (!(m_flags & LF_SHADOW))
    {
        return;
    }

    const ShadowMapFilter shadowMapFilter = GetShadowMapFilter();
    AssertDebug(shadowMapFilter < std::size(s_shadowMapFilterProperties));

    // Per shadow view flags
    Array<EnumFlags<ViewFlags>> shadowViewFlags = {
        ViewFlags::COLLECT_ALL_ENTITIES
    };

    ShaderDesc shaderDesc;
    shaderDesc.name = NAME("DrawShadowMap");
    shaderDesc.properties.Add(s_shadowMapFilterProperties[shadowMapFilter]);

    RenderTargetDesc renderTargetDesc {};
    renderTargetDesc.extent = m_shadowMapDimensions;

    switch (m_type)
    {
    case LT_POINT:
    {
        // Frustum culling for cubemap views not currently supported.
        shadowViewFlags[0] |= ViewFlags::NO_FRUSTUM_CULLING;

        renderTargetDesc.numAttachments = 0;
        renderTargetDesc.numLayers = 6;

        // depth, depth^2 texture (for variance shadow map)
        AttachmentDesc& moments = renderTargetDesc.attachments[renderTargetDesc.numAttachments++];
        moments.imageType = TT_CUBEMAP;
        moments.format = PointLightShadowFormat;
        moments.loadOp = LoadOperation::CLEAR;
        moments.storeOp = StoreOperation::STORE;
        std::fill(std::begin(moments.clearColor), std::end(moments.clearColor), 1000.0f);

        AttachmentDesc& depth = renderTargetDesc.attachments[renderTargetDesc.numAttachments++];
        depth.imageType = TT_CUBEMAP;
        depth.format = TF_DEPTH_32F;
        depth.loadOp = LoadOperation::CLEAR;
        depth.storeOp = StoreOperation::STORE;

        shaderDesc.name = NAME("DrawCubemap");

        shaderDesc.properties = {};
        shaderDesc.properties.Add(s_propModeShadows);

        break;
    }
    case LT_DIRECTIONAL:
    {
        // For directional lights, we have one for static objects and one for dynamic objects
        shadowViewFlags = {
            DefaultShadowViewFlags | ViewFlags::COLLECT_STATIC_ENTITIES,
            DefaultShadowViewFlags | ViewFlags::COLLECT_DYNAMIC_ENTITIES
        };

        renderTargetDesc.numAttachments = 0;

        // depth, depth^2 texture (for variance shadow map)
        AttachmentDesc& moments = renderTargetDesc.attachments[renderTargetDesc.numAttachments++];
        moments.format = DirectionalLightShadowFormats[shadowMapFilter];
        moments.imageType = TT_TEX2D;
        moments.loadOp = LoadOperation::CLEAR;
        moments.storeOp = StoreOperation::STORE;
        std::fill(std::begin(moments.clearColor), std::end(moments.clearColor), 1000.0f);

        AttachmentDesc& depth = renderTargetDesc.attachments[renderTargetDesc.numAttachments++];
        depth.format = TF_DEPTH_32F;
        depth.imageType = TT_TEX2D;
        depth.loadOp = LoadOperation::CLEAR;
        depth.storeOp = StoreOperation::STORE;

        break;
    }
    default:
        // no shadow mapping implementation
        return;
    }

    Handle<Camera> shadowMapCamera;

    // Check existing immediate children for Camera instances (used for deserialization)
    auto shadowMapCameraIt = FindIf(m_childNodes, [](const Handle<Node>& node)
        {
            return node->IsA<Camera>();
        });

    if (shadowMapCameraIt != m_childNodes.End())
    {
        shadowMapCamera = ObjCast<Camera>(*shadowMapCameraIt);
    }

    if (!shadowMapCamera)
    {
        shadowMapCamera = MakeHandle<Camera>(int(m_shadowMapDimensions.x), int(m_shadowMapDimensions.y));
        shadowMapCamera->SetName(NAME_FMT("ShadowMapCamera_{}", GetName()));

        switch (m_type)
        {
        case LT_DIRECTIONAL:
            shadowMapCamera->AddCameraController(MakeHandle<OrthoCameraController>());
            break;
        case LT_POINT:
            shadowMapCamera->SetFOV(90.0f);
            shadowMapCamera->SetNear(0.01f);
            shadowMapCamera->SetFar(m_radius);

            shadowMapCamera->AddCameraController(MakeHandle<PerspectiveCameraController>());

            shadowMapCamera->SetDirection(Vec3f(0.0f, 0.0f, 1.0f));
            break;
        default:
            break;
        }

        InitObject(shadowMapCamera);
        AddChild(shadowMapCamera);
    }

    AssertDebug(shadowViewFlags.Size() >= 1);
    m_shadowViews.Resize(shadowViewFlags.Size());

    const RenderableAttributeSet overrideAttributes(
        MeshAttributes {},
        MaterialAttributes {
            .shaderName = shaderDesc.name,
            .shaderProperties = shaderDesc.properties,
            .cullFaces = shadowMapFilter == SMF_VSM ? FCM_FRONT : FCM_BACK
        });

    for (int i = 0; i < int(shadowViewFlags.Size()); i++)
    {
        ViewDesc viewDesc {
            .flags = shadowViewFlags[i] | DefaultShadowViewFlags,
            .viewport = Viewport { .extent = m_shadowMapDimensions, .position = Vec2i::Zero() },
            .renderTargetDesc = renderTargetDesc,
            .scenes = {},
            .camera = shadowMapCamera,
            .overrideAttributes = overrideAttributes
        };

        m_shadowViews[i] = MakeHandle<View>(viewDesc);

        if (Scene* scene = GetScene())
        {
            m_shadowViews[i]->AddScene(MakeStrongRef(scene));
        }

        InitObject(m_shadowViews[i]);
    }
}

void Light::UpdateShadowViews()
{
    HYP_SCOPE;

    for (int i = 0; i < int(m_shadowViews.Size()); i++)
    {
        const Handle<View>& shadowView = m_shadowViews[i];
        AssertDebug(shadowView != nullptr);

        const Handle<Camera>& shadowCamera = shadowView->GetCamera();
        AssertDebug(shadowCamera != nullptr);

        switch (m_type)
        {
        case LT_DIRECTIONAL:
            ShadowCameraHelper::UpdateShadowCameraDirectional(
                shadowCamera,
                Vec3f::Zero(), // TODO: Center around camera
                GetPosition(),
                45.0f, /// TODO: add proper radius for directional light.
                m_shadowAabb);

            break;
        case LT_POINT:
            m_shadowAabb = GetAABB();

            shadowCamera->SetTranslation(m_position);

            break;
        default:
            HYP_LOG(Scene, Warning, "Shadow view update not implemented for light type {}", EnumToString(m_type));
            break;
        }
    }
}

void Light::OnAttachedToNode(Node* node)
{
    HYP_SCOPE;

    Entity::OnAttachedToNode(node);
}

void Light::OnDetachedFromNode(Node* node)
{
    HYP_SCOPE;

    Entity::OnDetachedFromNode(node);
}

void Light::OnAddedToScene(Scene* scene)
{
    HYP_SCOPE;

    Entity::OnAddedToScene(scene);

    if (m_flags & LF_SHADOW)
    {
        for (const Handle<View>& shadowView : m_shadowViews)
        {
            AssertDebug(shadowView != nullptr);

            if (!shadowView)
            {
                continue;
            }

            shadowView->AddScene(MakeStrongRef(scene));
        }
    }
}

void Light::OnRemovedFromScene(Scene* scene)
{
    HYP_SCOPE;

    Entity::OnRemovedFromScene(scene);

    if (m_flags & LF_SHADOW)
    {
        for (const Handle<View>& shadowView : m_shadowViews)
        {
            AssertDebug(shadowView != nullptr);

            if (!shadowView)
            {
                continue;
            }

            shadowView->RemoveScene(scene);
        }
    }
}

void Light::OnTransformUpdated()
{
    HYP_SCOPE;

    Entity::OnTransformUpdated();

    m_position = GetWorldTranslation();

    if (m_type == LT_DIRECTIONAL)
    {
        m_position.Normalize();
    }

    UpdateShadowViews();
}

void Light::Update(float delta)
{
    HYP_SCOPE;

    if (m_flags & LF_SHADOW)
    {
        for (int i = 0; i < int(m_shadowViews.Size()); i++)
        {
            GetWorld()->ProcessViewAsync(m_shadowViews[i]);
        }

        SetNeedsRenderProxyUpdate();
    }
}

void Light::SetPosition(const Vec3f& position)
{
    HYP_SCOPE;

    if (m_position == position)
    {
        return;
    }

    m_position = position;

    SetNeedsRenderProxyUpdate();
}

void Light::SetNormal(const Vec3f& normal)
{
    if (m_normal == normal)
    {
        return;
    }

    m_normal = normal;

    SetNeedsRenderProxyUpdate();
}

void Light::SetAreaSize(const Vec2f& areaSize)
{
    if (m_areaSize == areaSize)
    {
        return;
    }

    m_areaSize = areaSize;

    SetNeedsRenderProxyUpdate();
}

void Light::SetColor(const Color& color)
{
    if (m_color == color)
    {
        return;
    }

    m_color = color;

    SetNeedsRenderProxyUpdate();
}

void Light::SetIntensity(float intensity)
{
    if (m_intensity == intensity)
    {
        return;
    }

    m_intensity = intensity;

    SetNeedsRenderProxyUpdate();
}

void Light::SetRadius(float radius)
{
    if (m_radius == radius)
    {
        return;
    }

    m_radius = radius;

    SetNeedsRenderProxyUpdate();
}

void Light::SetFalloff(float falloff)
{
    if (m_falloff == falloff)
    {
        return;
    }

    m_falloff = falloff;

    SetNeedsRenderProxyUpdate();
}

void Light::SetSpotAngles(const Vec2f& spotAngles)
{
    if (m_spotAngles == spotAngles)
    {
        return;
    }

    m_spotAngles = spotAngles;

    SetNeedsRenderProxyUpdate();
}

void Light::SetMaterial(Handle<Material> material)
{
    if (material == m_material)
    {
        return;
    }

    if (m_material)
    {
        SafeDelete(std::move(m_material));
    }

    m_material = std::move(material);
    InitObject(m_material);

    SetNeedsRenderProxyUpdate();
}

Pair<Vec3f, Vec3f> Light::CalculateAreaLightRect() const
{
    HYP_SCOPE;

    Vec3f tangent;
    Vec3f bitangent;
    MathUtil::ComputeOrthonormalBasis(m_normal, tangent, bitangent);

    const float halfWidth = m_areaSize.x * 0.5f;
    const float halfHeight = m_areaSize.y * 0.5f;

    const Vec3f center = m_position;

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

    m_shadowMapDimensions = shadowMapDimensions;

    SetNeedsRenderProxyUpdate();
}

void Light::SetShadowMapFilter(ShadowMapFilter shadowMapFilter)
{
    if (shadowMapFilter == GetShadowMapFilter())
    {
        return;
    }

    m_flags &= ~LF_SHADOW_FILTER_MASK;

    // ShadowMapFilter enum members are sequentially ordered so turn it into a flag
    m_flags |= EnumFlags<LightFlags>(1u << shadowMapFilter);

    SetNeedsRenderProxyUpdate();
}

BoundingBox Light::GetAABB() const
{
    HYP_SCOPE;

    if (m_type == LT_DIRECTIONAL)
    {
        return BoundingBox::Infinity();
    }

    if (m_type == LT_AREA_RECT)
    {
        const Pair<Vec3f, Vec3f> rect = CalculateAreaLightRect();

        return BoundingBox::Empty()
            .Union(rect.first)
            .Union(rect.second)
            .Union(GetWorldTranslation() + m_normal * m_radius);
    }

    if (m_type == LT_POINT)
    {
        return BoundingBox(GetBoundingSphere());
    }

    return BoundingBox::Empty();
}

BoundingSphere Light::GetBoundingSphere() const
{
    HYP_SCOPE;

    if (m_type == LT_DIRECTIONAL)
    {
        return BoundingSphere::infinity;
    }

    return BoundingSphere(m_position, m_radius);
}

void Light::UpdateRenderProxy(RenderProxyLight* proxy)
{
    HYP_SCOPE;

    proxy->light = WeakHandleFromThis();
    proxy->lightMaterial = m_material.Get();

    proxy->shadowViews.Resize(m_shadowViews.Size());
    for (SizeType i = 0; i < m_shadowViews.Size(); i++)
    {
        proxy->shadowViews[i] = m_shadowViews[i].Get();
    }

    const BoundingBox aabb = GetAABB();

    LightShaderData& bufferData = proxy->bufferData;
    bufferData.lightType = uint32(m_type);
    bufferData.color = Vec4f(m_color);
    bufferData.radiusFalloffPacked = (uint32(Float16(m_falloff).Raw()) << 16) | Float16(m_radius).Raw();
    bufferData.positionIntensity = Vec4f(m_position, m_intensity);
    bufferData.materialIndex = ~0u; // materialIndex gets set in WriteBufferData_Light()
    bufferData.flags = m_flags;

    switch (GetLightType())
    {
    case LT_AREA_RECT:
        bufferData.areaSize = m_areaSize;
        bufferData.areaNormal[0] = m_normal.x;
        bufferData.areaNormal[1] = m_normal.y;
        bufferData.areaNormal[2] = m_normal.z;
        break;
    case LT_SPOT:
        bufferData.areaSize = m_spotAngles;
        bufferData.spotLightDir[0] = m_normal.x;
        bufferData.spotLightDir[1] = m_normal.y;
        bufferData.spotLightDir[2] = m_normal.z;
        break;
    case LT_POINT:
        break;
    default:
        break;
    }

    if (m_shadowViews.Any())
    {
        bufferData.shadowMatrix = m_shadowViews[0]->GetCamera()->GetViewProjectionMatrix();
        bufferData.aabbMin = Vec4f(m_shadowAabb.min, 1.0f);
        bufferData.aabbMax = Vec4f(m_shadowAabb.max, 1.0f);
    }
    else
    {
        bufferData.shadowMatrix = Mat4f::Identity();
        bufferData.aabbMin = MathUtil::MaxSafeValue<Vec4f>();
        bufferData.aabbMax = MathUtil::MinSafeValue<Vec4f>();
    }
}

#pragma endregion Light

} // namespace Hyperion
