/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/model_loaders/OBJModelLoader.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>

#include <core/logging/Logger.hpp>

#include <util/fs/FsUtil.hpp>

#include <Engine.hpp>

#include <algorithm>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

constexpr bool create_obj_indices = true;
constexpr bool mesh_per_material = true; // set true to create a new mesh on each instance of 'use <mtllib>'
constexpr bool load_materials = true;

using Tokens = Array<String>;

using OBJModel = OBJModelLoader::OBJModel;
using OBJMesh = OBJModel::OBJMesh;
using OBJIndex = OBJModel::OBJIndex;

template <class Vector>
static Vector ReadVector(const Tokens &tokens, SizeType offset = 1)
{
    Vector result { 0.0f };

    int value_index = 0;

    for (SizeType i = offset; i < tokens.Size(); i++) {
        const String &token = tokens[i];

        if (token.Empty()) {
            continue;
        }

        result.values[value_index++] = static_cast<typename Vector::Type>(std::atof(token.Data()));

        if (value_index == std::size(result.values)) {
            break;
        }
    }

    return result;
}

static void AddMesh(OBJModel &model, const String &tag, const String &material)
{
    String unique_tag(tag);
    int counter = 0;

    while (model.meshes.Any([&unique_tag](const OBJMesh &obj_mesh) { return obj_mesh.tag == unique_tag; })) {
        unique_tag = tag + String::ToString(++counter);
    }

    model.meshes.PushBack(OBJMesh {
        .tag        = unique_tag,
        .material   = material
    });
}

static OBJMesh &LastMesh(OBJModel &model)
{
    if (model.meshes.Empty()) {
        AddMesh(model, "default", "default");
    }

    return model.meshes.Back();
}

static OBJIndex ParseOBJIndex(const String &token)
{
    OBJIndex obj_index { 0, 0, 0 };
    SizeType token_index = 0;

    const Array<String> parts = token.Split('/');

    for (const String &part : parts) {
        if (!part.Empty()) {
            switch (token_index) {
            case 0:
                obj_index.vertex = StringUtil::Parse<int64>(part.Data()) - 1;
                break;
            case 1:
                obj_index.texcoord = StringUtil::Parse<int64>(part.Data()) - 1;
                break;
            case 2:
                obj_index.normal = StringUtil::Parse<int64>(part.Data()) - 1;
                break;
            default:
                // ??
                break;
            }
        }

        ++token_index;
    }

    return obj_index;
}

template <class Vector>
Vector GetIndexedVertexProperty(int64 vertex_index, const Array<Vector> &vectors)
{
    const int64 vertex_absolute = vertex_index >= 0
        ? vertex_index
        : static_cast<int64>(vectors.Size()) + (vertex_index);

    AssertReturnMsg(
        vertex_absolute >= 0 && vertex_absolute < static_cast<int64>(vectors.Size()),
        Vector(),
        "Vertex index of %lld (absolute: %lld) is out of bounds (%llu)\n",
        vertex_index,
        vertex_absolute,
        vectors.Size()
    );

    return vectors[vertex_absolute];
}

OBJModel OBJModelLoader::LoadModel(LoaderState &state)
{
    OBJModel model;
    model.filepath = state.filepath;

    Tokens tokens;
    tokens.Reserve(5);
    
    model.tag = "unnamed";

    String active_material;

    state.stream.ReadLines([&](const String &line, bool *) {
        tokens.Clear();

        const String trimmed = line.Trimmed();

        if (trimmed.Empty() || trimmed.Front() == '#') {
            return;
        }

        tokens = trimmed.Split(' ');

        if (tokens.Empty()) {
            return;
        }

        if (tokens[0] == "v") {
            model.positions.PushBack(ReadVector<Vec3f>(tokens, 1));

            return;
        }

        if (tokens[0] == "vn") {
            model.normals.PushBack(ReadVector<Vec3f>(tokens, 1));

            return;
        }

        if (tokens[0] == "vt") {
            model.texcoords.PushBack(ReadVector<Vec2f>(tokens, 1));

            return;
        }

        if (tokens[0] == "f") {
            OBJMesh &last_mesh = LastMesh(model);

            /* we don't support per-face material so we compromise by setting the mesh's material
             * to the last 'usemtl' value when we hit the face command.
             */
            if (!active_material.Empty()) {
                last_mesh.material = active_material;
            }

            if (tokens.Size() > 5) {
                HYP_LOG(Assets, LogLevel::WARNING, "Faces with more than 4 vertices are not supported by the OBJ model loader");
            }

            /* Performs simple triangulation on quad faces */
            for (int64 i = 0; i < static_cast<int64>(tokens.Size()) - 3; i++) {
                last_mesh.indices.PushBack(ParseOBJIndex(tokens[1]));
                last_mesh.indices.PushBack(ParseOBJIndex(tokens[2 + i]));
                last_mesh.indices.PushBack(ParseOBJIndex(tokens[3 + i]));
            }

            return;
        }

        if (tokens[0] == "o") {
            if (tokens.Size() != 1) {
                model.tag = tokens[1];
            }

            return;
        }

        if (tokens[0] == "s") {
            /* smooth shading; ignore */
            return;
        }

        if (tokens[0] == "mtllib") {
            if (tokens.Size() != 1) {
                String material_library_name;

                for (SizeType i = 1; i < tokens.Size(); i++) {
                    if (i != 1) {
                        material_library_name += ' ';
                    }

                    material_library_name += tokens[i];
                }

                model.material_library = material_library_name;
            }

            return;
        }

        if (tokens[0] == "g") {
            String tag = "default";

            if (tokens.Size() != 1) {
                tag = tokens[1];
            }
            
            AddMesh(model, tag, active_material);

            return;
        }

        if (tokens[0] == "usemtl") {
            if (tokens.Size() == 1) {
                HYP_LOG(Assets, LogLevel::WARNING, "Cannot set obj model material -- no material provided");

                return;
            }

            active_material = tokens[1];

            if constexpr (mesh_per_material) {
                AddMesh(model, active_material, active_material);
            }

            return;
        }

        HYP_LOG(Assets, LogLevel::WARNING, "Unable to parse obj model line: {}", trimmed);
    });

    return model;
}

