#include "ogre_loader.h"
#include "../../asset/asset_manager.h"
#include "../../rendering/environment.h"
#include "../../entity.h"
#include "../../math/vector2.h"
#include "../../math/vector3.h"
#include "../../math/vertex.h"
#include "../../rendering/mesh.h"
#include "../../rendering/shader_manager.h"
#include "../../rendering/shaders/lighting_shader.h"
#include "../../animation/bone.h"
#include "../../animation/animation.h"
#include "../../animation/skeleton.h"
#include "../../animation/skeleton_control.h"
#include "../../util.h"
#include "../../util/string_util.h"

#include <vector>
#include <map>
#include <memory>

namespace hyperion {
struct BoneAssign {
    size_t vertex_idx;
    size_t bone_idx;
    float bone_weight;
};

struct OgreSubmesh {
    std::vector<size_t> faces;
    std::vector<Vertex> vertices;
};

class OgreHandler : public xml::SaxHandler {
public:
    OgreHandler(const std::string &filepath)
        : filepath(filepath),
          submeshes_enabled(true),
          has_submeshes(false),
          has_normals(false),
          has_texcoords(false)
    {
    }

    std::vector<Vector3> positions;
    std::vector<Vector3> normals;
    std::vector<Vector2> texcoords;
    std::vector<size_t> faces;

    std::vector<OgreSubmesh> submeshes;
    bool submeshes_enabled;
    bool has_submeshes, has_normals, has_texcoords;

    std::vector<std::shared_ptr<Bone>> bones;
    std::vector<std::shared_ptr<Animation>> animations;
    std::map<size_t, std::vector<BoneAssign>> bone_assigns;

    OgreSubmesh &CurrentSubmesh()
    {
        return submeshes.back();
    }

    void AddBoneAssign(size_t idx, const BoneAssign &assign)
    {
        if (bone_assigns.find(idx) != bone_assigns.end()) {
            std::vector<BoneAssign> &vec = bone_assigns[idx];
            if (vec.size() < 4) {
                vec.push_back(assign);
            }
        } else {
            std::vector<BoneAssign> vec;
            vec.push_back(assign);
            bone_assigns[idx] = vec;
        }
    }

    void LoopThrough(const std::vector<size_t> &faces, std::vector<Vertex> &out)
    {
        for (size_t i = 0; i < faces.size(); i++) {
            Vertex vert(positions[faces[i]],
                !texcoords.empty() ? texcoords[faces[i]] : Vector2(),
                !normals.empty() ? normals[faces[i]] : Vector3());

            if (bone_assigns.find(faces[i]) != bone_assigns.end()) {
                std::vector<BoneAssign> &vec = bone_assigns[faces[i]];
                for (size_t j = 0; j < vec.size(); j++) {
                    vert.AddBoneIndex(vec[j].bone_idx);
                    vert.AddBoneWeight(vec[j].bone_weight);
                }
            }
            out.push_back(vert);
        }
    }

    void Begin(const std::string &name, const xml::AttributeMap &attributes)
    {
        if (name == "position") {
            float x = std::stof(attributes.at("x"));
            float y = std::stof(attributes.at("y"));
            float z = std::stof(attributes.at("z"));
            positions.push_back(Vector3(x, y, z));
        } else if (name == "normal") {
            float x = std::stof(attributes.at("x"));
            float y = std::stof(attributes.at("y"));
            float z = std::stof(attributes.at("z"));
            normals.push_back(Vector3(x, y, z));
        } else if (name == "texcoord") {
            float x = std::stof(attributes.at("u"));
            float y = std::stof(attributes.at("v"));
            texcoords.push_back(Vector2(x, y));
        } else if (name == "face") {
            AssertThrow(attributes.size() == 3);

            if (!has_submeshes) {
                for (auto &&it : attributes) {
                    faces.push_back(std::stof(it.second));
                }
            } else {
                for (auto &&it : attributes) {
                    CurrentSubmesh().faces.push_back(std::stof(it.second));
                }
            }
        } else if (name == "skeletonlink") {
            std::string skel_name = attributes.begin()->second;
            std::string current = filepath;
            current = current.substr(0, current.find_last_of("\\/"));
            if (!StringUtil::Contains(current, "/") && !StringUtil::Contains(current, "\\")) { // so just make the string empty
                current.clear();
            }
            current += "/" + skel_name + ".xml";

            if (auto skeleton = AssetManager::GetInstance()->LoadFromFile<Skeleton>(current)) {
                for (auto &&bone : skeleton->bones) {
                    bones.push_back(bone);
                }
                for (auto &&anim : skeleton->animations) {
                    animations.push_back(anim);
                }
            }
        } else if (name == "vertexboneassignment") {
            size_t vidx = (size_t)std::stol(attributes.at("vertexindex"));
            size_t bidx = (size_t)std::stol(attributes.at("boneindex"));
            float bweight = std::stof(attributes.at("weight"));
            AddBoneAssign(vidx, { vidx, bidx, bweight });
        } else if (name == "submesh") {
            has_submeshes = submeshes_enabled;
            submeshes.push_back(OgreSubmesh());
        } else if (name == "vertexbuffer") {
            auto it = attributes.find("normals");

            if (it != attributes.end() && it->second == "true") {
                has_normals = true;
            }

            it = attributes.find("texture_coords");

            if (it != attributes.end() && it->second != "0") {
                has_texcoords = true;
            }
        }
    }

