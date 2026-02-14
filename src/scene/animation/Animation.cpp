/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ScenePch.hpp>

#include <asset/AssetRegistry.hpp>

#include <scene/animation/Animation.hpp>
#include <scene/animation/Bone.hpp>
#include <scene/animation/Skeleton.hpp>

#include <core/math/MathUtil.hpp>

#include <Animation.generated.inl>

namespace Hyperion {

#pragma region AnimationTrack

AnimationTrack::AnimationTrack()
{
}

AnimationTrack::AnimationTrack(Name boneName)
    : m_boneName(boneName)
{
}

AnimationTrack::~AnimationTrack()
{
    FreeBlobData(m_keyframeData);
}

void AnimationTrack::Init()
{
    HYP_SCOPE;

    ObjectBase::Init();

    SetReady(true);
}

void AnimationTrack::PageBlobData(BlobStorage& blobStorage)
{
    if (m_keyframeData.raw == nullptr
        && m_keyframeData.bufferOffset != InvalidBufferOffset
        && m_keyframeData.size != 0)
    {
        m_keyframeData.raw = blobStorage.Map(m_keyframeData.bufferOffset, m_keyframeData.size);
        m_keyframeData.readOnly = true;
    }
}

void AnimationTrack::UnpageBlobData(BlobStorage& blobStorage)
{
    if (m_keyframeData.readOnly)
    {
        return;
    }

    Assert(m_keyframeData.raw != nullptr);

    if (m_keyframeData.bufferOffset == InvalidBufferOffset)
    {
        BlobHeader header {};
        Memory::Copy(header.magic, "TRAK", 4);
        header.version = 1;
        header.payloadOffset = 0;
        header.payloadSize = m_keyframeData.size;

        Assert(blobStorage.AllocateBlob(header, m_keyframeData.bufferOffset));
    }
        
    ByteWriter* writeStream = blobStorage.GetWriteStream();

    writeStream->Seek(m_keyframeData.bufferOffset);
    writeStream->Write(m_keyframeData.raw, m_keyframeData.size);
}

float AnimationTrack::GetLength() const
{
    if (m_keyframeData.size == 0)
    {
        return 0.0f;
    }

    Span<const Keyframe> keyframes = GetKeyframes();

    return keyframes[keyframes.Size() - 1].time;
}

void AnimationTrack::SetKeyframes(Span<const Keyframe> keyframes)
{
    FreeBlobData(m_keyframeData);
    AllocateBlobData<Keyframe>(m_keyframeData, keyframes);
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

    for (int i = 0; i < int(m_keyframeData.size - 1); i++)
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
    : m_name(name)
{
}

void Animation::Init()
{
    HYP_SCOPE;

    ObjectBase::Init();

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