LoadedAsset OBJModelLoader::BuildModel(LoaderState &state, OBJModel &model)
{
    AssertThrow(state.asset_manager != nullptr);

    NodeProxy top(new Node(model.tag));

    Handle<MaterialGroup> material_library;
    
    if (load_materials && !model.material_library.Empty()) {
        String material_library_path = String(FileSystem::RelativePath(
            (String(StringUtil::BasePath(model.filepath.Data()).c_str()) + "/" + model.material_library).Data(),
            FileSystem::CurrentPath()
        ).c_str());

        if (!material_library_path.EndsWith(".mtl")) {
            material_library_path += ".mtl";
        }

        auto material_library_asset = state.asset_manager->Load<MaterialGroup>(material_library_path);

        if (material_library_asset.IsOK()) {
            material_library = material_library_asset.Result();
        } else {
            HYP_LOG(Assets, LogLevel::WARNING, "Obj model loader: Could not load material library at {}: {}", material_library_path, material_library_asset.result.message);
        }
    }

    const bool has_vertices = !model.positions.Empty(),
        has_normals = !model.normals.Empty(),
        has_texcoords = !model.texcoords.Empty();

    for (OBJMesh &obj_mesh : model.meshes) {
        Array<Vertex> vertices;
        vertices.Reserve(model.positions.Size());

        Array<Mesh::Index> indices;
        indices.Reserve(obj_mesh.indices.Size());

        FlatMap<OBJIndex, Mesh::Index> index_map;

        const bool has_indices = !obj_mesh.indices.Empty();

        if (has_indices) {
            for (const OBJIndex &obj_index : obj_mesh.indices) {
                const auto it = index_map.Find(obj_index);

                if (create_obj_indices) {
                    if (it != index_map.End()) {
                        indices.PushBack(it->second);

                        continue;
                    }
                }

                Vertex vertex;

                if (has_vertices) {
                    vertex.SetPosition(GetIndexedVertexProperty(obj_index.vertex, model.positions));
                }

                if (has_normals) {
                    vertex.SetNormal(GetIndexedVertexProperty(obj_index.normal, model.normals));
                }

                if (has_texcoords) {
                    vertex.SetTexCoord0(GetIndexedVertexProperty(obj_index.texcoord, model.texcoords));
                }

                const uint32 index = uint32(vertices.Size());

                vertices.PushBack(vertex);
                indices.PushBack(index);

                index_map[obj_index] = index;
            }
        } else {
            /* mesh does not have faces defined */
            HYP_LOG(Assets, LogLevel::WARNING, "Obj model loader: Mesh does not have any faces defined; skipping.");

            continue;
        }

        Handle<Mesh> mesh = CreateObject<Mesh>(
            vertices, 
            indices,
            Topology::TRIANGLES
        );

        mesh->SetName(CreateNameFromDynamicString(obj_mesh.tag));

        if (!has_normals) {
            mesh->CalculateNormals();
        }

        mesh->CalculateTangents();

        InitObject(mesh);

        Handle<Material> material;

        if (!obj_mesh.material.Empty() && material_library) {
            if (material_library->Has(obj_mesh.material)) {
                material = material_library->Get(obj_mesh.material)->Clone();
            } else {
                HYP_LOG(Assets, LogLevel::WARNING, "OBJ model loader: Material '{}' could not be found in material library", obj_mesh.material);
            }
        }

        if (!material.IsValid()) {
            material = g_material_system->GetOrCreate(
                {
                    .shader_definition = ShaderDefinition {
                        NAME("Forward"),
                        ShaderProperties(mesh->GetVertexAttributes())
                    },
                    .bucket = Bucket::BUCKET_OPAQUE
                },
                {
                    { Material::MATERIAL_KEY_ALBEDO, Vec4f(1.0f) },
                    { Material::MATERIAL_KEY_ROUGHNESS, 0.65f },
                    { Material::MATERIAL_KEY_METALNESS, 0.0f }
                }
            );
        }

        InitObject(material);

        Handle<Scene> scene = g_engine->GetWorld()->GetDetachedScene(Threads::CurrentThreadID());

        const ID<Entity> entity = scene->GetEntityManager()->AddEntity();

        scene->GetEntityManager()->AddComponent(
            entity,
            TransformComponent { }
        );

        scene->GetEntityManager()->AddComponent(
            entity,
            MeshComponent {
                mesh,
                material
            }
        );

        scene->GetEntityManager()->AddComponent(
            entity,
            BoundingBoxComponent {
                mesh->GetAABB()
            }
        );

        scene->GetEntityManager()->AddComponent(
            entity,
            VisibilityStateComponent { }
        );

        NodeProxy node(new Node(obj_mesh.tag));
        node->SetEntity(entity);
        
        top->AddChild(std::move(node));
    }

    return { { LoaderResult::Status::OK }, top };
}

} // namespace hyperion
