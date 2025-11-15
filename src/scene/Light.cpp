/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <scene/Light.hpp>
#include <scene/View.hpp>
#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/EntityTag.hpp>
#include <scene/EntityManager.hpp>

#include <scene/camera/Camera.hpp>
#include <scene/camera/OrthoCamera.hpp>
#include <scene/camera/PerspectiveCamera.hpp>

#include <shadows/ShadowMap.hpp>
#include <shadows/ShadowMapAllocator.hpp>
#include <shadows/ShadowCameraHelper.hpp>

#include <rendering/Material.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderGlobalState.hpp>

#include <rendering/renderers/ShadowRenderer.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <core/threading/Threads.hpp>

// for EnumToString
#include <core/reflection/Enum.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/utilities/Float16.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

#include <Light.generated.inl>

namespace hyperion {

static constexpr TextureFormat PointLightShadowFormat = TF_RG16F;
static constexpr TextureFormat DirectionalLightShadowFormats[SMF_MAX] = {
    TF_RGBA8, // STANDARD
    TF_RGBA8, // PCF
    TF_RGBA8, // CONTACT_HARDENING
    TF_RG16F  // VSM
};

static const ShaderProperty s_shadowMapFilterProperties[SMF_MAX] = {
    ShaderProperty(NAME("MODE"), NAME("STANDARD")),
    ShaderProperty(NAME("MODE"), NAME("PCF")),
    ShaderProperty(NAME("MODE"), NAME("CONTACT_HARDENED")),
    ShaderProperty(NAME("MODE"), NAME("VSM"))
};

static constexpr EnumFlags<ViewFlags> DefaultShadowViewFlags = ViewFlags::SKIP_LIGHTS
    | ViewFlags::SKIP_LIGHTMAP_VOLUMES | ViewFlags::SKIP_ENV_PROBES | ViewFlags::SKIP_ENV_GRIDS
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
    m_entityInitInfo.initialTags = { EntityTag::LIGHT };
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
    m_entityInitInfo.initialTags = { EntityTag::LIGHT };
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

    ShaderDefinition shaderDefinition;

    ShaderProperties shaderProperties;
    shaderProperties.SetRequiredVertexAttributes(staticMeshVertexAttributes);
    shaderProperties.Set(s_shadowMapFilterProperties[shadowMapFilter]);

    ViewOutputTargetDesc outputTargetDesc {};
    outputTargetDesc.extent = m_shadowMapDimensions;

    switch (m_type)
    {
    case LT_POINT:
    {
        // Frustum culling for cubemap views not currently supported.
        shadowViewFlags[0] |= ViewFlags::NO_FRUSTUM_CULLING;

        outputTargetDesc.numViews = 6;

        // depth, depth^2 texture (for variance shadow map)
        ViewOutputTargetAttachmentDesc& attachmentDesc = outputTargetDesc.attachments.EmplaceBack();
        attachmentDesc.format = PointLightShadowFormat;
        attachmentDesc.imageType = TT_CUBEMAP;
        attachmentDesc.loadOp = LoadOperation::CLEAR;
        attachmentDesc.storeOp = StoreOperation::STORE;
        attachmentDesc.clearColor = Vec4f(10000.0f);

        ViewOutputTargetAttachmentDesc& depthAttachmentDesc = outputTargetDesc.attachments.EmplaceBack();
        depthAttachmentDesc.format = g_renderBackend->GetDefaultFormat(DIF_DEPTH);
        depthAttachmentDesc.imageType = TT_CUBEMAP;
        depthAttachmentDesc.loadOp = LoadOperation::CLEAR;
        depthAttachmentDesc.storeOp = StoreOperation::STORE;

        shaderProperties.Set(NAME("MODE_SHADOWS"));
        shaderDefinition = ShaderDefinition(NAME("RenderToCubemap"), shaderProperties);

        break;
    }
    case LT_DIRECTIONAL:
    {
        // For directional lights, we have one for static objects and one for dynamic objects
        shadowViewFlags = {
            DefaultShadowViewFlags | ViewFlags::COLLECT_STATIC_ENTITIES,
            DefaultShadowViewFlags | ViewFlags::COLLECT_DYNAMIC_ENTITIES
        };

        // depth, depth^2 texture (for variance shadow map)
        ViewOutputTargetAttachmentDesc& attachmentDesc = outputTargetDesc.attachments.EmplaceBack();
        attachmentDesc.format = DirectionalLightShadowFormats[shadowMapFilter];
        attachmentDesc.imageType = TT_TEX2D;
        attachmentDesc.loadOp = LoadOperation::CLEAR;
        attachmentDesc.storeOp = StoreOperation::STORE;
        attachmentDesc.clearColor = Vec4f(10000.0f);

        ViewOutputTargetAttachmentDesc& depthAttachmentDesc = outputTargetDesc.attachments.EmplaceBack();
        depthAttachmentDesc.format = g_renderBackend->GetDefaultFormat(DIF_DEPTH);
        depthAttachmentDesc.imageType = TT_TEX2D;
        depthAttachmentDesc.loadOp = LoadOperation::CLEAR;
        depthAttachmentDesc.storeOp = StoreOperation::STORE;

        shaderDefinition = ShaderDefinition(NAME("Shadows"), shaderProperties);

        break;
    }
    default:
    {
        // no shadow mapping implementation
        return;
    }
    }

