#ifndef HYPERION_V2_ANIMATION_H
#define HYPERION_V2_ANIMATION_H

#include "Keyframe.hpp"

#include <vector>
#include <string>

namespace hyperion::v2 {

class Bone;

struct AnimationTrack {
    Bone                 *bone = nullptr;
    std::string           bone_name;
    std::vector<Keyframe> keyframes;

    float GetLength() const
    {
        return keyframes.empty() ? 0.0f : keyframes.back().GetTime();
    }

    Keyframe GetKeyframe(float time) const;
};

class Animation {
public:
    Animation(const std::string &name);
    Animation(const Animation &other) = default;
    Animation &operator=(const Animation &other) = default;
    Animation(Animation &&other) noexcept = default;
    Animation &operator=(Animation &&other) noexcept = default;
    ~Animation() = default;

    const std::string &GetName() const    { return m_name; }
    void SetName(const std::string &name) { m_name = name; }
     
    float GetLength() const
    {
        return m_tracks.empty() ? 0.0f : m_tracks.back().GetLength();
    }

    void AddTrack(const AnimationTrack &track)         { m_tracks.push_back(track); }
    auto &GetTracks()                                  { return m_tracks; }
    const auto &GetTracks() const                      { return m_tracks; }
    AnimationTrack &GetTrack(size_t index)             { return m_tracks[index]; }
    const AnimationTrack &GetTrack(size_t index) const { return m_tracks[index]; }
    size_t NumTracks() const                           { return m_tracks.size(); }

    void Apply(float time);
    void ApplyBlended(float time, float blend);

private:
    std::string                 m_name;
    std::vector<AnimationTrack> m_tracks;
};

} // namespace hyperion::v2

#endif