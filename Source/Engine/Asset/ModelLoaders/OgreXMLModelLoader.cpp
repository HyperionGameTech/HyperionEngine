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
#include <Scene/Prefab.hpp>
#include <Scene/DetachedScene.hpp>

#include <Scene/Animation/Skeleton.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/AnimationComponent.hpp>
#include <Scene/Components/TransformComponent.hpp>
#include <Scene/Components/VisibilityStateComponent.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>
#include <Rendering/Texture.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Framework/EngineDriver.hpp>

#include <Core/IO/ByteReader.hpp>

#include <Util/XML/SAXParser.hpp>

#include <OgreXMLModelLoader.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Assets);

using FatVertex = TVertex<VT_Simple | VT_Skeletal>;

using OgreXMLModel = OgreXMLModelLoader::OgreXMLModel;
using BoneAssignment = OgreXMLModelLoader::OgreXMLModel::BoneAssignment;
using SubMesh = OgreXMLModelLoader::OgreXMLModel::SubMesh;

struct OgreMaterialScript
{
    Vec4f diffuse = Vec4f::One();
    Array<String> textureNames;
    bool isAlphaBlended = false;
};

using OgreMaterialScriptMap = FlatMap<String, OgreMaterialScript>;

static void ParseOgreMaterialScript(const FilePath& filepath, OgreMaterialScriptMap& outMaterials)
{
    FileByteReader reader { filepath };

    if (reader.Eof())
    {
        return;
    }

    const String fileContents = String(reader.Read().ToByteView());

    OgreMaterialScript* currentMaterial = nullptr;

    for (const String& line : fileContents.Split('\n'))
    {
        const String trimmedLine = line.Trimmed();

        if (trimmedLine.Empty() || trimmedLine.StartsWith("//"))
        {
            continue;
        }

        Array<String> tokens;

        for (String& token : trimmedLine.Split(' ', '\t', '\r'))
        {
            if (token.Any())
            {
                tokens.PushBack(std::move(token));
            }
        }

        if (tokens.Empty())
        {
            continue;
        }

        if (tokens[0] == "material" && tokens.Size() >= 2)
        {
            currentMaterial = &outMaterials[tokens[1]];
        }
        else if (currentMaterial == nullptr)
        {
            continue;
        }
        else if (tokens[0] == "diffuse" && tokens.Size() >= 4)
        {
            currentMaterial->diffuse = Vec4f(
                StringUtil::Parse<float>(tokens[1]),
                StringUtil::Parse<float>(tokens[2]),
                StringUtil::Parse<float>(tokens[3]),
                1.0f);
        }
        else if (tokens[0] == "texture" && tokens.Size() >= 2)
        {
            currentMaterial->textureNames.PushBack(tokens[1]);
        }
        else if (tokens[0] == "scene_blend" && tokens.Size() >= 2 && tokens[1] == "alpha_blend")
        {
            currentMaterial->isAlphaBlended = true;
        }
    }
}

