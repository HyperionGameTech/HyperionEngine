/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/animation/Keyframe.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/object/HypObject.hpp>

#include <core/object/Handle.hpp>
#include <core/Name.hpp>

#include <core/Types.hpp>

namespace hyperion {

class Bone;
class Skeleton;

HYP_CLASS()
class HYP_API AnimationTrack final : public HypObjectBase
{
    HYP_OBJECT_BODY(AnimationTrack);

public:
    friend class Bone;
    friend class Animation;

    AnimationTrack();
    explicit AnimationTrack(Name boneName);
    AnimationTrack(const AnimationTrack& other) = delete;
    AnimationTrack& operator=(const AnimationTrack& other) = delete;
    ~AnimationTrack() override = default;

    HYP_METHOD()
    Name GetBoneName() const
    {
        return m_boneName;
    }

    HYP_METHOD()
    void SetBoneName(Name boneName)
    {
        m_boneName = boneName;
    }

    HYP_METHOD()
    void AddKeyframe(const Keyframe& keyframe)
    {
        m_keyframes.PushBack(keyframe);
    }

    HYP_METHOD()
    const Array<Keyframe>& GetKeyframes() const
    {
        return m_keyframes;
    }

    HYP_METHOD()
    float GetLength() const;

    HYP_METHOD()
    Keyframe GetKeyframe(float time) const;

private:
    void Init() override;

    HYP_FIELD()
    Name m_boneName;

    HYP_FIELD()
    Array<Keyframe> m_keyframes;
};

HYP_CLASS()
class HYP_API Animation final : public HypObjectBase
{
    HYP_OBJECT_BODY(Animation);

public:
    Animation();
    explicit Animation(Name name);
    Animation(const Animation& other) = delete;
    Animation& operator=(const Animation& other) = delete;
    ~Animation() = default;

    HYP_METHOD(Property = "Name")
    Name GetName() const
    {
        return m_name;
    }

    HYP_METHOD(Property = "Name")
    void SetName(Name name)
    {
        m_name = name;
    }

    HYP_METHOD(Property = "Length", Transient)
    float GetLength() const
    {
        return m_tracks.Empty() ? 0.0f : m_tracks.Back()->GetLength();
    }

    HYP_METHOD()
    void AddTrack(const Handle<AnimationTrack>& track);

    HYP_METHOD(Property = "Tracks")
    const Array<Handle<AnimationTrack>>& GetTracks() const
    {
        return m_tracks;
    }

    HYP_METHOD(Property = "Tracks")
    void SetTracks(const Array<Handle<AnimationTrack>>& tracks);

    HYP_METHOD()
    const Handle<AnimationTrack>& GetTrack(uint32 index) const
    {
        return m_tracks[index];
    }

    HYP_METHOD()
    uint32 NumTracks() const
    {
        return uint32(m_tracks.Size());
    }

    HYP_METHOD()
    void Apply(Skeleton* skeleton, float time);

    HYP_METHOD()
    void ApplyBlended(Skeleton* skeleton, float time, float blend);

private:
    void Init() override;

    HYP_FIELD(Property = "Name")
    Name m_name;

    HYP_FIELD(Property = "Tracks")
    Array<Handle<AnimationTrack>> m_tracks;
};

} // namespace hyperion
