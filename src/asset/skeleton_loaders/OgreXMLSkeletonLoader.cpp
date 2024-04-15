/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include "OgreXMLSkeletonLoader.hpp"
#include <Engine.hpp>
#include <scene/animation/Skeleton.hpp>
#include <scene/animation/Bone.hpp>

#include <Types.hpp>

#include <util/xml/SAXParser.hpp>

#include <algorithm>
#include <stack>

namespace hyperion::v2 {

using OgreXMLSkeleton = OgreXMLSkeletonLoader::OgreXMLSkeleton;

class OgreXMLSkeletonSAXHandler : public xml::SAXHandler
{
public:
    OgreXMLSkeletonSAXHandler(LoaderState *state, OgreXMLSkeleton &skeleton)
        : m_state(state),
          m_skeleton(skeleton)
    {
    }

    OgreXMLSkeleton::AnimationData &LastAnimation()
    {
        if (m_skeleton.animations.Empty()) {
            m_skeleton.animations.PushBack({ });
        }

        return m_skeleton.animations.Back();
    }

    OgreXMLSkeleton::AnimationTrackData &LastAnimationTrack()
    {
        auto &animation = LastAnimation();

        if (animation.tracks.Empty()) {
            animation.tracks.PushBack({ });
        }

        return animation.tracks.Back();
    }

    OgreXMLSkeleton::KeyframeData &LastKeyframe()
    {
        auto &track = LastAnimationTrack();

        if (track.keyframes.Empty()) {
            track.keyframes.PushBack({ });
        }

        return track.keyframes.Back();
    }

    template <class Lambda>
    OgreXMLSkeleton::BoneData *GetBone(Lambda lambda)
    {
        auto it = std::find_if(m_skeleton.bones.begin(), m_skeleton.bones.end(), lambda);

        if (it == m_skeleton.bones.end()) {
            return nullptr;
        }

        return &*it;
    }

    virtual void Begin(const String &name, const xml::AttributeMap &attributes) override
    {
        if (name == "bone") {
            String bone_name = attributes.At("name");
            const uint id = StringUtil::Parse<uint>(attributes.At("id"));

            m_skeleton.bones.PushBack({
                .name = std::move(bone_name),
                .id = id
            });
        } else if (name == "position") {
            auto x = StringUtil::Parse<float>(attributes.At("x"));
            auto y = StringUtil::Parse<float>(attributes.At("y"));
            auto z = StringUtil::Parse<float>(attributes.At("z"));

            if (!m_skeleton.bones.Empty()) {
                m_skeleton.bones.Back().binding_translation = Vector3(x, y, z);
            } else {
                DebugLog(LogType::Warn, "Ogre XML skeleton parser: Attempt to add position when no bones exist yet\n");
            }
        } else if (name == "rotation") {
            m_binding_angles.Push(StringUtil::Parse<float>(attributes.At("angle")));
        } else if (name == "boneparent") {
            String parent_name = attributes.At("parent");
            String child_name = attributes.At("bone");

            auto *child_bone = GetBone([&child_name](const auto &bone)
            {
                return bone.name == child_name;
            });

            if (child_bone != nullptr) {
                child_bone->parent_name = std::move(parent_name);
            } else {
                DebugLog(
                    LogType::Warn,
                    "Ogre XML skeleton parser: Attempt to set child bone '%s' to parent '%s' but child bone does not exist yet\n",
                    child_name.Data(),
                    parent_name.Data()
                );
            }
        } else if (name == "animation") {
            m_skeleton.animations.PushBack({
                .name = attributes.At("name")
            });
        } else if (name == "track") {
            LastAnimation().tracks.PushBack({
                .bone_name = attributes.At("bone")
            });
        } else if (name == "keyframe") {
            LastAnimationTrack().keyframes.PushBack({
                .time = StringUtil::Parse<float>(attributes.At("time"))
            });
        } else if (name == "translate") {
            auto x = StringUtil::Parse<float>(attributes.At("x"));
            auto y = StringUtil::Parse<float>(attributes.At("y"));
            auto z = StringUtil::Parse<float>(attributes.At("z"));

            LastKeyframe().translation = Vector3(x, y, z);
        } else if (name == "rotate") {
            m_keyframe_angles.Push(StringUtil::Parse<float>(attributes.At("angle")));
        } else if (name == "axis") {
            auto x = StringUtil::Parse<float>(attributes.At("x"));
            auto y = StringUtil::Parse<float>(attributes.At("y"));
            auto z = StringUtil::Parse<float>(attributes.At("z"));

            const auto axis = Vector3(x, y, z).Normalized();

            if (m_element_tags.Empty()) {
                DebugLog(LogType::Warn, "Ogre XML skeleton loader: Attempt to set rotation axis but no prior elements found\n");
            } else if (m_element_tags.Top() == "rotate") { /* keyframe */
                if (m_keyframe_angles.Empty()) {
                    DebugLog(LogType::Warn, "Ogre XML skeleton loader: Attempt to set keyframe rotation axis but no angle was set prior\n");
                } else {
                    LastKeyframe().rotation = Quaternion(axis, m_keyframe_angles.Top()).Invert();

                    m_keyframe_angles.Pop();
                }
            } else if (m_element_tags.Top() == "rotation") { /* bone binding pose */
                if (m_binding_angles.Empty()) {
                    DebugLog(LogType::Warn, "Ogre XML skeleton loader: Attempt to set bond binding rotation but no binding angles were set prior\n");
                } else {
                    if (m_skeleton.bones.Empty()) {
                        DebugLog(LogType::Warn, "Ogre XML skeleton loader: Attempt to set bone binding rotation but no bones were found\n");
                    } else {
                        m_skeleton.bones.Back().binding_rotation = Quaternion(axis, m_binding_angles.Top());
                    }

                    m_binding_angles.Pop();
                }
            }
        }

        m_element_tags.Push(name);
    }

