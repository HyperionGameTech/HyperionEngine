#include "ogre_skeleton_loader.h"
#include "../../math/vector3.h"

namespace hyperion {
class OgreSkeletonHandler : public xml::SaxHandler {
public:
    OgreSkeletonHandler()
    {
        keyframe_bone_time = 0.0f;
        skeleton = std::make_shared<Skeleton>();
        bind_angle = 0.0f;
        keyframe_bone_time = 0.0f;
        keyframe_bone_angle = 0.0f;
        bone_index = 0;
    }

    std::shared_ptr<Skeleton> GetSkeleton()
    {
        return skeleton;
    }

    void Begin(const std::string &name, const xml::AttributeMap &attributes) override
    {
        if (name == "bone") {
            std::string bone_name = attributes.at("name");
            bone_index = std::stoi(attributes.at("id"));
            skeleton->AddBone(std::make_shared<Bone>(bone_name));
        } else if (name == "position") {
            float x = std::stof(attributes.at("x"));
            float y = std::stof(attributes.at("y"));
            float z = std::stof(attributes.at("z"));
            skeleton->GetBone(bone_index)->bind_pos = Vector3(x, y, z);
        } else if (name == "rotation") {
            bind_angle = std::stof(attributes.at("angle"));
        } else if (name == "boneparent") {
            std::string parent_name = attributes.at("parent");
            std::string child_name = attributes.at("bone");

            auto parent_bone = skeleton->GetBone(parent_name);
            auto child_bone = skeleton->GetBone(child_name);
            parent_bone->AddChild(child_bone);
        } else if (name == "track") {
            std::string bone_name = attributes.at("bone");
            keyframe_bone_name = bone_name;
            auto bone = skeleton->GetBone(keyframe_bone_name);
            if (bone) {
                skeleton->animations.back()->AddTrack(AnimationTrack(bone));
            }
        } else if (name == "translate") {
            float x = std::stof(attributes.at("x"));
            float y = std::stof(attributes.at("y"));
            float z = std::stof(attributes.at("z"));
            keyframe_bone_translation = Vector3(x, y, z);
        } else if (name == "rotate") {
            keyframe_bone_angle = std::stof(attributes.at("angle"));
        } else if (name == "axis") {
            float x = std::stof(attributes.at("x"));
            float y = std::stof(attributes.at("y"));
            float z = std::stof(attributes.at("z"));

            if (last_element_name == "rotate") { // it is a keyframe
                keyframe_bone_axis = Vector3(x, y, z);
            } else if (last_element_name == "rotation") { // it is a bone bind pose
                bind_axis = Vector3(x, y, z);
                skeleton->bones.at(bone_index)->bind_rot = Quaternion(Vector3(bind_axis).Normalize(), bind_angle);
            }
        } else if (name == "keyframe") {
            keyframe_bone_time = std::stof(attributes.at("time"));
        } else if (name == "animation") {
            skeleton->animations.push_back(std::make_shared<Animation>(attributes.at("name")));
        }

        last_element_name = name;
    }

    void End(const std::string &name) override
    {
        if (name == "keyframe") {
            if (skeleton->GetBone(keyframe_bone_name)) {
                Keyframe frame(keyframe_bone_time, keyframe_bone_translation, 
                    Quaternion(keyframe_bone_axis, keyframe_bone_angle).Invert());

                auto last_anim = skeleton->animations.back();
                last_anim->GetTrack(last_anim->NumTracks() - 1).AddFrame(frame);
            }

            keyframe_bone_angle = 0.0f;
            keyframe_bone_axis = Vector3::Zero();
            keyframe_bone_time = 0.0f;
            keyframe_bone_translation = Vector3::Zero();
        } else if (name == "track") {
            keyframe_bone_name.clear();
        }
    }

    void Characters(const std::string &value) override
    {
    }

    void Comment(const std::string &comment) override
    {
    }

private:
    std::string keyframe_bone_name;
    float keyframe_bone_time;
    float keyframe_bone_angle;
    Vector3 keyframe_bone_axis;
    Vector3 keyframe_bone_translation;
    std::shared_ptr<Skeleton> skeleton;
    float bind_angle;
    Vector3 bind_axis;
    int bone_index;
    std::string last_element_name;
};

AssetLoader::Result OgreSkeletonLoader::LoadFromFile(const std::string &path)
{
    OgreSkeletonHandler handler;
    xml::SaxParser parser(&handler);

    auto sax_result = parser.Parse(path);

    if (!sax_result) {
        return AssetLoader::Result(AssetLoader::Result::ASSET_ERR, sax_result.message);
    }


    return AssetLoader::Result(handler.GetSkeleton());
}
}
