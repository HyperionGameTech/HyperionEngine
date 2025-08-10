/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/model_loaders/OgreXMLModelLoader.hpp>

#include <scene/World.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>

#include <scene/animation/Skeleton.hpp>

#include <scene/EntityManager.hpp>
#include <scene/components/MeshComponent.hpp>
#include <scene/components/AnimationComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/VisibilityStateComponent.hpp>

#include <core/logging/Logger.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

#include <util/xml/SAXParser.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

using OgreXMLModel = OgreXMLModelLoader::OgreXMLModel;
using BoneAssignment = OgreXMLModelLoader::OgreXMLModel::BoneAssignment;
using SubMesh = OgreXMLModelLoader::OgreXMLModel::SubMesh;

class OgreXMLSAXHandler : public xml::SAXHandler
{
public:
    OgreXMLSAXHandler(LoaderState* state, OgreXMLModel& object)
        : m_model(object)
    {
    }

    SubMesh& LastSubMesh()
    {
        if (m_model.submeshes.Empty())
        {
            m_model.submeshes.PushBack({});
        }

        return m_model.submeshes.Back();
    }

    void AddBoneAssignment(uint32 vertexIndex, BoneAssignment&& boneAssignment)
    {
        m_model.boneAssignments[vertexIndex].PushBack(std::move(boneAssignment));
    }

    virtual void Begin(const String& name, const xml::AttributeMap& attributes) override
    {
        if (name == "position")
        {
            if (!attributes.Contains("x") || !attributes.Contains("y") || !attributes.Contains("z"))
            {
                return;
            }

            const auto x = StringUtil::Parse<float>(attributes.At("x"));
            const auto y = StringUtil::Parse<float>(attributes.At("y"));
            const auto z = StringUtil::Parse<float>(attributes.At("z"));

            m_model.positions.PushBack(Vec3f(x, y, z));
        }
        else if (name == "normal")
        {
            if (!attributes.Contains("x") || !attributes.Contains("y") || !attributes.Contains("z"))
            {
                return;
            }

            const auto x = StringUtil::Parse<float>(attributes.At("x"));
            const auto y = StringUtil::Parse<float>(attributes.At("y"));
            const auto z = StringUtil::Parse<float>(attributes.At("z"));

            m_model.normals.PushBack(Vec3f(x, y, z));
        }
        else if (name == "texcoord")
        {
            if (!attributes.Contains("u") || !attributes.Contains("v"))
            {
                return;
            }

            const auto x = StringUtil::Parse<float>(attributes.At("u"));
            const auto y = StringUtil::Parse<float>(attributes.At("v"));

            m_model.texcoords.PushBack(Vec2f(x, y));
        }
        else if (name == "face")
        {
            if (attributes.Size() != 3)
            {
                HYP_LOG(Assets, Warning, "Ogre XML parser: `face` tag expected to have 3 attributes.");
            }

            FlatMap<String, uint32> faceElements;
            faceElements.Reserve(attributes.Size());

            for (const Pair<String, String>& it : attributes)
            {
                faceElements.Insert({ it.first, StringUtil::Parse<uint32>(it.second) });
            }

            for (const Pair<String, uint32>& it : faceElements)
            {
                LastSubMesh().indices.PushBack(it.second);
            }
        }
        else if (name == "skeletonlink")
        {
            m_model.skeletonName = attributes.At("name");
        }
        else if (name == "vertexboneassignment")
        {
            const auto vertexIndex = StringUtil::Parse<uint32>(attributes.At("vertexindex"));
            const auto boneIndex = StringUtil::Parse<uint32>(attributes.At("boneindex"));
            const auto boneWeight = StringUtil::Parse<float>(attributes.At("weight"));

            AddBoneAssignment(vertexIndex, { boneIndex, boneWeight });
        }
        else if (name == "submesh")
        {
            String name = String("submesh_") + String::ToString(m_model.submeshes.Size());

            if (auto nameIt = attributes.Find("material"); nameIt != attributes.End())
            {
                name = nameIt->second;
            }

            m_model.submeshes.PushBack({ name,
                Array<uint32> {} });
        }
        else if (name == "vertex")
        {
            /* no-op */
        }
        else
        {
            HYP_LOG(Assets, Warning, "Ogre XML parser: No handler for '{}' tag", name);
        }
    }

    virtual void End(const String& name) override
    {
    }

    virtual void Characters(const String& value) override
    {
    }

    virtual void Comment(const String& comment) override
    {
    }

private:
    OgreXMLModel& m_model;
};

