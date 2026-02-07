/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <AssetPch.hpp>

#include <asset/skeleton_loaders/OgreXMLSkeletonLoader.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

#include <scene/animation/Skeleton.hpp>
#include <scene/animation/Bone.hpp>
#include <scene/animation/Animation.hpp>

#include <core/utilities/StringUtil.hpp>

#include <util/xml/SAXParser.hpp>

#include <algorithm>

#include <OgreXMLSkeletonLoader.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

using OgreXMLSkeleton = OgreXMLSkeletonLoader::OgreXMLSkeleton;

class OgreXMLSkeletonSAXHandler : public xml::SAXHandler
{
public:
    OgreXMLSkeletonSAXHandler(LoaderState* state, OgreXMLSkeleton& skeleton)
        : m_state(state),
          m_skeleton(skeleton)
    {
    }

    OgreXMLSkeleton::AnimationData& LastAnimation()
    {
        if (m_skeleton.animations.Empty())
        {
            m_skeleton.animations.PushBack({});
        }

        return m_skeleton.animations.Back();
    }

    OgreXMLSkeleton::AnimationTrackData& LastAnimationTrack()
    {
        auto& animation = LastAnimation();

        if (animation.tracks.Empty())
        {
            animation.tracks.PushBack({});
        }

        return animation.tracks.Back();
    }

    OgreXMLSkeleton::KeyframeData& LastKeyframe()
    {
        auto& track = LastAnimationTrack();

        if (track.keyframes.Empty())
        {
            track.keyframes.PushBack({});
        }

        return track.keyframes.Back();
    }

    template <class Lambda>
    OgreXMLSkeleton::BoneData* GetBone(Lambda lambda)
    {
        auto it = std::find_if(m_skeleton.bones.begin(), m_skeleton.bones.end(), lambda);

        if (it == m_skeleton.bones.end())
        {
            return nullptr;
        }

        return &*it;
    }

    virtual void Begin(const String& name, const xml::AttributeMap& attributes) override
    {
        if (name == "bone")
        {
            String boneName = attributes.At("name");
            const uint32 id = StringUtil::Parse<uint32>(attributes.At("id"));

            m_skeleton.bones.PushBack({ .name = std::move(boneName),
                .id = id });
        }
        else if (name == "position")
        {
            auto x = StringUtil::Parse<float>(attributes.At("x"));
            auto y = StringUtil::Parse<float>(attributes.At("y"));
            auto z = StringUtil::Parse<float>(attributes.At("z"));

            if (!m_skeleton.bones.Empty())
            {
                m_skeleton.bones.Back().bindingTranslation = Vector3(x, y, z);
            }
            else
            {
                HYP_LOG(Assets, Warning, "Ogre XML skeleton parser: Attempt to add position when no bones exist yet");
            }
        }
        else if (name == "rotation")
        {
            m_bindingAngles.Push(StringUtil::Parse<float>(attributes.At("angle")));
        }
        else if (name == "boneparent")
        {
            String parentName = attributes.At("parent");
            String childName = attributes.At("bone");

            auto* childBone = GetBone([&childName](const auto& bone)
                {
                    return bone.name == childName;
                });

            if (childBone != nullptr)
            {
                childBone->parentName = std::move(parentName);
            }
            else
            {
                HYP_LOG(Assets, Warning, "Ogre XML skeleton parser: Attempt to set child bone '{}' to parent '{}' but child bone does not exist yet", childName, parentName);
            }
        }
        else if (name == "animation")
        {
            m_skeleton.animations.PushBack({ .name = attributes.At("name") });
        }
        else if (name == "track")
        {
            LastAnimation().tracks.PushBack({ .boneName = attributes.At("bone") });
        }
        else if (name == "keyframe")
        {
            LastAnimationTrack().keyframes.PushBack({ .time = StringUtil::Parse<float>(attributes.At("time")) });
        }
        else if (name == "translate")
        {
            auto x = StringUtil::Parse<float>(attributes.At("x"));
            auto y = StringUtil::Parse<float>(attributes.At("y"));
            auto z = StringUtil::Parse<float>(attributes.At("z"));

            LastKeyframe().translation = Vector3(x, y, z);
        }
        else if (name == "rotate")
        {
            m_keyframeAngles.Push(StringUtil::Parse<float>(attributes.At("angle")));
        }
        else if (name == "axis")
        {
            auto x = StringUtil::Parse<float>(attributes.At("x"));
            auto y = StringUtil::Parse<float>(attributes.At("y"));
            auto z = StringUtil::Parse<float>(attributes.At("z"));

            const auto axis = Vector3(x, y, z).Normalized();

            if (m_elementTags.Empty())
            {
                HYP_LOG(Assets, Warning, "Ogre XML skeleton loader: Attempt to set rotation axis but no prior elements found");
            }
            else if (m_elementTags.Top() == "rotate")
            { /* keyframe */
                if (m_keyframeAngles.Empty())
                {
                    HYP_LOG(Assets, Warning, "Ogre XML skeleton loader: Attempt to set keyframe rotation axis but no angle was set prior");
                }
                else
                {
                    LastKeyframe().rotation = Quaternion(axis, m_keyframeAngles.Top()).Inverse();

                    m_keyframeAngles.Pop();
                }
            }
            else if (m_elementTags.Top() == "rotation")
            { /* bone binding pose */
                if (m_bindingAngles.Empty())
                {
                    HYP_LOG(Assets, Warning, "Ogre XML skeleton loader: Attempt to set bond binding rotation but no binding angles were set prior");
                }
                else
                {
                    if (m_skeleton.bones.Empty())
                    {
                        HYP_LOG(Assets, Warning, "Ogre XML skeleton loader: Attempt to set bone binding rotation but no bones were found");
                    }
                    else
                    {
                        m_skeleton.bones.Back().bindingRotation = Quaternion(axis, m_bindingAngles.Top());
                    }

                    m_bindingAngles.Pop();
                }
            }
        }

        m_elementTags.Push(name);
    }

