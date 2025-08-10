/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/model_loaders/OBJModelLoader.hpp>

#include <rendering/Mesh.hpp>
#include <scene/World.hpp>
#include <rendering/Material.hpp>
#include <scene/Node.hpp>

#include <scene/EntityManager.hpp>
#include <scene/components/MeshComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/VisibilityStateComponent.hpp>

#include <core/containers/HashMap.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/Logger.hpp>

#include <core/filesystem/FsUtil.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

constexpr bool createObjIndices = true;
constexpr bool meshPerMaterial = true; // set true to create a new mesh on each instance of 'use <mtllib>'
constexpr bool loadMaterials = true;

using Tokens = Array<String>;

using OBJModel = OBJModelLoader::OBJModel;
using OBJMesh = OBJModel::OBJMesh;
using OBJIndex = OBJModel::OBJIndex;

template <class Vector>
static Vector ReadVector(const Tokens& tokens, SizeType offset = 1)
{
    Vector result { 0.0f };

    int valueIndex = 0;

    for (SizeType i = offset; i < tokens.Size(); i++)
    {
        const String& token = tokens[i];

        if (token.Empty())
        {
            continue;
        }

        result.values[valueIndex++] = static_cast<typename Vector::Type>(std::atof(token.Data()));

        if (valueIndex == std::size(result.values))
        {
            break;
        }
    }

    return result;
}

static void AddMesh(OBJModel& model, const String& name, const String& material)
{
    String uniqueName(name);
    int counter = 0;

    while (AnyOf(model.meshes, [&uniqueName](const OBJMesh& objMesh)
        {
            return objMesh.name == uniqueName;
        }))
    {
        uniqueName = name + String::ToString(++counter);
    }

    model.meshes.PushBack(OBJMesh {
        .name = uniqueName,
        .material = material });
}

static OBJMesh& LastMesh(OBJModel& model)
{
    if (model.meshes.Empty())
    {
        AddMesh(model, "default", "default");
    }

    return model.meshes.Back();
}

static OBJIndex ParseOBJIndex(const String& token)
{
    OBJIndex objIndex { 0, 0, 0 };
    SizeType tokenIndex = 0;

    const Array<String> parts = token.Split('/');

    for (const String& part : parts)
    {
        if (!part.Empty())
        {
            switch (tokenIndex)
            {
            case 0:
                objIndex.vertex = StringUtil::Parse<int64>(part.Data()) - 1;
                break;
            case 1:
                objIndex.texcoord = StringUtil::Parse<int64>(part.Data()) - 1;
                break;
            case 2:
                objIndex.normal = StringUtil::Parse<int64>(part.Data()) - 1;
                break;
            default:
                // ??
                break;
            }
        }

        ++tokenIndex;
    }

    return objIndex;
}

template <class Vector>
Vector GetIndexedVertexProperty(int64 vertexIndex, const Array<Vector>& vectors)
{
    const int64 vertexAbsolute = vertexIndex >= 0
        ? vertexIndex
        : int64(vectors.Size()) + vertexIndex;

    if (vertexAbsolute < 0 || vertexAbsolute >= int64(vectors.Size()))
    {
        HYP_LOG(Assets, Warning, "Vertex index of {} (absolute: {}) is out of bounds ({})",
            vertexIndex, vertexAbsolute, vectors.Size());

        return Vector();
    }

    return vectors[vertexAbsolute];
}

