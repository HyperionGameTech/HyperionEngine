#include "OBJModelLoader.hpp"
#include <Engine.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <util/fs/FsUtil.hpp>

#include <algorithm>
#include <stack>
#include <string>


namespace hyperion::v2 {

constexpr bool create_obj_indices = true;
constexpr bool mesh_per_material = true; // set true to create a new mesh on each instance of 'use <mtllib>'
constexpr bool load_materials = true;

using Tokens = std::vector<std::string>;

using OBJModel = OBJModelLoader::OBJModel;
using OBJMesh = OBJModel::OBJMesh;
using OBJIndex = OBJModel::OBJIndex;

template <class Vector>
static Vector ReadVector(const Tokens &tokens, SizeType offset = 1)
{
    Vector result { 0.0f };

    Int value_index = 0;

    for (SizeType i = offset; i < tokens.size(); i++) {
        const auto &token = tokens[i];

        if (token.empty()) {
            continue;
        }

        result.values[value_index++] = static_cast<Float>(std::atof(token.c_str()));

        if (value_index == std::size(result.values)) {
            break;
        }
    }

    return result;
}

static void AddMesh(OBJModel &model, const std::string &tag, const std::string &material)
{
    std::string unique_tag(tag);
    Int counter = 0;

    while (std::any_of(
        model.meshes.begin(),
        model.meshes.end(),
        [&unique_tag](const auto &mesh) { return mesh.tag == unique_tag; }
    )) {
        unique_tag = tag + std::to_string(++counter);
    }

    model.meshes.push_back(OBJMesh {
        .tag = unique_tag,
        .material = material
    });
}

static auto &LastMesh(OBJModel &model)
{
    if (model.meshes.empty()) {
        AddMesh(model, "default", "default");
    }

    return model.meshes.back();
}

static auto ParseOBJIndex(const std::string &token)
{
    OBJIndex obj_index { 0, 0, 0 };
    SizeType token_index = 0;

    StringUtil::SplitBuffered(token, '/', [&token_index, &obj_index](const std::string &index_str) {
        if (!index_str.empty()) {
            switch (token_index) {
            case 0:
                obj_index.vertex = StringUtil::Parse<Int64>(index_str) - 1;
                break;
            case 1:
                obj_index.texcoord = StringUtil::Parse<Int64>(index_str) - 1;
                break;
            case 2:
                obj_index.normal = StringUtil::Parse<Int64>(index_str) - 1;
                break;
            default:
                // ??
                break;
            }
        }

        ++token_index;
    });

    return obj_index;
}

template <class Vector>
Vector GetIndexedVertexProperty(Int64 vertex_index, const std::vector<Vector> &vectors)
{
    const Int64 vertex_absolute = vertex_index >= 0
        ? vertex_index
        : static_cast<Int64>(vectors.size()) + (vertex_index);

    AssertReturnMsg(
        vertex_absolute >= 0 && vertex_absolute < static_cast<Int64>(vectors.size()),
        Vector(),
        "Vertex index of %lld (absolute: %lld) is out of bounds (%llu)\n",
        vertex_index,
        vertex_absolute,
        vectors.size()
    );

    return vectors[vertex_absolute];
}

OBJModel OBJModelLoader::LoadModel(LoaderState &state)
{
    OBJModel model;
    model.filepath = state.filepath;

    Tokens tokens;
    tokens.reserve(5);

    auto splitter = [&tokens](const std::string &token) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    };

    model.tag = "unnamed";

    std::string active_material;

    state.stream.ReadLines([&](const String &line) {
        tokens.clear();

        const auto trimmed = StringUtil::Trim(std::string(line.Data()));

        if (trimmed.empty() || trimmed.front() == '#') {
            return;
        }

        StringUtil::SplitBuffered(trimmed, ' ', splitter);

        if (tokens.empty()) {
            return;
        }

        if (tokens[0] == "v") {
            model.positions.push_back(ReadVector<Vector3>(tokens, 1));

            return;
        }

        if (tokens[0] == "vn") {
            model.normals.push_back(ReadVector<Vector3>(tokens, 1));

            return;
        }

        if (tokens[0] == "vt") {
            model.texcoords.push_back(ReadVector<Vector2>(tokens, 1));

            return;
        }

        if (tokens[0] == "f") {
            auto &last_mesh = LastMesh(model);

            /* we don't support per-face material so we compromise by setting the mesh's material
             * to the last 'usemtl' value when we hit the face command.
             */
            if (!active_material.empty()) {
                last_mesh.material = active_material;
            }

            if (tokens.size() > 5) {
                DebugLog(LogType::Warn, "Faces with more than 4 vertices are not supported by the OBJ model loader\n");
            }

            /* Performs simple triangulation on quad faces */
            for (Int64 i = 0; i < static_cast<Int64>(tokens.size()) - 3; i++) {
                last_mesh.indices.push_back(ParseOBJIndex(tokens[1]));
                last_mesh.indices.push_back(ParseOBJIndex(tokens[2 + i]));
                last_mesh.indices.push_back(ParseOBJIndex(tokens[3 + i]));
            }

            return;
        }

        if (tokens[0] == "o") {
            if (tokens.size() != 1) {
                model.tag = tokens[1];
            }

            return;
        }

        if (tokens[0] == "s") {
            /* smooth shading; ignore */
            return;
        }

        if (tokens[0] == "mtllib") {
            if (tokens.size() != 1) {
                std::stringstream material_library_name;

                for (SizeType i = 1; i < tokens.size(); i++) {
                    if (i != 1) {
                        material_library_name << ' ';
                    }

                    material_library_name << tokens[i];
                }

                model.material_library = material_library_name.str();
            }

            return;
        }

        if (tokens[0] == "g") {
            std::string tag = "default";

            if (tokens.size() != 1) {
                tag = tokens[1];
            }
            
            AddMesh(model, tag, active_material);

            return;
        }

        if (tokens[0] == "usemtl") {
            if (tokens.size() == 1) {
                DebugLog(LogType::Warn, "Cannot set obj model material -- no material provided");

                return;
            }

            active_material = tokens[1];

            if constexpr (mesh_per_material) {
                AddMesh(model, active_material, active_material);
            }

            return;
        }

        DebugLog(LogType::Warn, "Unable to parse obj model line: %s\n", trimmed.c_str());
    });

