/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_ANIMATION_H
#define HYPERION_V2_ANIMATION_H

#include <scene/animation/Keyframe.hpp>
#include <core/Containers.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

class Bone;

struct AnimationTrack
{
    Bone            *bone = nullptr;
    String          bone_name;
    Array<Keyframe> keyframes;

    float GetLength() const
        { return keyframes.Empty() ? 0.0f : keyframes.Back().GetTime(); }

    HYP_API Keyframe GetKeyframe(float time) const;
};

class Animation
{
public:
    HYP_API Animation();
    HYP_API Animation(const String &name);
    Animation(const Animation &other)                   = default;
    Animation &operator=(const Animation &other)        = default;
    Animation(Animation &&other) noexcept               = default;
    Animation &operator=(Animation &&other) noexcept    = default;
    ~Animation()                                        = default;

    const String &GetName() const
        { return m_name; }

    void SetName(const String &name)
        { m_name = name; }

    float GetLength() const
        { return m_tracks.Empty() ? 0.0f : m_tracks.Back().GetLength(); }

    void AddTrack(const AnimationTrack &track)
        { m_tracks.PushBack(track); }

    Array<AnimationTrack> &GetTracks()
        { return m_tracks; }

    const Array<AnimationTrack> &GetTracks() const
        { return m_tracks; }

    AnimationTrack &GetTrack(SizeType index)
        { return m_tracks[index]; }

    const AnimationTrack &GetTrack(SizeType index) const
        { return m_tracks[index]; }

    SizeType NumTracks() const
        { return m_tracks.Size(); }

    HYP_API void Apply(float time);
    HYP_API void ApplyBlended(float time, float blend);

private:
    String                  m_name;
    Array<AnimationTrack>   m_tracks;
};

} // namespace hyperion::v2

#endif