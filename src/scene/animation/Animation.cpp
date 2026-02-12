/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ScenePch.hpp>

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

void AnimationTrack::Init()
{
    HYP_SCOPE;

    ObjectBase::Init();

    SetReady(true);
}

float AnimationTrack::GetLength() const
{
    if (m_keyframes.Empty())
    {
        return 0.0f;
    }

    return m_keyframes.Back().time;
}

Keyframe AnimationTrack::GetKeyframe(float time) const
{
    HYP_SCOPE;

    int first = 0, second = -1;

    if (m_keyframes.Empty())
    {
        return { time, Transform() };
    }

    for (int i = 0; i < int(m_keyframes.Size() - 1); i++)
    {
        if (MathUtil::InRange(time, { m_keyframes[i].time, m_keyframes[i + 1].time }))
        {
            first = i;
            second = i + 1;

            break;
        }
    }

    const Keyframe& current = m_keyframes[first];

    Transform transform = current.transform;

    if (second > first)
    {
        const Keyframe& next = m_keyframes[second];

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

    m_tracks.PushBack(track);
}

void Animation::SetTracks(const Array<Handle<AnimationTrack>>& tracks)
{
    HYP_SCOPE;

    if (IsInitCalled())
    {
        for (const Handle<AnimationTrack>& track : tracks)
        {
            InitObject(track);
        }
    }

    m_tracks = tracks;
}

#pragma endregion Animation

#pragma region AnimTrack

AnimKeyframe AnimTrack::GetKeyframeAtTime(float time) const
{
    int first = 0, second = -1;

    if (numKeyframes == 0)
    {
        return { time, Transform() };
    }

    for (int i = 0; i < int(numKeyframes - 1); i++)
    {
        if (MathUtil::InRange(time, { keyframes[i].time, keyframes[i + 1].time }))
        {
            first = i;
            second = i + 1;

            break;
        }
    }

    const AnimKeyframe& current = keyframes[first];

    Transform transform = current.transform;

    if (second > first)
    {
        const AnimKeyframe& next = keyframes[second];

        const float delta = (time - current.time) / (next.time - current.time);

        transform.translation = transform.translation.Lerp(next.transform.translation, delta);
        transform.rotation = transform.rotation.Slerp(next.transform.rotation, delta);
    }

    return { time, transform };
}

#pragma endregion AnimTrack

} // namespace Hyperion
