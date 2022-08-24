#include "OgreXMLSkeletonLoader.hpp"
#include <Engine.hpp>
#include <animation/Skeleton.hpp>
#include <animation/Bone.hpp>

#include <Types.hpp>

#include <util/xml/SAXParser.hpp>

#include <algorithm>
#include <stack>

namespace hyperion::v2 {

using OgreXMLSkeletonLoader = LoaderObject<Skeleton, LoaderFormat::OGRE_XML_SKELETON>::Loader;

class OgreXMLSkeletonSAXHandler : public xml::SAXHandler {
public:
    OgreXMLSkeletonSAXHandler(LoaderState *state, OgreXMLSkeletonLoader::Object &object)
        : m_state(state),
          m_object(object)
    {
    }

    OgreXMLSkeletonLoader::Object::AnimationData &LastAnimation()
    {
        if (m_object.animations.empty()) {
            m_object.animations.emplace_back();
        }

        return m_object.animations.back();
    }

    OgreXMLSkeletonLoader::Object::AnimationTrackData &LastAnimationTrack()
    {
        auto &animation = LastAnimation();

        if (animation.tracks.empty()) {
            animation.tracks.emplace_back();
        }

        return animation.tracks.back();
    }

    OgreXMLSkeletonLoader::Object::KeyframeData &LastKeyframe()
    {
        auto &track = LastAnimationTrack();

        if (track.keyframes.empty()) {
            track.keyframes.emplace_back();
        }

        return track.keyframes.back();
    }

    template <class Lambda>
    OgreXMLSkeletonLoader::Object::BoneData *GetBone(Lambda lambda)
    {
        auto it = std::find_if(m_object.bones.begin(), m_object.bones.end(), lambda);

        if (it == m_object.bones.end()) {
            return nullptr;
        }

        return &*it;
    }

    virtual void Begin(const std::string &name, const xml::AttributeMap &attributes) override
    {
        if (name == "bone") {
            std::string bone_name = attributes.at("name");
            auto id = StringUtil::Parse<UInt>(attributes.at("id"));

            m_object.bones.push_back({
                .name = String(bone_name.c_str()),
                .id = id
            });
        } else if (name == "position") {
            auto x = StringUtil::Parse<float>(attributes.at("x"));
            auto y = StringUtil::Parse<float>(attributes.at("y"));
            auto z = StringUtil::Parse<float>(attributes.at("z"));

            if (!m_object.bones.empty()) {
                m_object.bones.back().binding_translation = Vector(x, y, z);
            } else {
                DebugLog(LogType::Warn, "Ogre XML skeleton parser: Attempt to add position when no bones exist yet\n");
            }
        } else if (name == "rotation") {
            m_binding_angles.push(StringUtil::Parse<float>(attributes.at("angle")));
        } else if (name == "boneparent") {
            std::string parent_name = attributes.at("parent");
            std::string child_name = attributes.at("bone");

            auto *child_bone = GetBone([&child_name](const auto &bone) {
                return bone.name == child_name.c_str();
            });

            if (child_bone != nullptr) {
                child_bone->parent_name = String(parent_name.c_str());
            } else {
                DebugLog(
                    LogType::Warn,
                    "Ogre XML skeleton parser: Attempt to set child bone '%s' to parent '%s' but child bone does not exist yet\n",
                    child_name.c_str(),
                    parent_name.c_str()
                );
            }
        } else if (name == "animation") {
            m_object.animations.push_back({
                .name = String(attributes.at("name").c_str())
            });
        } else if (name == "track") {
            LastAnimation().tracks.push_back({
                .bone_name = String(attributes.at("bone").c_str())
            });
        } else if (name == "keyframe") {
            LastAnimationTrack().keyframes.push_back({
                .time = StringUtil::Parse<float>(attributes.at("time"))
            });
        } else if (name == "translate") {
            auto x = StringUtil::Parse<float>(attributes.at("x"));
            auto y = StringUtil::Parse<float>(attributes.at("y"));
            auto z = StringUtil::Parse<float>(attributes.at("z"));

            LastKeyframe().translation = Vector(x, y, z);
        } else if (name == "rotate") {
            m_keyframe_angles.push(StringUtil::Parse<float>(attributes.at("angle")));
        } else if (name == "axis") {
            auto x = StringUtil::Parse<float>(attributes.at("x"));
            auto y = StringUtil::Parse<float>(attributes.at("y"));
            auto z = StringUtil::Parse<float>(attributes.at("z"));

            const auto axis = Vector(x, y, z).Normalized();

            if (m_element_tags.empty()) {
                DebugLog(LogType::Warn, "Ogre XML skeleton loader: Attempt to set rotation axis but no prior elements found\n");
            } else if (m_element_tags.top() == "rotate") { /* keyframe */
                if (m_keyframe_angles.empty()) {
                    DebugLog(LogType::Warn, "Ogre XML skeleton loader: Attempt to set keyframe rotation axis but no angle was set prior\n");
                } else {
                    LastKeyframe().rotation = Quaternion(axis, m_keyframe_angles.top()).Invert();

                    m_keyframe_angles.pop();
                }
            } else if (m_element_tags.top() == "rotation") { /* bone binding pose */
                if (m_binding_angles.empty()) {
                    DebugLog(LogType::Warn, "Ogre XML skeleton loader: Attempt to set bond binding rotation but no binding angles were set prior\n");
                } else {
                    if (m_object.bones.empty()) {
                        DebugLog(LogType::Warn, "Ogre XML skeleton loader: Attempt to set bone binding rotation but no bones were found\n");
                    } else {
                        m_object.bones.back().binding_rotation = Quaternion(axis, m_binding_angles.top());
                    }

                    m_binding_angles.pop();
                }
            }
        }

        m_element_tags.push(name);
    }