    virtual void End(const String& name) override
    {
        m_elementTags.Pop();
    }

    virtual void Characters(const String& value) override
    {
    }

    virtual void Comment(const String& comment) override
    {
    }

private:
    LoaderState* m_state;
    OgreXMLSkeleton& m_skeleton;

    Stack<String> m_elementTags;
    Stack<float> m_bindingAngles;
    Stack<float> m_keyframeAngles;
};

AssetLoadResult OgreXMLSkeletonLoader::LoadAsset(LoaderState& state) const
{
    OgreXMLSkeleton object;

    OgreXMLSkeletonSAXHandler handler(&state, object);

    xml::SAXParser parser(&handler);
    xml::SAXParser::Result saxResult = parser.Parse(&state.stream);

    if (!saxResult)
    {
        return HYP_MAKE_ERROR(AssetLoadError, "Failed to parse XML: {}", saxResult.message);
    }

    SkeletonDesc skeletonDesc;

    Handle<Bone> rootBone;

    for (const auto& item : object.bones)
    {
        const Name boneName = CreateNameFromDynamicString(item.name);

        const Transform bindingTransform = Transform(
            item.bindingTranslation,
            Vec3f::One(),
            item.bindingRotation);

        BoneDesc& boneDesc = skeletonDesc.bones.EmplaceBack();
        boneDesc.name = boneName;
        boneDesc.bindingTransform = bindingTransform;

        Handle<Bone> bone = MakeHandle<Bone>(boneName);
        bone->SetBindingTransform(bindingTransform);

        if (item.parentName.Any())
        {
            if (!rootBone)
            {
                HYP_LOG(Assets, Warning, "Ogre XML parser: Attempt to set parent bone '{}' for node '{}' but no root bone has been set yet", item.parentName, item.name);

                continue;
            }
            
            boneDesc.parentName = CreateNameFromDynamicString(item.parentName);

            if (rootBone->GetBoneName() == item.parentName)
            {
                rootBone->AddChild(bone);

                continue;
            }

            auto parentBoneIt = std::find_if(rootBone->GetDescendants().begin(), rootBone->GetDescendants().end(), [parentName = item.parentName](Node* node)
                {
                    return node->IsA(Bone::StaticClass())
                        && static_cast<Bone*>(node)->GetBoneName() == parentName;
                });

            if (parentBoneIt != rootBone->GetDescendants().end())
            {
                Bone* parentBone = static_cast<Bone*>(*parentBoneIt);

                parentBone->AddChild(bone);

                continue;
            }

            HYP_LOG(Assets, Warning, "Ogre XML parser: Parent bone '{}' not found in hierarchy", item.parentName);
        }
        else if (rootBone)
        {
            HYP_LOG(Assets, Warning, "Ogre XML parser: Attempt to set root bone to node '{}' but it has already been set", item.name);
        }
        else
        {
            rootBone = bone;
        }
    }

    SkeletonData skeletonData;

    for (const auto& animationIt : object.animations)
    {
        const Name animationName = CreateNameFromDynamicString(animationIt.name);

        Handle<Animation> animation = MakeHandle<Animation>(animationName);

        for (const auto& trackIt : animationIt.tracks)
        {
            Handle<AnimationTrack> animationTrack = MakeHandle<AnimationTrack>(CreateNameFromDynamicString(trackIt.boneName));

            for (const auto& keyframeIt : trackIt.keyframes)
            {
                animationTrack->AddKeyframe(Keyframe(
                    keyframeIt.time,
                    Transform(keyframeIt.translation, Vector3::One(), keyframeIt.rotation)));
            }

            animation->AddTrack(animationTrack);
        }

        skeletonDesc.animationNames.PushBack(animationName);
        skeletonData.animations.PushBack(animation);
    }

    Handle<SkeletonAsset> skeletonAsset = MakeHandle<SkeletonAsset>(
        CreateNameFromDynamicString(state.filepath.Basename()),
        skeletonDesc,
        std::move(skeletonData));

    state.assetManager->GetAssetRegistry()->RegisterAsset("$Import/Media/Skeletons", skeletonAsset);

    Handle<Skeleton> skeleton = MakeHandle<Skeleton>(skeletonAsset);
    skeleton->SetRootBone(rootBone);

    if (Bone* rootBone = skeleton->GetRootBone())
    {
        rootBone->SetToBindingPose();

        rootBone->CalculateBoneRotation();
        rootBone->CalculateBoneTranslation();

        rootBone->StoreBindingPose();
        rootBone->ClearPose();

        rootBone->UpdateBoneTransform();
    }

    return LoadedAsset { skeleton };
}

} // namespace Hyperion
