/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/Assets.hpp>
#include <Asset/BlobStorage.hpp>

#include <Scene/Animation/Animation.hpp>
#include <Scene/Animation/Bone.hpp>
#include <Scene/Animation/Skeleton.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/IO/ByteWriter.hpp>
#include <Core/IO/ByteReader.hpp>

#include <Animation.generated.inl>

namespace Hyperion {

namespace /* Helpers */ {

bool IsBoneWithin(Bone& bone, Name rootBoneName)
{
    for (Node* node = &bone; node != nullptr; node = node->GetParent())
    {
        const Bone* ancestor = DynamicCast<Bone>(node);

        if (!ancestor)
        {
            break;
        }

        if (ancestor->GetBoneName() == rootBoneName)
        {
            return true;
        }
    }

    return false;
}

HYP_FORCE_INLINE bool IsBoneOverlaid(Bone& bone, const ApplyAnimParams& params)
{
    return (params.overlayRootBone.IsValid() && IsBoneWithin(bone, params.overlayRootBone))
        || (params.overlaySecondRootBone.IsValid() && IsBoneWithin(bone, params.overlaySecondRootBone));
}

} // namespace

#pragma region AnimationTrack

AnimationTrack::AnimationTrack()
{
}

AnimationTrack::AnimationTrack(Name name, Name boneName)
    : AssetObject(name),
      m_boneName(boneName)
{
}

AnimationTrack::~AnimationTrack()
{
    FreeBlobData(m_keyframeData);
}

void AnimationTrack::PageBlobData()
{
    if (m_keyframeData.raw == nullptr
        && m_keyframeData.key
        && m_keyframeData.size != 0)
    {
        if (PageBlobDataFromStorage(m_keyframeData))
        {
            return;
        }

        Handle<AssetRegistry> registry = GetAssetRegistry();
        AssertDebug(registry.IsValid());

        if (registry.IsValid())
        {
            // check if failed; if so, try to import from raw data blob in project directory
            const Name blobKey = m_keyframeData.key;
            const uint64 expectedSize = m_keyframeData.size;

            FileByteReader stream { registry->GetRootPath() / AssetBuckets::AnimationTracks.GetName() / (String(*GetName()) + ".KEYF.raw.blob") };
            if (!stream.Eof())
            {
                if (stream.Max() != expectedSize)
                {
                    HYP_LOG(Engine, Error, "Local blob data for animation track '{}' keyframes is {} bytes but the manifest expects {}, ignoring it",
                            GetName(), stream.Max(), expectedSize);

                    return;
                }

                ByteBuffer buffer = stream.Read(stream.Max());

                AllocateBlobData(m_keyframeData, buffer.Data(), buffer.Size(), alignof(Keyframe));
                m_keyframeData.key = blobKey;

                return;
            }
        }
            
        HYP_LOG(Engine, Error, "Data corruption detected for {} due to missing blob data", GetPath().ToString());

        m_keyframeData.readOnly = true;
    }
}

void AnimationTrack::UnpageBlobData()
{
    AssetObject::UnpageBlobData();
    
    AssertBlobDataPersisted(m_keyframeData);

    if (!m_keyframeData.readOnly)
    {
        FreeBlobData(m_keyframeData);
    }

    m_keyframeData.raw = nullptr;
}

float AnimationTrack::GetLength() const
{
    if (m_keyframeData.size == 0)
    {
        return 0.0f;
    }

    Span<const Keyframe> keyframes = GetKeyframes();
    AssertDebug(keyframes.Size() > 0);

    if (HYP_UNLIKELY(keyframes.Size() == 0))
    {
        return 0.0f;
    }

    return keyframes[keyframes.Size() - 1].time;
}

void AnimationTrack::SetKeyframes(Span<const Keyframe> keyframes)
{
    FreeBlobData(m_keyframeData);
    AllocateBlobData(m_keyframeData, keyframes.Data(), sizeof(Keyframe) * keyframes.Size());
}

Keyframe AnimationTrack::GetKeyframe(float time) const
{
    HYP_SCOPE;

    int first = 0, second = -1;

    if (m_keyframeData.size == 0)
    {
        return { time, Transform() };
    }

    Span<const Keyframe> keyframes = GetKeyframes();

    if (HYP_UNLIKELY(keyframes.Size() == 0))
    {
        return { time, Transform() };
    }

    // hold the last pose once this track ends (tracks in one animation can have different lengths)
    if (time >= keyframes[keyframes.Size() - 1].time)
    {
        return { time, keyframes[keyframes.Size() - 1].transform };
    }

    for (int i = 0; i < int(keyframes.Size()) - 1; i++)
    {
        if (MathUtil::InRange(time, { keyframes[i].time, keyframes[i + 1].time }))
        {
            first = i;
            second = i + 1;

            break;
        }
    }

    const Keyframe& current = keyframes[first];

    Transform transform = current.transform;

    if (second > first)
    {
        const Keyframe& next = keyframes[second];

        const float delta = (time - current.time) / (next.time - current.time);

        transform.translation = transform.translation.Lerp(next.transform.translation, delta);
        transform.scale = transform.scale.Lerp(next.transform.scale, delta);
        transform.rotation = transform.rotation.Slerp(next.transform.rotation, delta);
    }

    return { time, transform };
}

#pragma endregion AnimationTrack

#pragma region Animation

Animation::Animation() = default;

Animation::Animation(Name name)
    : AssetObject(name)
{
}

void Animation::AddTrack(const Handle<AnimationTrack>& track)
{
    HYP_SCOPE;

    if (!track)
    {
        return;
    }

    track->SetPersistentRequested(true);

    m_tracks.PushBack(track);
}

void Animation::SetTracks(const Array<Handle<AnimationTrack>>& tracks)
{
    for (const Handle<AnimationTrack>& track : tracks)
    {
        track->SetPersistentRequested(true);
    }

    m_tracks = tracks;
}

void Animation::Apply(Skeleton& skeleton, const ApplyAnimParams& params)
{
    HYP_SCOPE;

    for (const Handle<AnimationTrack>& track : m_tracks)
    {
        Bone* bone = skeleton.FindBone(track->GetBoneName());

        if (!bone)
        {
            continue;
        }

        bone->ClearPose();
        bone->SetKeyframe(track->GetKeyframe(params.time));
    }
}

void Animation::ApplyBlended(Skeleton& skeleton, const ApplyAnimParams& params)
{
    HYP_SCOPE;

    for (const Handle<AnimationTrack>& track : m_tracks)
    {
        Bone* bone = skeleton.FindBone(track->GetBoneName());

        if (!bone)
        {
            continue;
        }

        if (params.blend <= MathUtil::epsilonF)
        {
            bone->ClearPose();
        }

        Keyframe frame = track->GetKeyframe(params.time);
        Keyframe blended = bone->GetKeyframe().Blend(
            frame,
            MathUtil::Clamp(params.blend, 0.0f, 1.0f));

        bone->SetKeyframe(blended);
    }
}

void Animation::ApplyLayered(Skeleton& skeleton, const ApplyAnimParams& params)
{
    HYP_SCOPE;

    float layerWeight = MathUtil::Clamp(params.layerWeight, 0.0f, 1.0f);
    float blend = MathUtil::Clamp(params.blend, 0.0f, 1.0f);
    float secondLayerWeight = MathUtil::Clamp(params.secondLayerWeight, 0.0f, 1.0f);
    float overlayWeight = MathUtil::Clamp(params.overlayWeight, 0.0f, 1.0f);

    const Animation* layerAnimation = params.layerAnimation;

    const Animation* secondLayerAnimation = params.secondLayerAnimation;

    if (secondLayerWeight <= 0.0f)
    {
        secondLayerAnimation = nullptr;
    }

    const Animation* overlayAnimation = overlayWeight > 0.0f ? params.overlayAnimation : nullptr;

    for (const Handle<AnimationTrack>& track : m_tracks)
    {
        Bone* bone = skeleton.FindBone(track->GetBoneName());
        if (!bone)
        {
            continue;
        }

        Keyframe frame = track->GetKeyframe(params.time);

        if (const AnimationTrack* layerTrack = layerAnimation != nullptr ? layerAnimation->FindTrack(track->GetBoneName()) : nullptr)
        {
            if (!params.layerExcludedBone.IsValid() || !IsBoneWithin(*bone, params.layerExcludedBone))
            {
                frame = frame.Blend(layerTrack->GetKeyframe(params.layerTime), layerWeight);
            }
        }

        if (secondLayerAnimation != nullptr)
        {
            if (const AnimationTrack* secondLayerTrack = secondLayerAnimation->FindTrack(track->GetBoneName()))
            {
                frame = frame.Blend(secondLayerTrack->GetKeyframe(params.secondLayerTime), secondLayerWeight);
            }
        }

        if (overlayAnimation != nullptr && IsBoneOverlaid(*bone, params))
        {
            if (const AnimationTrack* overlayTrack = overlayAnimation->FindTrack(track->GetBoneName()))
            {
                frame = frame.Blend(overlayTrack->GetKeyframe(params.overlayTime), overlayWeight);
            }
        }

        bone->SetKeyframe(bone->GetKeyframe().Blend(frame, blend));
    }

    if (layerAnimation == nullptr)
    {
        return;
    }

    // Bones only the layer animates
    for (const Handle<AnimationTrack>& layerTrack : layerAnimation->GetTracks())
    {
        if (FindTrack(layerTrack->GetBoneName()) != nullptr)
        {
            continue;
        }

        Bone* bone = skeleton.FindBone(layerTrack->GetBoneName());
        if (!bone || (params.layerExcludedBone.IsValid() && IsBoneWithin(*bone, params.layerExcludedBone)))
        {
            continue;
        }

        bone->SetKeyframe(bone->GetKeyframe().Blend(layerTrack->GetKeyframe(params.layerTime), blend * layerWeight));
    }
}

AnimationTrack* Animation::FindTrack(Name boneName) const
{
    for (const Handle<AnimationTrack>& track : m_tracks)
    {
        if (track.IsValid() && track->GetBoneName() == boneName)
        {
            return track.Get();
        }
    }

    return nullptr;
}

#pragma endregion Animation

} // namespace Hyperion