    return model;
}

LoadedAsset OBJModelLoader::BuildModel(LoaderState &state, OBJModel &model)
{
    AssertThrow(state.asset_manager != nullptr);

    auto top = UniquePtr<Node>::Construct(model.tag.c_str());

    Handle<MaterialGroup> material_library;
    
    if (load_materials && !model.material_library.empty()) {
        auto material_library_path = FileSystem::RelativePath(
            StringUtil::BasePath(model.filepath) + "/" + model.material_library,
            FileSystem::CurrentPath()
        );

        if (!StringUtil::EndsWith(material_library_path, ".mtl")) {
            material_library_path += ".mtl";
        }

        LoaderResult result;
        material_library = state.asset_manager->Load<MaterialGroup>(material_library_path.c_str(), result);

        if (result.status != LoaderResult::Status::OK || !material_library) {
            DebugLog(LogType::Warn, "Obj model loader: Could not load material library at %s\n", material_library_path.c_str());
        }
    }

    const bool has_vertices = !model.positions.empty(),
        has_normals = !model.normals.empty(),
        has_texcoords = !model.texcoords.empty();

    for (auto &obj_mesh : model.meshes) {
        std::vector<Vertex> vertices;
        vertices.reserve(model.positions.size());

        std::vector<Mesh::Index> indices;
        indices.reserve(obj_mesh.indices.size());

        std::map<OBJIndex, Mesh::Index> index_map;

        const bool has_indices = !obj_mesh.indices.empty();

        if (has_indices) {
            for (const OBJIndex &obj_index : obj_mesh.indices) {
                const auto it = index_map.find(obj_index);

                if (create_obj_indices) {
                    if (it != index_map.end()) {
                        indices.push_back(it->second);

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

                const auto index = Mesh::Index(vertices.size());

                vertices.push_back(vertex);
                indices.push_back(index);

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

        Handle<Material> material;

        if (!obj_mesh.material.empty() && material_library) {
            if (material_library->Has(obj_mesh.material)) {
                material = material_library->Get(obj_mesh.material);
            } else {
                DebugLog(
                    LogType::Warn,
                    "Obj model loader: Material '%s' could not be found in material library\n",
                    obj_mesh.material.c_str()
                );
            }
        }

        if (!material) {
            material = Engine::Get()->CreateObject<Material>();
        }

        auto mesh = Engine::Get()->CreateObject<Mesh>(
            vertices, 
            indices,
            Topology::TRIANGLES
        );

        if (!has_normals) {
            mesh->CalculateNormals();
        }

        mesh->CalculateTangents();

        auto vertex_attributes = mesh->GetVertexAttributes();

        auto shader = Engine::Get()->shader_manager.GetShader(ShaderManager::Key::BASIC_FORWARD_SKINNED);

        auto entity = Engine::Get()->CreateObject<Entity>(
            std::move(mesh),
            std::move(shader),
            std::move(material),
            RenderableAttributeSet(
                MeshAttributes {
                    .vertex_attributes = vertex_attributes
                },
                MaterialAttributes {
                    .bucket = Bucket::BUCKET_OPAQUE
                }
            )
        );

        entity->SetName(String(obj_mesh.tag.c_str()));
        
        auto node = std::make_unique<Node>(obj_mesh.tag.c_str());
        node->SetEntity(std::move(entity));

        top->AddChild(NodeProxy(node.release()));
    }

    return { { LoaderResult::Status::OK }, top.Cast<void>() };
}

} // namespace hyperion::v2
