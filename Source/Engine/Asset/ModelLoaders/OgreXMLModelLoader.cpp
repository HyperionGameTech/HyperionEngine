/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <AssetPch.hpp>

#include <Asset/ModelLoaders/OgreXMLModelLoader.hpp>
#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Scene/Node.hpp>
#include <Scene/World.hpp>
#include <Scene/Scene.hpp>
#include <Scene/DetachedScene.hpp>

#include <Scene/Animation/Skeleton.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/AnimationComponent.hpp>
#include <Scene/Components/TransformComponent.hpp>
#include <Scene/Components/VisibilityStateComponent.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Framework/EngineDriver.hpp>

#include <Util/XML/SAXParser.hpp>

#include <OgreXMLModelLoader.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Assets);

using FatVertex = TVertex<VT_Simple | VT_Skeletal>;

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

            const float x = StringUtil::Parse<float>(attributes.At("x"));
            const float y = StringUtil::Parse<float>(attributes.At("y"));
            const float z = StringUtil::Parse<float>(attributes.At("z"));

            m_model.positions.PushBack(Vec3f(x, y, z));
        }
        else if (name == "normal")
        {
            if (!attributes.Contains("x") || !attributes.Contains("y") || !attributes.Contains("z"))
            {
                return;
            }

            const float x = StringUtil::Parse<float>(attributes.At("x"));
            const float y = StringUtil::Parse<float>(attributes.At("y"));
            const float z = StringUtil::Parse<float>(attributes.At("z"));

            m_model.normals.PushBack(Vec3f(x, y, z));
        }
        else if (name == "texcoord")
        {
            if (!attributes.Contains("u") || !attributes.Contains("v"))
            {
                return;
            }

            const float x = StringUtil::Parse<float>(attributes.At("u"));
            const float y = StringUtil::Parse<float>(attributes.At("v"));

            m_model.texcoords.PushBack(Vec2f(x, y));
        }
        else if (name == "face")
        {
            if (attributes.Size() != 3)
            {
                HYP_LOG(Assets, Warning, "Ogre XML parser: `face` tag expected to have 3 attributes.");
            }

            TMap<String, uint32> faceElements;
            faceElements.Reserve(attributes.Size());

            for (const Pair<String, String>& it : attributes)
            {
                faceElements.Insert({ it.first, StringUtil::Parse<uint32>(it.second) });
            }

            for (const KeyValuePair<String, uint32>& it : faceElements)
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
            const uint32 vertexIndex = StringUtil::Parse<uint32>(attributes.At("vertexindex"));
            const uint32 boneIndex = StringUtil::Parse<uint32>(attributes.At("boneindex"));
            const float boneWeight = StringUtil::Parse<float>(attributes.At("weight"));

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

    Array<FatVertex> vertices;
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

        vertices[i].SetPosition(position);
        vertices[i].SetUV0(texcoord);
        vertices[i].SetNormal(normal);
        vertices[i].boneIndices = UINT32_MAX;
        Memory::Zero(vertices[i].boneWeights, sizeof(vertices[i].boneWeights));

        const auto boneIt = model.boneAssignments.Find(i);

        if (boneIt != model.boneAssignments.end())
        {
            auto& boneAssignments = boneIt->second;

            for (size_t j = 0; j < boneAssignments.Size(); j++)
            {
                if (j == 4)
                {
                    HYP_LOG(Assets, Warning, "Ogre XML parser: Attempt to add more than 4 bone assignments");

                    break;
                }

                const uint32 boneIndex = vertices[i].NumBoneIndices();
                AssertDebug(boneAssignments[j].index != TVertexPacket<VT_Skeletal>::InvalidBoneIndex);

                vertices[i].SetBoneIndex(boneIndex, uint8(boneAssignments[j].index));
                vertices[i].SetBoneWeight(boneIndex, boneAssignments[j].weight);
            }
        }
    }

    model.vertexData = Array<float>(reinterpret_cast<const float*>(vertices.Data()), vertices.ByteSize() / sizeof(float));
}