    AssertDebug(shaderDefinition.IsValid(), "Shader definition is not valid for light type {}", EnumToString(m_type));

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
        shadowMapCamera = CreateObject<Camera>(int(m_shadowMapDimensions.x), int(m_shadowMapDimensions.y));
        shadowMapCamera->SetName(NAME_FMT("ShadowMapCamera_{}", Id())); // @FIXME Use name instead, ID is not persistent

        switch (m_type)
        {
        case LT_DIRECTIONAL:
            shadowMapCamera->AddCameraController(CreateObject<OrthoCameraController>());
            break;
        case LT_POINT:
            shadowMapCamera->SetFOV(90.0f);
            shadowMapCamera->SetNear(0.1f);
            shadowMapCamera->SetFar(250.0f);

            shadowMapCamera->AddCameraController(CreateObject<PerspectiveCameraController>());

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
            .shaderDefinition = shaderDefinition,
            .cullFaces = shadowMapFilter == SMF_VSM ? FCM_BACK : FCM_FRONT });

    for (int i = 0; i < int(shadowViewFlags.Size()); i++)
    {
        ViewDesc viewDesc {
            .flags = shadowViewFlags[i] | DefaultShadowViewFlags,
            .viewport = Viewport { .extent = m_shadowMapDimensions, .position = Vec2i::Zero() },
            .outputTargetDesc = outputTargetDesc,
            .scenes = {},
            .camera = shadowMapCamera,
            .overrideAttributes = overrideAttributes
        };

        m_shadowViews[i] = CreateObject<View>(viewDesc);

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
                85.0f, /// TODO: add proper radius for directional light.
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

void Light::OnTransformUpdated(const Transform& transform)
{
    HYP_SCOPE;

    Entity::OnTransformUpdated(transform);

    m_position = transform.GetTranslation();

    if (m_type == LT_DIRECTIONAL)
    {
        m_position.Normalize();
    }

    if (IsInitCalled())
    {
        SetNeedsRenderProxyUpdate();
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

    if (IsInitCalled())
    {
        SetNeedsRenderProxyUpdate();
    }
}

void Light::SetNormal(const Vec3f& normal)
{
    if (m_normal == normal)
    {
        return;
    }

    m_normal = normal;

    if (IsInitCalled())
    {
        SetNeedsRenderProxyUpdate();
    }
}

void Light::SetAreaSize(const Vec2f& areaSize)
{
    if (m_areaSize == areaSize)
    {
        return;
    }

    m_areaSize = areaSize;

    if (IsInitCalled())
    {
        SetNeedsRenderProxyUpdate();
    }
}

void Light::SetColor(const Color& color)
{
    if (m_color == color)
    {
        return;
    }

    m_color = color;

    if (IsInitCalled())
    {
        SetNeedsRenderProxyUpdate();
    }
}

void Light::SetIntensity(float intensity)
{
    if (m_intensity == intensity)
    {
        return;
    }

    m_intensity = intensity;

    if (IsInitCalled())
    {
        SetNeedsRenderProxyUpdate();
    }
}

void Light::SetRadius(float radius)
{
    if (m_radius == radius)
    {
        return;
    }

    m_radius = radius;

    if (IsInitCalled())
    {
        SetNeedsRenderProxyUpdate();
    }
}

void Light::SetFalloff(float falloff)
{
    if (m_falloff == falloff)
    {
        return;
    }

    m_falloff = falloff;

    if (IsInitCalled())
    {
        SetNeedsRenderProxyUpdate();
    }
}

void Light::SetSpotAngles(const Vec2f& spotAngles)
{
    if (m_spotAngles == spotAngles)
    {
        return;
    }

    m_spotAngles = spotAngles;

    if (IsInitCalled())
    {
        SetNeedsRenderProxyUpdate();
    }
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

    if (IsInitCalled())
    {
        InitObject(m_material);

        SetNeedsRenderProxyUpdate();
    }
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

    if (IsInitCalled())
    {
        SetNeedsRenderProxyUpdate();
    }
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

    if (IsInitCalled())
    {
        SetNeedsRenderProxyUpdate();
    }
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
            .Union(m_worldTransform.GetTranslation() + m_normal * m_radius);
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
    bufferData.normal = Vec4f(m_normal, 0.0f);
    bufferData.materialIndex = ~0u; // materialIndex gets set in WriteBufferData_Light()
    bufferData.flags = m_flags;

    switch (GetLightType())
    {
    case LT_AREA_RECT:
        bufferData.areaSize = m_areaSize;
        break;
    case LT_SPOT:
        bufferData.areaSize = m_spotAngles;
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

} // namespace hyperion
