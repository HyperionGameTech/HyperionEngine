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

    Float GetLength() const
        { return keyframes.Empty() ? 0.0f : keyframes.Back().GetTime(); }

    Keyframe GetKeyframe(Float time) const;
};

class Animation
{
public:
    Animation();
    Animation(const String &name);
    Animation(const Animation &other) = default;
    Animation &operator=(const Animation &other) = default;
    Animation(Animation &&other) noexcept = default;
    Animation &operator=(Animation &&other) noexcept = default;
    ~Animation() = default;

    const String &GetName() const
        { return m_name; }

    void SetName(const String &name)
        { m_name = name; }

    Float GetLength() const
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

    void Apply(Float time);
    void ApplyBlended(Float time, Float blend);

private:
    String                  m_name;
    Array<AnimationTrack>   m_tracks;
};

} // namespace hyperion::v2

#endif