OBJModel OBJModelLoader::LoadModel(LoaderState& state)
{
    OBJModel model;
    model.filepath = state.filepath;

    Tokens tokens;
    tokens.Reserve(5);

    String activeMaterial;

    state.stream.ReadLines([&](const String& line, bool*)
        {
            tokens.Clear();

            const String trimmed = line.Trimmed();

            if (trimmed.Empty() || trimmed.Front() == '#')
            {
                return;
            }

            tokens = trimmed.Split(' ');

            if (tokens.Empty())
            {
                return;
            }

            if (tokens[0] == "v")
            {
                model.positions.PushBack(ReadVector<Vec3f>(tokens, 1));

                return;
            }

            if (tokens[0] == "vn")
            {
                model.normals.PushBack(ReadVector<Vec3f>(tokens, 1));

                return;
            }

            if (tokens[0] == "vt")
            {
                model.texcoords.PushBack(ReadVector<Vec2f>(tokens, 1));

                return;
            }

            if (tokens[0] == "f")
            {
                OBJMesh& lastMesh = LastMesh(model);

                /* we don't support per-face material so we compromise by setting the mesh's material
                 * to the last 'usemtl' value when we hit the face command.
                 */
                if (!activeMaterial.Empty())
                {
                    lastMesh.material = activeMaterial;
                }

                if (tokens.Size() > 5)
                {
                    HYP_LOG(Assets, Warning, "Faces with more than 4 vertices are not supported by the OBJ model loader");
                }

                /* Performs simple triangulation on quad faces */
                for (int64 i = 0; i < int64(tokens.Size()) - 3; i++)
                {
                    lastMesh.indices.PushBack(ParseOBJIndex(tokens[1]));
                    lastMesh.indices.PushBack(ParseOBJIndex(tokens[2 + i]));
                    lastMesh.indices.PushBack(ParseOBJIndex(tokens[3 + i]));
                }

                return;
            }

            if (tokens[0] == "o")
            {
                if (tokens.Size() != 1)
                {
                    model.name = tokens[1];
                }

                return;
            }

            if (tokens[0] == "s")
            {
                /* smooth shading; ignore */
                return;
            }

            if (tokens[0] == "mtllib")
            {
                if (tokens.Size() != 1)
                {
                    String materialLibraryName;

                    for (SizeType i = 1; i < tokens.Size(); i++)
                    {
                        if (i != 1)
                        {
                            materialLibraryName += ' ';
                        }

                        materialLibraryName += tokens[i];
                    }

                    model.materialLibrary = materialLibraryName;
                }

                return;
            }

            if (tokens[0] == "g")
            {
                String name = "default";

                if (tokens.Size() != 1)
                {
                    name = tokens[1];
                }

                AddMesh(model, name, activeMaterial);

                return;
            }

            if (tokens[0] == "usemtl")
            {
                if (tokens.Size() == 1)
                {
                    HYP_LOG(Assets, Warning, "Cannot set obj model material -- no material provided");

                    return;
                }

                activeMaterial = tokens[1];

                if constexpr (meshPerMaterial)
                {
                    AddMesh(model, activeMaterial, activeMaterial);
                }

                return;
            }

            HYP_LOG(Assets, Warning, "Unable to parse obj model line: {}", trimmed);
        });

    return model;
}

