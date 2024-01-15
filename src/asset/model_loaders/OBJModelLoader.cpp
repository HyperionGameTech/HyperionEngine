#include "OBJModelLoader.hpp"
#include <Engine.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <util/fs/FsUtil.hpp>

#include <algorithm>
#include <stack>
#include <string>


namespace hyperion::v2 {

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

    Int value_index = 0;

    for (SizeType i = offset; i < tokens.Size(); i++) {
        const String &token = tokens[i];

        if (token.Empty()) {
            continue;
        }

        result.values[value_index++] = static_cast<Float>(std::atof(token.Data()));

        if (value_index == std::size(result.values)) {
            break;
        }
    }

    return result;
}

static void AddMesh(OBJModel &model, const String &tag, const String &material)
{
    String unique_tag(tag);
    Int counter = 0;

    while (model.meshes.Any([&unique_tag](const OBJMesh &obj_mesh) { return obj_mesh.tag == unique_tag; })) {
        unique_tag = tag + String::ToString(++counter);
    }

    model.meshes.PushBack(OBJMesh {
        .tag = unique_tag,
        .material = material
    });
}

static auto &LastMesh(OBJModel &model)
{
    if (model.meshes.Empty()) {
        AddMesh(model, "default", "default");
    }

    return model.meshes.Back();
}

