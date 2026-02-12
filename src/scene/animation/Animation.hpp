/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>
#include <core/Name.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

#include <scene/animation/Keyframe.hpp>

#include <asset/BlobStorageStructs.hpp>
#include <asset/BlobBuilder.hpp>

namespace Hyperion {

class Bone;
class Skeleton;

HYP_CLASS()
class HYP_API AnimationTrack final : public ObjectBase
{
    HYP_OBJECT_BODY(AnimationTrack);

public:
    friend class Bone;
    friend class Animation;

    AnimationTrack();
    explicit AnimationTrack(Name boneName);

    AnimationTrack(const AnimationTrack& other) = delete;
    AnimationTrack& operator=(const AnimationTrack& other) = delete;

    AnimationTrack(AnimationTrack&& other) noexcept = default;
    AnimationTrack& operator=(AnimationTrack&& other) noexcept = default;

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

struct AnimTrack
{
    static constexpr const char Header[] = "ATRK";
    static constexpr uint8 Version = 1;

    StringHash bone;
    uint32 numKeyframes;
    BlobPointer<AnimKeyframe> keyframes;

    AnimTrack() = default;

    float GetLength() const
    {
        return numKeyframes == 0 ? 0.0f : keyframes[numKeyframes - 1].time;
    }

    AnimKeyframe GetKeyframeAtTime(float time) const;

    static HYP_NODISCARD AnimTrack* Allocate(const AnimTrack& other, BlobHeader& outHeader)
    {
        return Allocate(
            other.bone,
            other.numKeyframes > 0
                ? Span<const AnimKeyframe>(&other.keyframes[0], other.numKeyframes)
                : Span<const AnimKeyframe>(),
            outHeader);
    }

    static HYP_NODISCARD AnimTrack* Allocate(StringHash bone, Span<const AnimKeyframe> keyframes, BlobHeader& outHeader)
    {
        AnimTrack data {};
        data.bone = bone;
        data.numKeyframes = uint32(keyframes.Size());
       
        TInlineBlobBuilder<AnimTrack> builder(&data);

        return builder
            .Append(offsetof(AnimTrack, keyframes), keyframes.ToSpan())
            .Build(outHeader);
    }
};

struct AnimData
{
    static constexpr const char Header[] = "ANIM";
    static constexpr uint8 Version = 1;

    StringHash animName;
    uint32 numTracks;
    BlobPointer<AnimTrack> tracks;

    AnimData() = default;

    float GetLength() const
    {
        return numTracks == 0 ? 0.0f : tracks[numTracks - 1].GetLength();
    }

    static HYP_NODISCARD AnimData* Allocate(const AnimData& other, BlobHeader& outHeader)
    {
        return Allocate(
            other.animName,
            other.numTracks > 0
                ? Span<const AnimTrack>(&other.tracks[0], other.numTracks)
                : Span<const AnimTrack>(),
            outHeader);
    }

    static HYP_NODISCARD AnimData* Allocate(StringHash animName, Span<const AnimTrack> tracks, BlobHeader& outHeader)
    {
        AnimData data {};
        data.animName = animName;
        data.numTracks = uint32(tracks.Size());
       
        TInlineBlobBuilder<AnimData> builder(&data);

        return builder
            .Append(offsetof(AnimData, tracks), tracks.ToSpan())
            .Build(outHeader);
    }
};

HYP_CLASS()
class HYP_API Animation final : public ObjectBase
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

private:
    void Init() override;

    HYP_FIELD(Property = "Name")
    Name m_name;

    HYP_FIELD(Property = "Tracks")
    Array<Handle<AnimationTrack>> m_tracks;
};

} // namespace Hyperion
