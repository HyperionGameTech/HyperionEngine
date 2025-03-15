/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/skeleton_loaders/OgreXMLSkeletonLoader.hpp>

#include <scene/animation/Skeleton.hpp>
#include <scene/animation/Bone.hpp>
#include <scene/animation/Animation.hpp>

#include <core/logging/Logger.hpp>

#include <core/containers/Stack.hpp>

#include <Types.hpp>

#include <util/xml/SAXParser.hpp>

#include <algorithm>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

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
            const uint32 id = StringUtil::Parse<uint32>(attributes.At("id"));

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
                HYP_LOG(Assets, Warning, "Ogre XML skeleton parser: Attempt to add position when no bones exist yet");
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
                HYP_LOG(Assets, Warning, "Ogre XML skeleton parser: Attempt to set child bone '{}' to parent '{}' but child bone does not exist yet", child_name, parent_name);
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
                HYP_LOG(Assets, Warning, "Ogre XML skeleton loader: Attempt to set rotation axis but no prior elements found");
            } else if (m_element_tags.Top() == "rotate") { /* keyframe */
                if (m_keyframe_angles.Empty()) {
                    HYP_LOG(Assets, Warning, "Ogre XML skeleton loader: Attempt to set keyframe rotation axis but no angle was set prior");
                } else {
                    LastKeyframe().rotation = Quaternion(axis, m_keyframe_angles.Top()).Invert();

                    m_keyframe_angles.Pop();
                }
            } else if (m_element_tags.Top() == "rotation") { /* bone binding pose */
                if (m_binding_angles.Empty()) {
                    HYP_LOG(Assets, Warning, "Ogre XML skeleton loader: Attempt to set bond binding rotation but no binding angles were set prior");
                } else {
                    if (m_skeleton.bones.Empty()) {
                        HYP_LOG(Assets, Warning, "Ogre XML skeleton loader: Attempt to set bone binding rotation but no bones were found");
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

AssetLoadResult OgreXMLSkeletonLoader::LoadAsset(LoaderState &state) const
{
    OgreXMLSkeleton object;

    OgreXMLSkeletonSAXHandler handler(&state, object);

    xml::SAXParser parser(&handler);
    xml::SAXParser::Result sax_result = parser.Parse(&state.stream);

    if (!sax_result) {
        return HYP_MAKE_ERROR(AssetLoadError, "Failed to parse XML: {}", sax_result.message);
    }

    Handle<Skeleton> skeleton_handle = CreateObject<Skeleton>();

    for (const auto &item : object.bones) {
        RC<Bone> bone = MakeRefCountedPtr<Bone>(item.name);

        bone->SetBindingTransform(Transform(
            item.binding_translation,
            Vec3f::One(),
            item.binding_rotation
        ));

        if (item.parent_name.Any()) {
            if (Bone *parent_bone = skeleton_handle->FindBone(item.parent_name)) {
                parent_bone->AddChild(NodeProxy(bone));

                continue;
            }

            HYP_LOG(Assets, Warning, "Ogre XML parser: Parent bone '{}' not found in skeleton at this stage", item.parent_name);
        } else if (skeleton_handle->GetRootBone() != nullptr) {
            HYP_LOG(Assets, Warning, "Ogre XML parser: Attempt to set root bone to node '{}' but it has already been set", item.name);
        } else {
            skeleton_handle->SetRootBone(bone);
        }
    }

    for (const auto &animation_it : object.animations) {
        Handle<Animation> animation = CreateObject<Animation>(animation_it.name);

        for (const auto &track_it : animation_it.tracks) {
            AnimationTrackDesc animation_track_desc;
            animation_track_desc.bone_name = track_it.bone_name;
            animation_track_desc.keyframes.Reserve(track_it.keyframes.Size());
            
            for (const auto &keyframe_it : track_it.keyframes) {
                animation_track_desc.keyframes.PushBack(Keyframe(
                    keyframe_it.time,
                    Transform(keyframe_it.translation, Vector3::One(), keyframe_it.rotation)
                ));
            }

            animation->AddTrack(CreateObject<AnimationTrack>(animation_track_desc));
        }

        skeleton_handle->AddAnimation(animation);
    }

    if (Bone *root_bone = skeleton_handle->GetRootBone()) {
        root_bone->SetToBindingPose();

        root_bone->CalculateBoneRotation();
        root_bone->CalculateBoneTranslation();

        root_bone->StoreBindingPose();
        root_bone->ClearPose();

        root_bone->UpdateBoneTransform();
    }
    
    return LoadedAsset { skeleton_handle };
}

} // namespace hyperion
