#include "ogre_xml_model_loader.h"
#include <scene/controllers/animation_controller.h>
#include <engine.h>

#include <util/xml/sax_parser.h>

#include <algorithm>

namespace hyperion::v2 {

using OgreXmlModelLoader = LoaderObject<Node, LoaderFormat::OGRE_XML_MODEL>::Loader;

class OgreXmlSaxHandler : public xml::SaxHandler {
public:
    OgreXmlSaxHandler(LoaderState *state, OgreXmlModelLoader::Object &object)
        : m_object(object)
    {
    }

    OgreXmlModelLoader::Object::OgreSubMesh &LastSubMesh()
    {
        if (m_object.submeshes.empty()) {
            m_object.submeshes.emplace_back();
        }

        return m_object.submeshes.back();
    }

    void AddBoneAssignment(size_t vertex_index, OgreXmlModelLoader::Object::BoneAssignment &&bone_assignment)
    {
        m_object.bone_assignments[vertex_index].push_back(std::move(bone_assignment));
    }

    virtual void Begin(const std::string &name, const xml::AttributeMap &attributes) override
    {
        if (name == "position") {
            const auto x = StringUtil::Parse<float>(attributes.at("x"));
            const auto y = StringUtil::Parse<float>(attributes.at("y"));
            const auto z = StringUtil::Parse<float>(attributes.at("z"));

            m_object.positions.emplace_back(x, y, z);
        } else if (name == "normal") {
            const auto x = StringUtil::Parse<float>(attributes.at("x"));
            const auto y = StringUtil::Parse<float>(attributes.at("y"));
            const auto z = StringUtil::Parse<float>(attributes.at("z"));

            m_object.normals.emplace_back(x, y, z);
        } else if (name == "texcoord") {
            const auto x = StringUtil::Parse<float>(attributes.at("u"));
            const auto y = StringUtil::Parse<float>(attributes.at("v"));

            m_object.texcoords.emplace_back(x, y);
        } else if (name == "face") {
            if (attributes.size() != 3) {
                DebugLog(LogType::Warn, "Ogre XML parser: `face` tag expected to have 3 attributes.\n");
            }

            for (auto &it : attributes) {
                LastSubMesh().indices.push_back(StringUtil::Parse<Mesh::Index>(it.second));
            }
        } else if (name == "skeletonlink") {
            m_object.skeleton_name = attributes.at("name");
        } else if (name == "vertexboneassignment") {
            const auto vertex_index = StringUtil::Parse<uint32_t>(attributes.at("vertexindex"));
            const auto bone_index   = StringUtil::Parse<uint32_t>(attributes.at("boneindex"));
            const auto bone_weight  = StringUtil::Parse<float>(attributes.at("weight"));

            AddBoneAssignment(vertex_index, {bone_index, bone_weight});
        } else if (name == "submesh") {
            m_object.submeshes.emplace_back();
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
    OgreXmlModelLoader::Object &m_object;
};

void BuildVertices(OgreXmlModelLoader::Object &object)
{
    const bool has_normals   = !object.normals.empty(),
               has_texcoords = !object.texcoords.empty();

    std::vector<Vertex> vertices;
    vertices.resize(object.positions.size());

    for (size_t i = 0; i < vertices.size(); i++) {
        Vector3 position;
        Vector3 normal;
        Vector2 texcoord;

        if (i < object.positions.size()) {
            position = object.positions[i];
        } else {
            DebugLog(
                LogType::Warn,
                "Ogre XML parser: Vertex index (%lu) out of bounds (%llu)\n",
                i,
                object.positions.size()
            );

            continue;
        }

        if (has_normals) {
            if (i < object.normals.size()) {
                normal = object.normals[i];
            } else {
                DebugLog(
                    LogType::Warn,
                    "Ogre XML parser: Normal index (%lu) out of bounds (%llu)\n",
                    i,
                    object.normals.size()
                );
            }
        }

        if (has_texcoords) {
            if (i < object.texcoords.size()) {
                texcoord = object.texcoords[i];
            } else {
                DebugLog(
                    LogType::Warn,
                    "Ogre XML parser: Texcoord index (%lu) out of bounds (%llu)\n",
                    i,
                    object.texcoords.size()
                );
            }
        }

        vertices[i] = Vertex(position, texcoord, normal);

        const auto bone_it = object.bone_assignments.find(i);

        if (bone_it != object.bone_assignments.end()) {
            auto &bone_assignments = bone_it->second;

            for (size_t j = 0; j < bone_assignments.size(); j++) {
                if (j == 4) {
                    DebugLog(LogType::Warn, "Ogre XML parser: Attempt to add more than 4 bone assignments\n");

                    break;
                }

                vertices[i].AddBoneIndex(bone_assignments[j].index);
                vertices[i].AddBoneWeight(bone_assignments[j].weight);
            }
        }
    }

    object.vertices = std::move(vertices);
}

LoaderResult OgreXmlModelLoader::LoadFn(LoaderState *state, Object &object)
{
    object.filepath = state->filepath;

    OgreXmlSaxHandler handler(state, object);

    xml::SaxParser parser(&handler);
    auto sax_result = parser.Parse(&state->stream);

    if (!sax_result) {
        return {LoaderResult::Status::ERR, sax_result.message};
    }
    
    BuildVertices(object);

    return {};
}

std::unique_ptr<Node> OgreXmlModelLoader::BuildFn(Engine *engine, const Object &object)
{
    auto top = std::make_unique<Node>();

    std::unique_ptr<Skeleton> skeleton;

    if (!object.skeleton_name.empty()) {
        const auto skeleton_path = StringUtil::BasePath(object.filepath) + "/" + object.skeleton_name + ".xml";

        skeleton = engine->assets.Load<Skeleton>(skeleton_path);

        if (skeleton == nullptr) {
            DebugLog(LogType::Warn, "Ogre XML parser: Could not load skeleton at %s\n", skeleton_path.c_str());
        }
    }

    engine->resources.Lock([&](Resources &resources) {
        auto skeleton_ref = skeleton != nullptr
            ? resources.skeletons.Add(std::move(skeleton))
            : nullptr;

        for (auto &sub_mesh : object.submeshes) {
            if (sub_mesh.indices.empty()) {
                DebugLog(LogType::Info, "Ogre XML parser: Skipping submesh with empty indices\n");

                continue;
            }

            auto material = resources.materials.Add(std::make_unique<Material>("ogrexml_material"));

            auto mesh = resources.meshes.Add(
                std::make_unique<Mesh>(
                    object.vertices, 
                    sub_mesh.indices
                )
            );

            if (object.normals.empty()) {
                mesh->CalculateNormals();
            }

            mesh->CalculateTangents();

            auto vertex_attributes = mesh->GetVertexAttributes();

            auto shader = engine->shader_manager.GetShader(ShaderManager::Key::BASIC_FORWARD).IncRef();
            const auto shader_id = shader != nullptr ? shader->GetId() : Shader::empty_id;

            auto spatial = resources.spatials.Add(
                std::make_unique<Spatial>(
                    std::move(mesh),
                    std::move(shader),
                    std::move(material),
                    RenderableAttributeSet{
                        .bucket            = Bucket::BUCKET_OPAQUE,
                        .shader_id         = shader_id,
                        .vertex_attributes = vertex_attributes
                    }
                )
            );

            if (skeleton_ref != nullptr) {
                spatial->SetSkeleton(skeleton_ref.IncRef());
            }
            
            auto node = std::make_unique<Node>();
            node->SetSpatial(std::move(spatial));

            top->AddChild(std::move(node));
        }
        
        if (skeleton_ref != nullptr) {
            top->AddController<AnimationController>();
        }
    });

    return std::move(top);
}

} // namespace hyperion::v2
