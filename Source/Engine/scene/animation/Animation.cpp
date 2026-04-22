/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/Assets.hpp>
#include <asset/BlobStorage.hpp>

#include <scene/animation/Animation.hpp>
#include <scene/animation/Bone.hpp>
#include <scene/animation/Skeleton.hpp>

#include <Core/math/MathUtil.hpp>

#include <Animation.generated.inl>

namespace Hyperion {

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
        Handle<AssetRegistry> registry = GetAssetRegistry();
        AssertDebug(registry.IsValid());

        if (!registry.IsValid())
        {
            return;
        }

        BlobStorage* blobStorage = registry->HasBlobStorage() ? &registry->GetBlobStorage() : nullptr;

        if (!blobStorage || !blobStorage->GetData(m_keyframeData.key, m_keyframeData.size, m_keyframeData.raw))
        {
#if HYP_EDITOR || HYP_ALLOW_INLINE_BLOBS
            // check if failed; if so, try to import from raw data blob in project directory
            FileByteReader stream { registry->GetRootPath() / AssetBuckets::AnimationTracks.GetName() / (String(*GetName()) + ".KEYF.raw.blob") };
            if (!stream.Eof())
            {
                ByteBuffer buffer = stream.Read(stream.Max());

                AllocateBlobData(m_keyframeData, buffer.Data(), buffer.Size(), alignof(Keyframe));

                MarkDirty();

                return;
            }
#endif

            HYP_FAIL("Blob data missing! Data corruption detected.");
        }
        else
        {
            m_keyframeData.readOnly = true;
        }
    }
}

void AnimationTrack::UnpageBlobData()
{
    if (m_keyframeData.readOnly)
    {
        m_keyframeData.raw = nullptr;
    }   
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

void Animation::Init()
{
    HYP_SCOPE;

    for (const Handle<AnimationTrack>& track : m_tracks)
    {
        track->SetPersistentRequested(true);

        InitObject(track);
    }

    SetReady(true);
}

void Animation::AddTrack(const Handle<AnimationTrack>& track)
{
    HYP_SCOPE;

    if (!track)
    {
        return;
    }

    if (IsInitCalled())
    {
        InitObject(track);
    }

    track->SetPersistentRequested(true);

    m_tracks.PushBack(track);
}

void Animation::SetTracks(const Array<Handle<AnimationTrack>>& tracks)
{
    HYP_SCOPE;

    if (IsInitCalled())
    {
        for (const Handle<AnimationTrack>& track : tracks)
        {
            track->SetPersistentRequested(true);

            InitObject(track);
        }
    }

    m_tracks = tracks;
}

void Animation::Apply(Skeleton* skeleton, float time)
{
    HYP_SCOPE;
    Assert(skeleton != nullptr);

    for (const Handle<AnimationTrack>& track : m_tracks)
    {
        Bone* bone = skeleton->FindBone(track->GetBoneName());
        if (!bone)
        {
            continue;
        }

        bone->ClearPose();
        bone->SetKeyframe(track->GetKeyframe(time));
    }
}

void Animation::ApplyBlended(Skeleton* skeleton, float time, float blend)
{
    HYP_SCOPE;
    Assert(skeleton != nullptr);

    for (const Handle<AnimationTrack>& track : m_tracks)
    {
        Bone* bone = skeleton->FindBone(track->GetBoneName());
        if (!bone)
        {
            continue;
        }

        if (blend <= MathUtil::epsilonF)
        {
            bone->ClearPose();
        }

        Keyframe frame = track->GetKeyframe(time);
        Keyframe blended = bone->GetKeyframe().Blend(
            frame,
            MathUtil::Clamp(blend, 0.0f, 1.0f));

        bone->SetKeyframe(blended);
    }
}

#pragma endregion Animation

} // namespace Hyperion