LoadedAsset OBJModelLoader::BuildModel(LoaderState& state, OBJModel& model)
{
    Assert(state.assetManager != nullptr);

    Handle<Node> top = CreateObject<Node>(CreateNameFromDynamicString(model.name));

    Handle<MaterialGroup> materialLibrary;

    if (loadMaterials && !model.materialLibrary.Empty())
    {
        String materialLibraryPath = String(FileSystem::RelativePath(
            (String(StringUtil::BasePath(model.filepath.Data()).c_str()) + "/" + model.materialLibrary).Data(),
            FileSystem::CurrentPath())
                .c_str());

        if (!materialLibraryPath.EndsWith(".mtl"))
        {
            materialLibraryPath += ".mtl";
        }

        auto materialLibraryAsset = state.assetManager->Load<MaterialGroup>(materialLibraryPath);

        if (materialLibraryAsset.HasValue())
        {
            materialLibrary = materialLibraryAsset->Result();
        }
        else
        {
            HYP_LOG(Assets, Warning, "Obj model loader: Could not load material library at {}: {}", materialLibraryPath, materialLibraryAsset.GetError().GetMessage());
        }
    }

    const bool hasVertices = !model.positions.Empty(),
               hasNormals = !model.normals.Empty(),
               hasTexcoords = !model.texcoords.Empty();

    for (OBJMesh& objMesh : model.meshes)
    {
        Array<Vertex> vertices;
        vertices.Reserve(model.positions.Size());

        Array<uint32> indices;
        indices.Reserve(objMesh.indices.Size());

        HashMap<OBJIndex, uint32> indexMap;

        const bool hasIndices = !objMesh.indices.Empty();

        BoundingBox meshAabb = BoundingBox::Empty();

        if (hasIndices)
        {
            for (const OBJIndex& objIndex : objMesh.indices)
            {
                const auto it = indexMap.Find(objIndex);

                if (createObjIndices)
                {
                    if (it != indexMap.End())
                    {
                        indices.PushBack(it->second);

                        continue;
                    }
                }

                Vertex vertex;

                if (hasVertices)
                {
                    vertex.SetPosition(GetIndexedVertexProperty(objIndex.vertex, model.positions));

                    meshAabb = meshAabb.Union(vertex.GetPosition());
                }

                if (hasNormals)
                {
                    vertex.SetNormal(GetIndexedVertexProperty(objIndex.normal, model.normals));
                }

                if (hasTexcoords)
                {
                    vertex.SetTexCoord0(GetIndexedVertexProperty(objIndex.texcoord, model.texcoords));
                }

                const uint32 index = uint32(vertices.Size());

                vertices.PushBack(vertex);
                indices.PushBack(index);

                indexMap[objIndex] = index;
            }
        }
        else
        {
            /* mesh does not have faces defined */
            continue;
        }

        const Vec3f meshAabbMin = meshAabb.GetMin();
        const Vec3f meshAabbCenter = meshAabb.GetCenter();

        // offset all vertices by the AABB's center,
        // we will apply the transformation to the entity's transform component
        for (Vertex& vertex : vertices)
        {
            vertex.SetPosition(vertex.GetPosition() - meshAabbCenter);
        }

        Name assetName = CreateNameFromDynamicString(StringUtil::StripExtension(objMesh.name.Split('/', '\\').Back()));

        MeshData meshData;
        meshData.desc.numIndices = uint32(indices.Size());
        meshData.desc.numVertices = uint32(vertices.Size());
        meshData.vertexData = vertices;
        meshData.indexData.SetSize(indices.Size() * sizeof(uint32));
        meshData.indexData.Write(indices.Size() * sizeof(uint32), 0, indices.Data());

        meshData.CalculateNormals();
        meshData.CalculateTangents();

        Handle<Mesh> mesh = CreateObject<Mesh>();
        mesh->SetName(assetName);
        mesh->SetMeshData(meshData);

        mesh->GetAsset()->Rename(assetName);
        mesh->GetAsset()->SetOriginalFilepath(FilePath::Relative(state.filepath, state.assetManager->GetBasePath()));

        state.assetManager->GetAssetRegistry()->RegisterAsset("$Import/Media/Meshes", mesh->GetAsset());

        InitObject(mesh);

        Handle<Material> material;

        if (!objMesh.material.Empty() && materialLibrary)
        {
            if (materialLibrary->Has(objMesh.material))
            {
                material = materialLibrary->Get(objMesh.material);
            }
            else
            {
                HYP_LOG(Assets, Warning, "OBJ model loader: Material '{}' could not be found in material library", objMesh.material);
            }
        }

        if (!material.IsValid())
        {
            material = MaterialCache::GetInstance()->GetOrCreate(
                NAME("BasicOBJMaterial"),
                { .shaderDefinition = ShaderDefinition {
                      NAME("Forward"),
                      ShaderProperties(mesh->GetVertexAttributes()) },
                    .bucket = RB_OPAQUE },
                { { Material::MATERIAL_KEY_ALBEDO, Vec4f(1.0f) }, { Material::MATERIAL_KEY_ROUGHNESS, 0.65f }, { Material::MATERIAL_KEY_METALNESS, 0.0f } });
        }

        InitObject(material);

        Handle<Scene> scene = g_engineDriver->GetDefaultWorld()->GetDetachedScene(Threads::CurrentThreadId());

        const Handle<Entity> entity = scene->GetEntityManager()->AddEntity();

        scene->GetEntityManager()->AddComponent<TransformComponent>(entity, TransformComponent {});
        scene->GetEntityManager()->AddComponent<MeshComponent>(entity, MeshComponent { mesh, material });
        scene->GetEntityManager()->AddComponent<BoundingBoxComponent>(entity, BoundingBoxComponent { mesh->GetAABB() });
        
        entity->SetLocalTranslation(meshAabbCenter);
        
        top->AddChild(entity);
    }

    return LoadedAsset { top };
}

} // namespace hyperion