    virtual void End(const String &name) override
    {
        m_element_tags.Pop();
    }

    virtual void Characters(const String &value) override {}
    virtual void Comment(const String &comment) override {}

private:
    LoaderState     *m_state;
    OgreXMLSkeleton &m_skeleton;

    Stack<String>   m_element_tags;
    Stack<float>    m_binding_angles;
    Stack<float>    m_keyframe_angles;
};

LoadedAsset OgreXMLSkeletonLoader::LoadAsset(LoaderState &state) const
{
    OgreXMLSkeleton object;

    OgreXMLSkeletonSAXHandler handler(&state, object);

    xml::SAXParser parser(&handler);
    auto sax_result = parser.Parse(&state.stream);

    if (!sax_result) {
        return { { LoaderResult::Status::ERR, sax_result.message }, UniquePtr<void>() };
    }

    auto skeleton_handle = UniquePtr<Handle<Skeleton>>::Construct(CreateObject<Skeleton>());

    for (const auto &item : object.bones) {
        UniquePtr<Bone> bone(new Bone(item.name));

        bone->SetBindingTransform(Transform(
            item.binding_translation,
            Vector3::One(),
            item.binding_rotation
        ));

        if (item.parent_name.Any()) {
            if (auto *parent_bone = (*skeleton_handle)->FindBone(item.parent_name)) {
                parent_bone->AddChild(NodeProxy(bone.Release()));

                continue;
            }

            DebugLog(
                LogType::Warn,
                "Ogre XML parser: Parent bone '%s' not found in skeleton at this stage\n",
                item.parent_name.Data()
            );
        } else if ((*skeleton_handle)->GetRootBone() != nullptr) {
            DebugLog(
                LogType::Warn,
                "Ogre XML parser: Attempt to set root bone to node '%s' but it has already been set\n",
                item.name.Data()
            );
        } else {
            (*skeleton_handle)->SetRootBone(NodeProxy(bone.Release()));
        }
    }

    for (const auto &animation_it : object.animations) {
        Animation animation(animation_it.name);

        for (const auto &track_it : animation_it.tracks) {
            AnimationTrack animation_track;
            animation_track.bone_name = track_it.bone_name;
            animation_track.keyframes.Reserve(track_it.keyframes.Size());
            
            for (const auto &keyframe_it : track_it.keyframes) {
                animation_track.keyframes.PushBack(Keyframe(
                    keyframe_it.time,
                    Transform(keyframe_it.translation, Vector3::One(), keyframe_it.rotation)
                ));
            }

            animation.AddTrack(animation_track);
        }

        (*skeleton_handle)->AddAnimation(std::move(animation));
    }

    if (auto *root_bone = (*skeleton_handle)->GetRootBone()) {
        root_bone->SetToBindingPose();

        root_bone->CalculateBoneRotation();
        root_bone->CalculateBoneTranslation();

        root_bone->StoreBindingPose();
        root_bone->ClearPose();

        root_bone->UpdateBoneTransform();
    }
    
    return { { LoaderResult::Status::OK }, skeleton_handle.Cast<void>() };
}

} // namespace hyperion::v2