    void End(const std::string &name)
    {
        // get out of me swamp
    }

    void Characters(const std::string &value)
    {
    }

    void Comment(const std::string &comment)
    {
    }

private:
    std::string filepath;
};

std::shared_ptr<Loadable> OgreLoader::LoadFromFile(const std::string &path)
{
    OgreHandler handler(path);
    xml::SaxParser parser(&handler);
    parser.Parse(path);

    auto final_node = std::make_shared<Entity>();

    std::string current = std::string(path);
    std::string node_filepath = current.substr(current.find_last_of("\\/") + 1);
    node_filepath = node_filepath.substr(0, node_filepath.find_first_of(".")); // trim extension
    final_node->SetName(node_filepath);

    std::vector<Vertex> vertices;
    if (!handler.has_submeshes) {
        handler.LoopThrough(handler.faces, vertices);
    } else {
        for (int i = handler.submeshes.size() - 1; i > -1; i--) {
            OgreSubmesh &s = handler.submeshes[i];
            if (!s.faces.empty()) {
                handler.LoopThrough(s.faces, s.vertices);
            } else {
                handler.submeshes.erase(handler.submeshes.begin() + i);
            }
        }
    }

    if (!handler.bones.empty()) {
        for (auto &&bone : handler.bones) {
            bone->SetToBindingPose();
        }

        handler.bones.at(0)->CalcBindingRotation();
        handler.bones.at(0)->CalcBindingTranslation();

        for (auto &&bone : handler.bones) {
            bone->StoreBindingPose();
            bone->ClearPose();
        }
    }

    bool has_animations = !handler.animations.empty();
    bool has_bones = !handler.bones.empty();

    ShaderProperties shader_properties;

    if (has_bones) {
        shader_properties.Define("SKINNING", true);
        shader_properties.Define("NUM_BONES", int(handler.bones.size()));
    }

    auto shader = ShaderManager::GetInstance()->GetShader<LightingShader>(shader_properties);

    if (!handler.has_submeshes) {
        auto mesh = std::make_shared<Mesh>();
        mesh->SetShader(shader);
        mesh->SetVertices(vertices);

        if (has_bones) {
            mesh->SetAttribute(Mesh::ATTR_BONEINDICES, Mesh::MeshAttribute::BoneIndices);
            mesh->SetAttribute(Mesh::ATTR_BONEWEIGHTS, Mesh::MeshAttribute::BoneWeights);
        }
        if (handler.has_normals) {
            mesh->SetAttribute(Mesh::ATTR_NORMALS, Mesh::MeshAttribute::Normals);
        }
        if (handler.has_texcoords) {
            mesh->SetAttribute(Mesh::ATTR_TEXCOORDS0, Mesh::MeshAttribute::TexCoords0);
        }

        auto ent = std::make_shared<Entity>();
        ent->SetRenderable(mesh);
        final_node->AddChild(ent);
    } else {
        for (auto &&sm : handler.submeshes) {
            auto mesh = std::make_shared<Mesh>();
            mesh->SetShader(shader);
            mesh->SetVertices(sm.vertices);

            if (has_bones) {
                mesh->SetAttribute(Mesh::ATTR_BONEINDICES, Mesh::MeshAttribute::BoneIndices);
                mesh->SetAttribute(Mesh::ATTR_BONEWEIGHTS, Mesh::MeshAttribute::BoneWeights);
            }

            if (handler.has_normals) {
                mesh->SetAttribute(Mesh::ATTR_NORMALS, Mesh::MeshAttribute::Normals);
            } else {
                mesh->CalculateNormals();
            }

            if (handler.has_texcoords) {
                mesh->SetAttribute(Mesh::ATTR_TEXCOORDS0, Mesh::MeshAttribute::TexCoords0);
            }

            mesh->CalculateTangents();

            auto ent = std::make_shared<Entity>();
            ent->SetRenderable(mesh);
            final_node->AddChild(ent);
        }
    }

    if (has_bones) {
        auto skeleton_control = std::make_shared<SkeletonControl>(shader);

        if (has_animations) {
            for (auto &&anim : handler.animations) {
                skeleton_control->AddAnimation(anim);
            }
        }

        if (handler.bones.size() != 0) {
            if (auto root_bone = handler.bones.front()) {
                final_node->AddChild(root_bone);
                root_bone->UpdateTransform();
            }
        }

        final_node->AddControl(skeleton_control);
    }

    return final_node;
}
} // namespace hyperion
