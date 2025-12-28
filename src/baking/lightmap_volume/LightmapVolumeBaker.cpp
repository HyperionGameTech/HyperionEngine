/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <baking/lightmap_volume/LightmapVolumeBaker.hpp>
#include <baking/lightmap_volume/LightmapVolumeBakeJob.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <scene/EntityManager.hpp>
#include <scene/LightmapVolume.hpp>

#include <scene/components/MeshComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/LightmapElementComponent.hpp>

#include <core/threading/TaskThread.hpp>

namespace Hyperion {
namespace Baking {

static constexpr LightmapElementId InvalidLightmapElementId = LightmapElementId(~0u);

Baker<LightmapVolume>::Baker(LightmapperConfig&& config, const Handle<LightmapVolume>& volume)
    : BakerBase(std::move(config), volume, MakeStrongRef(volume->GetScene()), volume->GetWorldBounds()),
      m_volume(volume),
      m_lightmapElementId(InvalidLightmapElementId)
{
}

UniquePtr<BakeJobBase> Baker<LightmapVolume>::CreateJob(BakeJobParams&& params)
{
    return MakeUnique<BakeJob<LightmapVolume>>(std::move(params), m_volume, &m_bakeData);
}

void Baker<LightmapVolume>::Initialize_Internal()
{
    // no-op
}

void Baker<LightmapVolume>::Build()
{
    HYP_SCOPE;

    EntityManager& mgr = *m_scene->GetEntityManager();

    m_subElements.Clear();
    m_subElementsByEntity.Clear();

    const bool onlyOverlappingElements = OnlyOverlappingElements();

    for (auto [entity, meshComponent, transformComponent, boundingBoxComponent, _] : mgr.GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent, TagComponent<EntityTag::MobStatic>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
    {
        if (entity->InstanceClass() != Entity::StaticClass())
        {
            continue;
        }

        if (!meshComponent.mesh || !meshComponent.material)
        {
            continue;
        }

        if (meshComponent.material->GetBucket() != RB_OPAQUE && meshComponent.material->GetBucket() != RB_TRANSLUCENT)
        {
            continue;
        }

        const BoundingBox& worldAabb = boundingBoxComponent.worldAabb;

        if (!onlyOverlappingElements && !m_aabb.Overlaps(worldAabb))
        {
            continue;
        }

        m_subElements.PushBack(LightmapSubElement {
            MakeStrongRef(entity),
            meshComponent.mesh,
            meshComponent.material,
            Transform(transformComponent.translation, transformComponent.scale, transformComponent.rotation).GetMatrix(),
            boundingBoxComponent.worldAabb });
    }

    // Build global data
    m_bakeData = BakeData<LightmapVolume>(m_subElements.ToSpan(), m_volume);

    if (Result result = m_bakeData.Build(); result.HasError())
    {
        HYP_LOG(Lightmap, Error, "Failed to build lightmap data: {}", result.GetError().GetMessage());
        return;
    }

    LightmapElement lightmapElement;
    if (!m_volume->AddElement({ m_bakeData.GetWidth(), m_bakeData.GetHeight() }, lightmapElement, /* shrinkToFit */ true, /* downscaleLimit */ 0.1f))
    {
        HYP_LOG(Lightmap, Error, "Failed to add element to volume!");
        return;
    }

    m_lightmapElementId = lightmapElement.id;
    AssertDebug(m_lightmapElementId != InvalidLightmapElementId);

    BakerBase::DispatchJobs();
}

void Baker<LightmapVolume>::HandleCompletedJob_Internal(BakeJobBase* job)
{
    HYP_SCOPE;

    if (m_numJobs == 1)
    {
        AssertDebug(m_lightmapElementId != InvalidLightmapElementId);

        if (!m_volume->BuildElementTextures(m_bakeData, m_lightmapElementId))
        {
            HYP_LOG(Lightmap, Error, "Failed to build LightmapElement textures for LightmapVolume, element id: {}", m_lightmapElementId);
            return;
        }

        const LightmapElement* lightmapElement = m_volume->GetElement(m_lightmapElementId);
        Assert(lightmapElement != nullptr);

        HYP_LOG(Lightmap, Debug, "Lightmap baking complete! Building element with id {}, UV offset: {}, Scale: {}", m_lightmapElementId,
            lightmapElement->offsetUv, lightmapElement->scale);

        // Update meshes
        for (SizeType subElementIndex = 0; subElementIndex < m_subElements.Size(); subElementIndex++)
        {
            LightmapSubElement& subElement = m_subElements[subElementIndex];

            auto updateMeshData = [&]()
            {
                const Handle<Mesh>& mesh = subElement.mesh;
                Assert(mesh.IsValid());

                Assert(subElementIndex < m_bakeData.GetMeshData().Size());

                const LightmapMeshData& lightmapMeshData = m_bakeData.GetMeshData()[subElementIndex];
                Assert(lightmapMeshData.mesh == mesh);

                MeshDesc newMeshDesc;
                newMeshDesc.meshAttributes = mesh->GetMeshAttributes();
                newMeshDesc.numVertices = uint32(lightmapMeshData.vertices.Size());
                newMeshDesc.numIndices = uint32(lightmapMeshData.indices.Size());

                MeshData newMeshData;
                newMeshData.vertexData = lightmapMeshData.vertices;
                newMeshData.indexData = ByteBuffer(lightmapMeshData.indices.ToByteView());

                for (SizeType i = 0; i < newMeshData.vertexData.Size(); i++)
                {
                    Vec2f& lightmapUv = newMeshData.vertexData[i].texcoord1;
                    lightmapUv.y = 1.0f - lightmapUv.y; // Invert Y coordinate for lightmaps
                    lightmapUv *= lightmapElement->scale;
                    lightmapUv += Vec2f(lightmapElement->offsetUv.x, lightmapElement->offsetUv.y);
                }

                mesh->SetMeshData(newMeshDesc, newMeshData);
            };

            updateMeshData();

            bool isNewMaterial = false;

            if (subElement.material)
            {
                Handle<Material> clonedMaterial = subElement.material->Clone();
                SafeDelete(std::move(subElement.material));

                subElement.material = clonedMaterial;
            }
            else
            {
                subElement.material = CreateObject<Material>();
            }

            isNewMaterial = true;

            subElement.material->SetBucket(RB_LIGHTMAP);

            subElement.material->SetTexture(MaterialTextureKey::IRRADIANCE_MAP, m_volume->GetAtlasTexture(lightmapElement->GetAtlasIndex(), LTT_IRRADIANCE));
            subElement.material->SetTexture(MaterialTextureKey::RADIANCE_MAP, m_volume->GetAtlasTexture(lightmapElement->GetAtlasIndex(), LTT_RADIANCE));

            auto updateMeshComponent = [entityManagerWeak = MakeWeakRef(m_scene->GetEntityManager()),
                                           lightmapElementId = m_lightmapElementId,
                                           volume = m_volume,
                                           subElement = subElement,
                                           newMaterial = (isNewMaterial ? subElement.material : Handle<Material>::empty)]()
            {
                Handle<EntityManager> entityManager = entityManagerWeak.Lock();

                if (!entityManager)
                {
                    return;
                }

                const Handle<Entity>& entity = subElement.entity;

                if (entityManager->HasComponent<MeshComponent>(entity))
                {
                    MeshComponent& meshComponent = entityManager->GetComponent<MeshComponent>(entity);

                    if (newMaterial.IsValid())
                    {
                        InitObject(newMaterial);

                        SafeDelete(std::move(meshComponent.material));

                        meshComponent.material = std::move(newMaterial);
                    }
                }
                else
                {
                    Assert(newMaterial.IsValid());
                    InitObject(newMaterial);

                    MeshComponent meshComponent {};
                    meshComponent.mesh = subElement.mesh;
                    meshComponent.material = newMaterial;

                    entityManager->AddComponent<MeshComponent>(entity, std::move(meshComponent));
                }

                if (entityManager->HasComponent<LightmapElementComponent>(entity))
                {
                    LightmapElementComponent& lightmapElementComponent = entityManager->GetComponent<LightmapElementComponent>(entity);

                    lightmapElementComponent.lightmapVolume = volume.ToWeak();
                    lightmapElementComponent.lightmapElementId = lightmapElementId;
                    lightmapElementComponent.lightmapVolumeUuid = volume->GetUUID();
                }
                else
                {
                    LightmapElementComponent lightmapElementComponent;

                    lightmapElementComponent.lightmapVolume = volume.ToWeak();
                    lightmapElementComponent.lightmapElementId = lightmapElementId;
                    lightmapElementComponent.lightmapVolumeUuid = volume->GetUUID();

                    entityManager->AddComponent<LightmapElementComponent>(entity, std::move(lightmapElementComponent));
                }

                entity->SetNeedsRenderProxyUpdate();
            };

            if (IsOnThread(m_scene->GetEntityManager()->GetOwnerThreadId()))
            {
                updateMeshComponent();
            }
            else
            {
                ThreadBase* thread = GetThreadById(m_scene->GetEntityManager()->GetOwnerThreadId());
                Assert(thread != nullptr);

                thread->GetScheduler().Enqueue(std::move(updateMeshComponent), TaskEnqueueFlags::FIRE_AND_FORGET);
            }
        }
    }
}

} // namespace Baking
} // namespace Hyperion