void BuildVertices(OgreXMLModel& model)
{
    const bool hasNormals = !model.normals.Empty(),
               hasTexcoords = !model.texcoords.Empty();

    Array<Vertex> vertices;
    vertices.Resize(model.positions.Size());

    for (uint32 i = 0; i < vertices.Size(); i++)
    {
        Vec3f position;
        Vec3f normal;
        Vec2f texcoord;

        if (i < model.positions.Size())
        {
            position = model.positions[i];
        }
        else
        {
            HYP_LOG(Assets, Warning, "Ogre XML parser: Vertex index ({}) out of bounds ({})", i, model.positions.Size());

            continue;
        }

        if (hasNormals)
        {
            if (i < model.normals.Size())
            {
                normal = model.normals[i];
            }
            else
            {
                HYP_LOG(Assets, Warning, "Ogre XML parser: Normal index ({}) out of bounds ({})", i, model.normals.Size());
            }
        }

        if (hasTexcoords)
        {
            if (i < model.texcoords.Size())
            {
                texcoord = model.texcoords[i];
            }
            else
            {
                HYP_LOG(Assets, Warning, "Ogre XML parser: Texcoord index ({}) out of bounds ({})", i, model.texcoords.Size());
            }
        }

        vertices[i] = Vertex(position, texcoord, normal);

        const auto boneIt = model.boneAssignments.Find(i);

        if (boneIt != model.boneAssignments.end())
        {
            auto& boneAssignments = boneIt->second;

            for (SizeType j = 0; j < boneAssignments.Size(); j++)
            {
                if (j == 4)
                {
                    HYP_LOG(Assets, Warning, "Ogre XML parser: Attempt to add more than 4 bone assignments");

                    break;
                }

                vertices[i].AddBoneIndex(boneAssignments[j].index);
                vertices[i].AddBoneWeight(boneAssignments[j].weight);
            }
        }
    }

    model.vertices = std::move(vertices);
}

AssetLoadResult OgreXMLModelLoader::LoadAsset(LoaderState& state) const
{
    Assert(state.assetManager != nullptr);

    OgreXMLModel model;
    model.filepath = state.filepath;

    OgreXMLSAXHandler handler(&state, model);

    xml::SAXParser parser(&handler);
    auto saxResult = parser.Parse(&state.stream);

    if (!saxResult)
    {
        return HYP_MAKE_ERROR(AssetLoadError, "XML error: {}", saxResult.message);
    }

    BuildVertices(model);

    Handle<Node> top = CreateObject<Node>();

    Handle<Skeleton> skeleton;

    if (!model.skeletonName.Empty())
    {
        const String skeletonPath((StringUtil::BasePath(model.filepath.Data()).c_str() + String("/") + model.skeletonName + ".xml").Data());

        auto skeletonAsset = state.assetManager->Load<Skeleton>(skeletonPath);

        if (skeletonAsset.HasValue())
        {
            skeleton = skeletonAsset->Result();
        }
        else
        {
            HYP_LOG(Assets, Warning, "Ogre XML parser: Could not load skeleton at {}", skeletonPath);
        }
    }

    for (SubMesh& subMesh : model.submeshes)
    {
        if (subMesh.indices.Empty())
        {
            HYP_LOG(Assets, Info, "Ogre XML parser: Skipping submesh with empty indices");

            continue;
        }

        Handle<Scene> scene = g_engineDriver->GetDefaultWorld()->GetDetachedScene(Threads::CurrentThreadId());

        const Handle<Entity> entity = scene->GetEntityManager()->AddEntity();

        scene->GetEntityManager()->AddComponent<TransformComponent>(entity, TransformComponent {});
        scene->GetEntityManager()->AddComponent<VisibilityStateComponent>(entity, VisibilityStateComponent {});

        Name assetName = CreateNameFromDynamicString(subMesh.name);

        MeshData meshData;
        meshData.desc.numVertices = uint32(model.vertices.Size());
        meshData.desc.numIndices = uint32(subMesh.indices.Size());
        meshData.desc.meshAttributes.vertexAttributes = staticMeshVertexAttributes;

        if (skeleton.IsValid())
        {
            meshData.desc.meshAttributes.vertexAttributes |= skeletonVertexAttributes;
        }

        meshData.vertexData = model.vertices;
        meshData.indexData.SetSize(subMesh.indices.Size() * sizeof(uint32));
        meshData.indexData.Write(subMesh.indices.Size() * sizeof(uint32), 0, subMesh.indices.Data());

        meshData.CalculateNormals();

        Handle<Mesh> mesh = CreateObject<Mesh>();
        mesh->SetName(assetName);
        mesh->SetMeshData(meshData);

        mesh->GetAsset()->Rename(assetName);
        mesh->GetAsset()->SetOriginalFilepath(FilePath::Relative(state.filepath, state.assetManager->GetBasePath()));

        state.assetManager->GetAssetRegistry()->RegisterAsset("$Import/Media/Meshes", mesh->GetAsset());

        MaterialAttributes materialAttributes {};
        materialAttributes.shaderDefinition = ShaderDefinition {
            NAME("Forward"),
            ShaderProperties(mesh->GetVertexAttributes())
        };

        Handle<Material> material = CreateObject<Material>(CreateNameFromDynamicString(ANSIString(subMesh.name.Data())), materialAttributes);

        scene->GetEntityManager()->AddComponent<MeshComponent>(entity, MeshComponent { mesh, material, skeleton });
        scene->GetEntityManager()->AddComponent<BoundingBoxComponent>(entity, BoundingBoxComponent { mesh->GetAABB() });

        entity->SetName(CreateNameFromDynamicString(subMesh.name));

        if (skeleton.IsValid())
        {
            AnimationComponent animationComponent {};
            animationComponent.playbackState = {
                .animationIndex = 0,
                .status = AnimationPlaybackStatus::PLAYING,
                .loopMode = AnimationLoopMode::REPEAT,
                .speed = 1.0f,
                .currentTime = 0.0f
            };

            scene->GetEntityManager()->AddComponent<AnimationComponent>(entity, animationComponent);

            scene->GetEntityManager()->RemoveTag<EntityTag::STATIC>(entity);
            scene->GetEntityManager()->AddTag<EntityTag::DYNAMIC>(entity);
        }

        top->AddChild(entity);
    }

    return LoadedAsset { top };
}

} // namespace hyperion