static auto ParseOBJIndex(const String &token)
{
    OBJIndex obj_index { 0, 0, 0 };
    SizeType token_index = 0;

    const auto parts = token.Split('/');

    for (const auto &part : parts) {
        if (!part.Empty()) {
            switch (token_index) {
            case 0:
                obj_index.vertex = StringUtil::Parse<Int64>(part.Data()) - 1;
                break;
            case 1:
                obj_index.texcoord = StringUtil::Parse<Int64>(part.Data()) - 1;
                break;
            case 2:
                obj_index.normal = StringUtil::Parse<Int64>(part.Data()) - 1;
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
Vector GetIndexedVertexProperty(Int64 vertex_index, const Array<Vector> &vectors)
{
    const Int64 vertex_absolute = vertex_index >= 0
        ? vertex_index
        : static_cast<Int64>(vectors.Size()) + (vertex_index);

    AssertReturnMsg(
        vertex_absolute >= 0 && vertex_absolute < static_cast<Int64>(vectors.Size()),
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

        const auto trimmed = line.Trimmed();

        if (trimmed.Empty() || trimmed.Front() == '#') {
            return;
        }

        tokens = trimmed.Split(' ');

        if (tokens.Empty()) {
            return;
        }

        if (tokens[0] == "v") {
            model.positions.PushBack(ReadVector<Vector3>(tokens, 1));

            return;
        }

        if (tokens[0] == "vn") {
            model.normals.PushBack(ReadVector<Vector3>(tokens, 1));

            return;
        }

        if (tokens[0] == "vt") {
            model.texcoords.PushBack(ReadVector<Vector2>(tokens, 1));

            return;
        }

        if (tokens[0] == "f") {
            auto &last_mesh = LastMesh(model);

            /* we don't support per-face material so we compromise by setting the mesh's material
             * to the last 'usemtl' value when we hit the face command.
             */
            if (!active_material.Empty()) {
                last_mesh.material = active_material;
            }

            if (tokens.Size() > 5) {
                DebugLog(LogType::Warn, "Faces with more than 4 vertices are not supported by the OBJ model loader\n");
            }

            /* Performs simple triangulation on quad faces */
            for (Int64 i = 0; i < static_cast<Int64>(tokens.Size()) - 3; i++) {
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
                DebugLog(LogType::Warn, "Cannot set obj model material -- no material provided");

                return;
            }

            active_material = tokens[1];

            if constexpr (mesh_per_material) {
                AddMesh(model, active_material, active_material);
            }

            return;
        }

        DebugLog(LogType::Warn, "Unable to parse obj model line: %s\n", trimmed.Data());
    });

    return model;
}

LoadedAsset OBJModelLoader::BuildModel(LoaderState &state, OBJModel &model)
{
    AssertThrow(state.asset_manager != nullptr);

    auto top = UniquePtr<Node>::Construct(model.tag);

    Handle<MaterialGroup> material_library;
    
    if (load_materials && !model.material_library.Empty()) {
        auto material_library_path = String(FileSystem::RelativePath(
            (String(StringUtil::BasePath(model.filepath.Data()).c_str()) + "/" + model.material_library).Data(),
            FileSystem::CurrentPath()
        ).c_str());

        if (!material_library_path.EndsWith(".mtl")) {
            material_library_path += ".mtl";
        }

        LoaderResult result;
        material_library = state.asset_manager->Load<MaterialGroup>(material_library_path, result);

        if (result.status != LoaderResult::Status::OK || !material_library) {
            DebugLog(LogType::Warn, "Obj model loader: Could not load material library at %s\n", material_library_path.Data());
        }
    }

    const bool has_vertices = !model.positions.Empty(),
        has_normals = !model.normals.Empty(),
        has_texcoords = !model.texcoords.Empty();

    for (auto &obj_mesh : model.meshes) {
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

                const auto index = Mesh::Index(vertices.Size());

                vertices.PushBack(vertex);
                indices.PushBack(index);

                index_map[obj_index] = index;
            }
        } else {
            /* mesh does not have faces defined */
            DebugLog(
                LogType::Warn,
                "Obj model loader: Mesh does not have any faces defined; skipping.\n"
            );

            continue;
        }

        auto mesh = CreateObject<Mesh>(
            vertices, 
            indices,
            Topology::TRIANGLES
        );

        if (!has_normals) {
            mesh->CalculateNormals();
        }

        mesh->CalculateTangents();

        InitObject(mesh);

        Handle<Material> material;

        // if (!obj_mesh.material.Empty() && material_library) {
        //     if (material_library->Has(obj_mesh.material)) {
        //         material = material_library->Get(obj_mesh.material);
        //     } else {
        //         DebugLog(
        //             LogType::Warn,
        //             "Obj model loader: Material '%s' could not be found in material library\n",
        //             obj_mesh.material.Data()
        //         );
        //     }
        // }

        if (!material) {
            material = g_material_system->GetOrCreate(
                {
                    .shader_definition = ShaderDefinition {
                        HYP_NAME(Forward),
                        ShaderProperties(mesh->GetVertexAttributes())
                    },
                    .bucket = Bucket::BUCKET_OPAQUE
                },
                {
                    { Material::MATERIAL_KEY_ALBEDO, Vector4(1.0f) },
                    { Material::MATERIAL_KEY_ROUGHNESS, 0.65f },
                    { Material::MATERIAL_KEY_METALNESS, 0.0f }
                }
            );
        }

        InitObject(material);

        const ID<Entity> entity = g_engine->GetWorld()->GetDetachedScene()->GetEntityManager()->AddEntity();

        g_engine->GetWorld()->GetDetachedScene()->GetEntityManager()->AddComponent(
            entity,
            TransformComponent { }
        );

        g_engine->GetWorld()->GetDetachedScene()->GetEntityManager()->AddComponent(
            entity,
            MeshComponent {
                mesh,
                material
            }
        );

        g_engine->GetWorld()->GetDetachedScene()->GetEntityManager()->AddComponent(
            entity,
            BoundingBoxComponent {
                mesh->GetAABB()
            }
        );

        g_engine->GetWorld()->GetDetachedScene()->GetEntityManager()->AddComponent(
            entity,
            VisibilityStateComponent { }
        );

        auto *node = new Node(obj_mesh.tag);
        node->SetEntity(entity);

        // Takes ownership of node
        top->AddChild(NodeProxy(node));
    }

    return { { LoaderResult::Status::OK }, top.Cast<void>() };
}

} // namespace hyperion::v2