AssetLoadResult OgreXMLModelLoader::LoadAsset(LoaderState& state) const
{
    Assert(state.assetManager != nullptr);

    OgreXMLModel model;
    model.filepath = state.filepath;

    OgreXMLSAXHandler handler(&state, model);

    xml::SAXParser parser(&handler);
    auto saxResult = parser.Parse(state.stream);

    if (!saxResult)
    {
        return HYP_MAKE_ERROR(AssetLoadError, "XML error: {}", saxResult.GetError().GetMessage());
    }

    BuildVertices(model);

    Handle<Node> top = MakeHandle<Node>();

    Handle<Skeleton> skeleton;

    if (!model.skeletonName.Empty())
    {
        const String skeletonPath = StringUtil::BasePath(model.filepath) + "/" + model.skeletonName + ".xml";

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
            HYP_LOG(Assets, Verbose, "Ogre XML parser: Skipping submesh with empty indices");

            continue;
        }

        // Reverse triangle winding: convert right-handed CCW to left-handed CW
        for (uint32 i = 0; i + 2 < subMesh.indices.Size(); i += 3)
        {
            std::swap(subMesh.indices[i + 1], subMesh.indices[i + 2]);
        }

        Scene* scene = GetDetachedSceneForCurrentThread();

        const Handle<Entity> entity = scene->GetEntityManager()->AddEntity();

        Name assetName = CreateNameFromDynamicString(subMesh.name);

        AssertDebug(model.vertexData.ByteSize() % sizeof(FatVertex) == 0);

        MeshDesc meshDesc;
        meshDesc.meshAttributes.inputLayout = { VT_Simple | VT_Skeletal };
        meshDesc.meshAttributes.indexBufferElemType = GET_UNSIGNED_INT;
        meshDesc.meshAttributes.topology = TOP_TRIANGLES;
        meshDesc.numVertices = uint32(model.vertexData.ByteSize() / sizeof(FatVertex));
        meshDesc.numIndices = uint32(subMesh.indices.Size());

        Handle<Mesh> mesh = MakeHandle<Mesh>();
        mesh->SetName(assetName);

        VertexArrayView vertexArrayView {};
        vertexArrayView.floatData = model.vertexData.Data();
        vertexArrayView.vertexCount = meshDesc.numVertices;
        vertexArrayView.layoutDesc = meshDesc.meshAttributes.inputLayout;

        mesh->SetMeshData(meshDesc, vertexArrayView, subMesh.indices.ToByteView());
        // mesh->SetOriginalFilepath(FilePath::Relative(state.filepath, state.assetManager->GetBasePath()));

        GetCurrentAssetRegistry()->PutAsset(mesh);

        MaterialAttributes attributes {};
        attributes.bucket = RenderBucket::Translucent;
        attributes.blendFunction = BlendFunction::AlphaBlending();
        attributes.shaderName = NAME("GeometryPass");
        attributes.shaderProperties = {};

        MaterialParameters parameters;
        parameters.albedo = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
        parameters.transmission = 0.9f;
        parameters.roughness = 0.2f;

        Handle<Material> material = MakeHandle<Material>(CreateNameFromDynamicString(ANSIString(subMesh.name.Data())), attributes, parameters, MaterialTextures {});
        GetCurrentAssetRegistry()->PutAsset(material);

        InitObject(material);

        entity->SetLocalBounds(mesh->GetAABB());

        scene->GetEntityManager()->AddComponent<MeshComponent>(entity, MeshComponent { mesh, material, skeleton });

        Handle<Node> node = MakeHandle<Node>();
        node->SetName(CreateNameFromDynamicString(subMesh.name));
        node->AddChild(entity);

        if (skeleton.IsValid())
        {
            entity->SetIsDynamic(true);

            AnimationComponent animationComponent {};
            animationComponent.playbackState = {
                .animationIndex = 0,
                .status = AnimationPlaybackStatus::PLAYING,
                .loopMode = AnimationLoopMode::REPEAT,
                .speed = 1.0f,
                .currentTime = 0.0f
            };

            scene->GetEntityManager()->AddComponent<AnimationComponent>(entity, animationComponent);
        }

        top->AddChild(std::move(node));
    }

    return LoadedAsset { top };
}

} // namespace Hyperion
