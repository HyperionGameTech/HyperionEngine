#include "OgreXMLModelLoader.hpp"
#include <scene/controllers/AnimationController.hpp>
#include <Engine.hpp>

#include <util/xml/SAXParser.hpp>

#include <algorithm>

namespace hyperion::v2 {

using OgreXMLModel = OgreXMLModelLoader::OgreXMLModel;
using BoneAssignment = OgreXMLModelLoader::OgreXMLModel::BoneAssignment;
using SubMesh = OgreXMLModelLoader::OgreXMLModel::SubMesh;

class OgreXMLSAXHandler : public xml::SAXHandler
{
public:
    OgreXMLSAXHandler(LoaderState *state, OgreXMLModel &object)
        : m_model(object)
    {
    }

    SubMesh &LastSubMesh()
    {
        if (m_model.submeshes.empty()) {
            m_model.submeshes.emplace_back();
        }

        return m_model.submeshes.back();
    }

    void AddBoneAssignment(UInt vertex_index, BoneAssignment &&bone_assignment)
    {
        m_model.bone_assignments[vertex_index].push_back(bone_assignment);
    }

    virtual void Begin(const std::string &name, const xml::AttributeMap &attributes) override
    {
        if (name == "position") {
            const auto x = StringUtil::Parse<float>(attributes.at("x"));
            const auto y = StringUtil::Parse<float>(attributes.at("y"));
            const auto z = StringUtil::Parse<float>(attributes.at("z"));

            m_model.positions.emplace_back(x, y, z);
        } else if (name == "normal") {
            const auto x = StringUtil::Parse<float>(attributes.at("x"));
            const auto y = StringUtil::Parse<float>(attributes.at("y"));
            const auto z = StringUtil::Parse<float>(attributes.at("z"));

            m_model.normals.emplace_back(x, y, z);
        } else if (name == "texcoord") {
            const auto x = StringUtil::Parse<float>(attributes.at("u"));
            const auto y = StringUtil::Parse<float>(attributes.at("v"));

            m_model.texcoords.emplace_back(x, y);
        } else if (name == "face") {
            if (attributes.size() != 3) {
                DebugLog(LogType::Warn, "Ogre XML parser: `face` tag expected to have 3 attributes.\n");
            }

            for (auto &it : attributes) {
                LastSubMesh().indices.push_back(StringUtil::Parse<Mesh::Index>(it.second));
            }
        } else if (name == "skeletonlink") {
            m_model.skeleton_name = attributes.at("name");
        } else if (name == "vertexboneassignment") {
            const auto vertex_index = StringUtil::Parse<uint32_t>(attributes.at("vertexindex"));
            const auto bone_index   = StringUtil::Parse<uint32_t>(attributes.at("boneindex"));
            const auto bone_weight  = StringUtil::Parse<float>(attributes.at("weight"));

            AddBoneAssignment(vertex_index, {bone_index, bone_weight});
        } else if (name == "submesh") {
            m_model.submeshes.emplace_back();
        } else if (name == "vertex") {
            /* no-op */
        }  else {
            DebugLog(LogType::Warn, "Ogre XML parser: No handler for '%s' tag\n", name.c_str());
        }
    }

