/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/ShadowMapCaptureState.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/Framebuffer.hpp>
#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/ShaderPropertyDictionary.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <Scene/Light/Light.hpp>
#include <Scene/Scene.hpp>

#include <Scene/Camera/Camera.hpp>
#include <Scene/Camera/PerspectiveCamera.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Framework/CVarManager.hpp>
#include <Framework/EngineGlobals.hpp>
#include <Framework/View.hpp>

namespace Hyperion {

extern CVar<float> g_cvShadowDepthBias;
extern CVar<float> g_cvShadowDepthBiasDirectional;

static StaticShaderPropertyId s_propModeShadows { ShaderProperty(NAME("MODE_SHADOWS")) };

ShadowMapCaptureState::ShadowMapCaptureState(Light* light, Name swatchName)
    : swatchName(swatchName),
      m_light(light),
      m_numFaces(1)
{
}

ShadowMapCaptureState::~ShadowMapCaptureState()
{
    if (m_light && m_light->GetShadowMapCaptureState() == this)
    {
        m_light->m_shadowMapCaptureState = nullptr;
    }
}

void ShadowMapCaptureState::Begin()
{
    Assert(m_light != nullptr);

    if (!m_light)
    {
        return;
    }

    Assert(m_light->GetShadowMapCaptureState() == nullptr,
        "Light {} is already capturing a shadow map", m_light->Id());

    const Vec2u dimensions = m_light->GetShadowMapDimensions();
    AssertDebug(dimensions.Volume() > 0);

    const bool isOmni = m_light->GetLightType() == LightType::Point;
    m_numFaces = isOmni ? 6 : 1;

    m_texture = MakeHandle<Texture>(TextureDesc {
        isOmni ? TextureType::Cubemap : TextureType::Texture2D,
        TextureFormat::D16,
        Vec3u { dimensions, 1 },
        TextureFilterMode::Nearest,
        TextureFilterMode::Nearest,
        TextureWrapMode::ClampToEdge,
        1,
        ImageUsage::Sampled | ImageUsage::Attachment });

    m_texture->SetName(NAME_FMT("{}_BakedShadowMap", m_light->GetName()));
    m_texture->SetIsTransient(true);
    Check(m_texture->Create());

    if (EngineGlobals::IsHeadless())
    {
        return;
    }

    const float farClip = isOmni
        ? MathUtil::Max(m_light->GetRadius(), 1.0f)
        : 1000.0f;

    m_camera = MakeHandle<Camera>(int(dimensions.x), int(dimensions.y));
    m_camera->SetName(NAME_FMT("{}_ShadowBakeCamera", m_light->GetName()));
    m_camera->SetFOV(90.0f);
    m_camera->SetNearClip(0.01f);
    m_camera->SetFarClip(farClip);
    m_camera->AddCameraController(MakeHandle<PerspectiveCameraController>());

    InitObject(m_camera);

    m_camera->SetWorldTranslation(m_light->GetWorldTranslation());

    const Mat4f projMat = m_camera->GetProjectionMatrix();

    MaterialAttributes materialAttributes;
    materialAttributes.shaderName = isOmni ? NAME("DrawCubemap") : NAME("DrawShadowMap");

    if (isOmni)
    {
        materialAttributes.shaderProperties.Add(s_propModeShadows);
    }

    const bool isDirectional = m_light->GetLightType() == LightType::Directional;
    const float depthBias = isDirectional ? g_cvShadowDepthBiasDirectional.Get() : g_cvShadowDepthBias.Get();
    const float depthRange = farClip - m_camera->GetNearClip();

    materialAttributes.flags = MAF_DEPTH_WRITE | MAF_DEPTH_TEST | MAF_DEPTH_BIAS | MAF_DEPTH_CLAMP;
    materialAttributes.depthBias = int32(MathUtil::Round(depthBias * depthRange));
    materialAttributes.depthBiasSlope = 2.0f;
    materialAttributes.cullFaces = FaceCullMode::Back;

    Scene* scene = m_light->GetScene();

    for (uint32 faceIndex = 0; faceIndex < m_numFaces; faceIndex++)
    {
        FramebufferDesc framebufferDesc {};
        framebufferDesc.extent = dimensions;
        framebufferDesc.numAttachments = 1;
        framebufferDesc.attachments[0] = AttachmentDesc {
            isOmni ? TextureType::Cubemap : TextureType::Texture2D,
            TextureFormat::D16,
            LoadOperation::Clear,
            StoreOperation::Store
        };

        FramebufferRef framebuffer = RI.MakeFramebuffer(framebufferDesc);
        Assert(framebuffer.IsValid());

        GpuImageViewRef imageView = RI.MakeImageView(
            m_texture->GetGpuImage(),
            0,
            1,
            faceIndex,
            1,
            TextureType::Texture2D);
        Check(imageView->Create());

        framebuffer->AddAttachment(0, framebufferDesc.attachments[0], imageView);
        Check(framebuffer->Create());

        m_framebuffers[faceIndex] = std::move(framebuffer);

        ViewDesc viewDesc {};
        viewDesc.flags = ViewFlags::SHADOW_VIEW
            | ViewFlags::COLLECT_STATIC_ENTITIES
            | ViewFlags::SKIP_LIGHTS | ViewFlags::SKIP_CAMERAS
            | ViewFlags::SKIP_LIGHTMAP_VOLUMES | ViewFlags::SKIP_PARTICLE_VOLUMES | ViewFlags::SKIP_FOG_VOLUMES
            | ViewFlags::SKIP_ENV_PROBES
            | ViewFlags::NO_SHADOW_VIEWS
            | ViewFlags::EXTERNAL_RENDERTARGET
            | ViewFlags::NO_PARALLEL_DRAW_CALL_COLLECTION
            | ViewFlags::NOT_MULTI_BUFFERED
            | ViewFlags::NO_ASYNC_SHADER_LOADING;

        if (isOmni)
        {
            viewDesc.flags |= ViewFlags::CUBEMAP_FACE_VIEW;
            viewDesc.viewIndex = uint8(faceIndex);
        }

        viewDesc.scenes = { scene };
        viewDesc.camera = m_camera.Get();
        viewDesc.overrideAttributes = RenderableAttributeSet(
            MeshAttributes {},
            materialAttributes);

        Handle<View> view = MakeHandle<View>(viewDesc);
        view->SetName(NAME_FMT("{}_ShadowBakeView_{}", m_light->GetName(), faceIndex));
        InitObject(view);

        const Vec3f origin = m_camera->GetWorldTranslation();

        Mat4f viewMat;

        if (isOmni)
        {
            viewMat = Mat4f::LookAt(Texture::s_cubemapDirections[faceIndex].first, Texture::s_cubemapDirections[faceIndex].second)
                * Mat4f::Translation(-origin);
        }
        else
        {
            const Vec3f forward = m_light->GetNormal().Normalized();

            Vec3f up = Vec3f::UnitY();

            if (MathUtil::Abs(forward.Dot(up)) > 0.999f)
            {
                up = Vec3f::UnitX();
            }

            viewMat = Mat4f::LookAt(origin, origin + forward, up);
        }

        view->cachedMatrices.view = viewMat;
        view->cachedMatrices.viewProj = projMat * viewMat;
        view->cachedMatrices.invProj = projMat.Inverse();

        view->cachedFrustum.SetFromViewProjectionMatrix(view->cachedMatrices.viewProj);

        m_views[faceIndex] = std::move(view);
    }

    m_light->m_shadowMapCaptureState = this;
}

void ShadowMapCaptureState::End(bool commitResult)
{
    Assert(m_light != nullptr);

    if (!m_light)
    {
        return;
    }

    if (m_light->GetShadowMapCaptureState() == this)
    {
        m_light->m_shadowMapCaptureState = nullptr;
    }

    // teardown capture data
    for (uint32 faceIndex = 0; faceIndex < m_numFaces; faceIndex++)
    {
        if (Handle<View>& view = m_views[faceIndex]; view.IsValid())
        {
            if (Scene* scene = m_light->GetScene())
            {
                view->RemoveScene(scene);
            }

            view->SetCamera(nullptr);

            EnqueueDeletion(std::move(view));
        }

        if (FramebufferRef& framebuffer = m_framebuffers[faceIndex]; framebuffer.IsValid())
        {
            EnqueueDeletion(std::move(framebuffer));
        }
    }

    m_views = {};
    m_framebuffers = {};

    if (m_camera.IsValid())
    {
        EnqueueDeletion(std::move(m_camera));
    }

    if (!commitResult)
    {
        // abandon ship
        return;
    }

    if (m_texture.IsValid())
    {
        m_texture->SetIsTransient(false);

        auto readbackCallback = [](Texture* texture, GpuBuffer& buffer) mutable
        {
            Assert(texture != nullptr);

            auto writeScope = texture->GetWriteScope();

            TextureDesc desc = texture->GetTextureDesc();
            AssertDebug(desc.extent.Volume() != 0);

            MemoryByteWriter<DynamicAllocator> stream;

            const uint8* inData = reinterpret_cast<const uint8*>(buffer.Map());

            desc.mipOffsets = {};

            const uint8 numMips = desc.NumMips();
            const uint16 numLayers = desc.NumArrayLayers();

            size_t mipOffset = 0;

            for (uint8 mipIndex = 0; mipIndex < numMips; mipIndex++)
            {
                const size_t mipOffsetBefore = mipOffset;
                const size_t mipByteSize = desc.GetMipByteSize(mipIndex, /* includeArrayLayers */ false);

                for (uint16 layerIndex = 0; layerIndex < numLayers; layerIndex++)
                {
                    stream.Write(inData, mipByteSize);

                    inData += mipByteSize;

                    mipOffset += mipByteSize;

                    stream.Seek(mipOffset);
                }

                if (mipIndex > 0)
                {
                    desc.mipOffsets[mipIndex - 1] = uint32(mipOffsetBefore);
                }
            }

            texture->SetTextureDesc(desc);
            texture->SetImageData(stream.GetBuffer().ToByteView());

            writeScope.Reset();
        };

        if (IsOnThread(g_renderThread))
        {
            m_texture->EnqueueReadback(
                [texture = m_texture, cb = std::move(readbackCallback)](GpuBuffer& buffer)
                {
                    cb(texture.Get(), buffer);
                });
        }
        else
        {
            GetThreadById(g_renderThread)->GetScheduler().Enqueue(
                [texture = m_texture, callback = std::move(readbackCallback)]() mutable
                {
                    // thunk it LOUDER
                    texture->EnqueueReadback(
                        [texture, cb = std::move(callback)](GpuBuffer& buffer)
                        {
                            cb(texture.Get(), buffer);
                        });
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);
        }

        GetCurrentAssetRegistry()->PutAssetUnique(m_texture);

        m_light->SetBakedShadowMap(m_texture);
    }
}

} // namespace Hyperion