static String MakeSafeAssetName(const String& name)
{
    return name.ReplaceAll(":", "_");
}

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

            // Use flatmap to ensure sorting of keys
            FlatMap<String, uint32> faceElements;
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
            ANSIString name;

            // Take material name if present
            if (auto nameIt = attributes.Find("material"); nameIt != attributes.End())
            {
                name = nameIt->second;
            }

            if (name.Empty())
            {
                // Else falls back to numbered name
                name = ANSIString("submesh_") + ANSIString::ToString(m_model.submeshes.Size());
            }

            auto& sm = m_model.submeshes.EmplaceBack();
            sm.name = Name(name.ToAnsi());
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
        // Ogre texture coordinates have their origin at the top left
        vertices[i].SetUV0(Vec2f(texcoord.x, 1.0f - texcoord.y));
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

    OgreXMLModel model {};

    OgreXMLSAXHandler handler(&state, model);

    xml::SAXParser parser(&handler);
    auto saxResult = parser.Parse(state.stream);

    if (!saxResult)
    {
        return HYP_MAKE_ERROR(AssetLoadError, "XML error: {}", saxResult.GetError().GetMessage());
    }

    BuildVertices(model);

    Handle<Node> top = MakeHandle<Node>(Name(ANSIString(StringUtil::Basename(state.filepath.StripExtension()))));

    Handle<Skeleton> skeleton;

    if (!model.skeletonName.Empty())
    {
        const FilePath skeletonPath = ResolveReferencedFilepath(state.filepath, model.skeletonName + ".xml");

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

    OgreMaterialScriptMap materialScripts;
    Array<String> parsedMaterialScriptFiles;

    // Exporters write either one .material file per material (':' replaced with '_') or one for the whole mesh
    auto FindMaterialScript = [&](const String& materialName) -> const OgreMaterialScript*
    {
        const String candidateFilenames[] = {
            MakeSafeAssetName(materialName) + ".material",
            String(StringUtil::StripExtension(StringUtil::Basename(state.filepath.StripExtension()))) + ".material"
        };

        for (const String& candidateFilename : candidateFilenames)
        {
            if (auto it = materialScripts.Find(materialName); it != materialScripts.End())
            {
                return &it->second;
            }

            if (parsedMaterialScriptFiles.Contains(candidateFilename))
            {
                continue;
            }

            parsedMaterialScriptFiles.PushBack(candidateFilename);

            const FilePath materialScriptPath = ResolveReferencedFilepath(state.filepath, candidateFilename);

            if (materialScriptPath.Exists())
            {
                ParseOgreMaterialScript(materialScriptPath, materialScripts);
            }
        }

        if (auto it = materialScripts.Find(materialName); it != materialScripts.End())
        {
            return &it->second;
        }

        return nullptr;
    };

    FlatMap<String, Handle<Texture>> loadedTextures;

    auto LoadTexture = [&](const String& textureName) -> Handle<Texture>
    {
        if (auto it = loadedTextures.Find(textureName); it != loadedTextures.End())
        {
            return it->second;
        }

        Handle<Texture> texture;

        const FilePath texturePath = ResolveReferencedFilepath(state.filepath, textureName);

        if (auto textureResult = state.assetManager->Load<Texture>(texturePath, state.batchIdentifier, state.hint | AssetLoadHint::TextureSRGB);
            textureResult.HasValue())
        {
            texture = textureResult->Result();

            TextureDesc textureDesc = texture->GetTextureDesc();
            textureDesc.filterModeMin = TextureFilterMode::LinearMipmap;
            textureDesc.filterModeMag = TextureFilterMode::Linear;
            textureDesc.wrapMode = TextureWrapMode::Repeat;
            texture->SetTextureDesc(textureDesc);

            texture->SetName(CreateNameFromDynamicString(ANSIString(StringUtil::Basename(texturePath.StripExtension()))));

            GetCurrentAssetRegistry()->PutAssetUnique(texture);
        }
        else
        {
            HYP_LOG(Assets, Warning, "Ogre XML parser: Could not load texture at {}", texturePath);
        }

        loadedTextures.Set(textureName, texture);

        return texture;
    };

    for (SubMesh& subMesh : model.submeshes)
    {
        if (subMesh.indices.Empty())
        {
            HYP_LOG(Assets, Verbose, "Ogre XML parser: Skipping submesh with empty indices");

            continue;
        }

        const uint32 vertexCount = uint32(model.positions.Size());

        if (subMesh.indices.FindIf([vertexCount](uint32 index) { return index >= vertexCount; }) != subMesh.indices.End())
        {
            HYP_LOG(Assets, Warning, "Ogre XML parser: Skipping submesh '{}' with face indices out of range of {} vertices", subMesh.name, vertexCount);

            continue;
        }

        // Reverse triangle winding: convert right-handed CCW to left-handed CW
        for (uint32 i = 0; i + 2 < subMesh.indices.Size(); i += 3)
        {
            std::swap(subMesh.indices[i + 1], subMesh.indices[i + 2]);
        }

        Scene& scene = GetDetachedSceneForCurrentThread();

        const Handle<Entity> entity = scene.GetEntityManager()->AddEntity();

        const String materialScriptName(subMesh.name.LookupString());
        const Name assetName = CreateNameFromDynamicString(ANSIString(MakeSafeAssetName(materialScriptName)));

        AssertDebug(model.vertexData.ByteSize() % sizeof(FatVertex) == 0);

        MeshDesc meshDesc;
        meshDesc.meshAttributes.inputLayout = { VT_Simple | VT_Skeletal };
        meshDesc.meshAttributes.indexBufferElemType = GpuElemType::UnsignedInt;
        meshDesc.meshAttributes.topology = Topology::Triangles;
        meshDesc.lods[0].numVertices = uint32(model.vertexData.ByteSize() / sizeof(FatVertex));
        meshDesc.lods[0].numIndices = uint32(subMesh.indices.Size());

        Handle<Mesh> mesh = MakeHandle<Mesh>();
        mesh->SetName(assetName);

        VertexArrayView vertexArrayView {};
        vertexArrayView.floatData = model.vertexData.Data();
        vertexArrayView.vertexCount = meshDesc.lods[0].numVertices;
        vertexArrayView.layoutDesc = meshDesc.meshAttributes.inputLayout;

        MeshDataView meshData {};
        meshData.vertices[0] = vertexArrayView;
        meshData.indices[0] = subMesh.indices.ToByteView();

        mesh->SetMeshData(meshDesc, meshData);
        // mesh->SetOriginalFilepath(FilePath::Relative(state.filepath, state.assetManager->GetBasePath()));

        GetCurrentAssetRegistry()->PutAsset(mesh);

        const OgreMaterialScript* materialScript = FindMaterialScript(materialScriptName);

        MaterialAttributes attributes {};
        attributes.bucket = RenderBucket::Opaque;
        attributes.shaderName = NAME("GeometryPass");
        attributes.shaderProperties = {};

        MaterialParameters parameters;
        parameters.albedo = materialScript != nullptr ? materialScript->diffuse : Vec4f::One();
        parameters.metalness = 0.0f;
        parameters.roughness = 0.65f;

        MaterialTextures textures;

        if (materialScript != nullptr)
        {
            if (materialScript->isAlphaBlended)
            {
                attributes.bucket = RenderBucket::Translucent;
                attributes.blendFunction = BlendFunction::AlphaBlending();
            }

            // Only the diffuse map is used; this loader doesn't generate the tangents a normal map needs
            if (materialScript->textureNames.Any())
            {
                if (Handle<Texture> diffuseTexture = LoadTexture(materialScript->textureNames[0]))
                {
                    textures[MaterialTextureKey::Diffuse] = std::move(diffuseTexture);
                }
            }
        }
        else
        {
            HYP_LOG(Assets, Warning, "Ogre XML parser: No material script found for '{}', using a default material", materialScriptName);
        }

        Handle<Material> material = MakeHandle<Material>(assetName, attributes, parameters, textures);
        GetCurrentAssetRegistry()->PutAsset(material);

        InitObject(material);

        entity->SetLocalBounds(mesh->GetAABB());

        scene.GetEntityManager()->AddComponent<MeshComponent>(entity, MeshComponent { mesh, material, skeleton });

        Handle<Node> node = MakeHandle<Node>();
        node->SetName(assetName);
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

            scene.GetEntityManager()->AddComponent<AnimationComponent>(entity, animationComponent);
        }

        top->AddChild(std::move(node));
    }

    return LoadedAsset { MakeHandle<Prefab>(top->GetName(), top) };
}

} // namespace Hyperion