    virtual void End(const std::string &name) override {}
    virtual void Characters(const std::string &value) override {}
    virtual void Comment(const std::string &comment) override {}

private:
    OgreXMLModel &m_model;
};

void BuildVertices(OgreXMLModel &model)
{
    const bool has_normals = !model.normals.empty(),
        has_texcoords = !model.texcoords.empty();

    std::vector<Vertex> vertices;
    vertices.resize(model.positions.size());

    for (SizeType i = 0; i < vertices.size(); i++) {
        Vector3 position;
        Vector3 normal;
        Vector2 texcoord;

        if (i < model.positions.size()) {
            position = model.positions[i];
        } else {
            DebugLog(
                LogType::Warn,
                "Ogre XML parser: Vertex index (%lu) out of bounds (%llu)\n",
                i,
                model.positions.size()
            );

            continue;
        }

        if (has_normals) {
            if (i < model.normals.size()) {
                normal = model.normals[i];
            } else {
                DebugLog(
                    LogType::Warn,
                    "Ogre XML parser: Normal index (%lu) out of bounds (%llu)\n",
                    i,
                    model.normals.size()
                );
            }
        }

        if (has_texcoords) {
            if (i < model.texcoords.size()) {
                texcoord = model.texcoords[i];
            } else {
                DebugLog(
                    LogType::Warn,
                    "Ogre XML parser: Texcoord index (%lu) out of bounds (%llu)\n",
                    i,
                    model.texcoords.size()
                );
            }
        }

        vertices[i] = Vertex(position, texcoord, normal);

        const auto bone_it = model.bone_assignments.find(i);

        if (bone_it != model.bone_assignments.end()) {
            auto &bone_assignments = bone_it->second;

            for (SizeType j = 0; j < bone_assignments.size(); j++) {
                if (j == 4) {
                    DebugLog(LogType::Warn, "Ogre XML parser: Attempt to add more than 4 bone assignments\n");

                    break;
                }

                vertices[i].AddBoneIndex(bone_assignments[j].index);
                vertices[i].AddBoneWeight(bone_assignments[j].weight);
            }
        }
    }

    model.vertices = std::move(vertices);
}

LoadedAsset OgreXMLModelLoader::LoadAsset(LoaderState &state) const
{
    AssertThrow(state.asset_manager != nullptr);

    OgreXMLModel model;
    model.filepath = state.filepath;

    OgreXMLSAXHandler handler(&state, model);

    xml::SAXParser parser(&handler);
    auto sax_result = parser.Parse(&state.stream);

    if (!sax_result) {
        return { { LoaderResult::Status::ERR, sax_result.message }, UniquePtr<void>() };
    }
    
    BuildVertices(model);

    auto top = UniquePtr<Node>::Construct();

    Handle<Skeleton> skeleton;

    if (!model.skeleton_name.empty()) {
        const String skeleton_path((StringUtil::BasePath(model.filepath) + "/" + model.skeleton_name + ".xml").c_str());

        LoaderResult result;
        skeleton = state.asset_manager->Load<Skeleton>(skeleton_path, result);

        if (result.status != LoaderResult::Status::OK || !skeleton) {
            DebugLog(LogType::Warn, "Ogre XML parser: Could not load skeleton at %s\n", skeleton_path.Data());
        }
    }

    for (auto &sub_mesh : model.submeshes) {
        if (sub_mesh.indices.empty()) {
            DebugLog(LogType::Info, "Ogre XML parser: Skipping submesh with empty indices\n");

            continue;
        }

        Handle<Material> material = CreateObject<Material>(HYP_NAME(ogrexml_material));

        Handle<Mesh> mesh = CreateObject<Mesh>(
            model.vertices,
            sub_mesh.indices,
            Topology::TRIANGLES
        );

        if (model.normals.empty()) {
            mesh->CalculateNormals();
        }

        mesh->CalculateTangents();


        auto vertex_attributes = mesh->GetVertexAttributes();
        
        ShaderProperties shader_properties(vertex_attributes);
        shader_properties.Set("SKINNING", skeleton.IsValid());

        Handle<Shader> shader = g_shader_manager->GetOrCreate(HYP_NAME(Forward), shader_properties);

        auto entity = CreateObject<Entity>(
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

        if (skeleton) {
            entity->AddController<AnimationController>();
            entity->SetSkeleton(skeleton);
        }
        
        NodeProxy node(new Node);
        node.SetEntity(std::move(entity));

        top->AddChild(std::move(node));
    }

    return { { LoaderResult::Status::OK }, top.Cast<void>() };
}

} // namespace hyperion::v2