    virtual void End(const std::string &name) override
    {
        m_element_tags.pop();
    }

    virtual void Characters(const std::string &value) override {}
    virtual void Comment(const std::string &comment) override {}

private:
    LoaderState *m_state;
    OgreXMLSkeletonLoader::Object &m_object;

    std::stack<std::string> m_element_tags;
    std::stack<float> m_binding_angles;
    std::stack<float> m_keyframe_angles;
};

LoaderResult OgreXMLSkeletonLoader::LoadFn(LoaderState *state, Object &object)
{
    OgreXMLSkeletonSAXHandler handler(state, object);

    xml::SAXParser parser(&handler);
    auto sax_result = parser.Parse(&state->stream);

    if (!sax_result) {
        return { LoaderResult::Status::ERR, sax_result.message };
    }
    
    return { };
}

std::unique_ptr<Skeleton> OgreXMLSkeletonLoader::BuildFn(Engine *engine, const Object &object)
{
    auto skeleton = std::make_unique<Skeleton>();
    
    for (const auto &item : object.bones) {
        auto bone = std::make_unique<Bone>(item.name);

        bone->SetBindingTransform(Transform(
            item.binding_translation,
            Vector3::One(),
            item.binding_rotation
        ));

        if (item.parent_name.Any()) {
            if (auto *parent_bone = skeleton->FindBone(item.parent_name)) {
                parent_bone->AddChild(NodeProxy(bone.release()));

                continue;
            }

            DebugLog(
                LogType::Warn,
                "Ogre XML parser: Parent bone '%s' not found in skeleton at this stage\n",
                item.parent_name.Data()
            );
        } else if (skeleton->GetRootBone() != nullptr) {
            DebugLog(
                LogType::Warn,
                "Ogre XML parser: Attempt to set root bone to node '%s' but it has already been set\n",
                item.name.Data()
            );
        } else {
            skeleton->SetRootBone(std::move(bone));
        }
    }

    for (const auto &animation_it : object.animations) {
        auto animation = std::make_unique<Animation>(animation_it.name);

        for (const auto &track_it : animation_it.tracks) {
            AnimationTrack animation_track;
            animation_track.bone_name = track_it.bone_name;
            animation_track.keyframes.reserve(track_it.keyframes.size());
            
            for (const auto &keyframe_it : track_it.keyframes) {
                animation_track.keyframes.emplace_back(
                    keyframe_it.time,
                    Transform(keyframe_it.translation, Vector3::One(), keyframe_it.rotation)
                );
            }

            animation->AddTrack(animation_track);
        }

        skeleton->AddAnimation(std::move(animation));
    }

    if (auto *root_bone = skeleton->GetRootBone()) {
        root_bone->SetToBindingPose();

        root_bone->CalculateBoneRotation();
        root_bone->CalculateBoneTranslation();

        root_bone->StoreBindingPose();
        root_bone->ClearPose();

        root_bone->UpdateBoneTransform();
    }

    return skeleton;
}

} // namespace hyperion::v2
