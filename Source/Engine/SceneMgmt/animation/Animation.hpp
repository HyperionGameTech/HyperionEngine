/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Types.hpp>
#include <Core/name/Name.hpp>

#include <Core/io/ByteWriter.hpp>
#include <Core/io/ByteReader.hpp>

#include <Core/containers/Array.hpp>
#include <Core/containers/String.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <scene/animation/Keyframe.hpp>

#include <asset/AssetObject.hpp>

namespace Hyperion {

class Bone;
class Skeleton;

HYP_CLASS(AssetBucket = "AnimationTracks")
class ENGINE_API AnimationTrack final : public AssetObject
{
    HYP_OBJECT_BODY(AnimationTrack);

public:
    friend class Bone;
    friend class Animation;

    AnimationTrack();
    AnimationTrack(Name name, Name boneName);

    AnimationTrack(const AnimationTrack& other) = delete;
    AnimationTrack& operator=(const AnimationTrack& other) = delete;

    AnimationTrack(AnimationTrack&& other) noexcept = default;
    AnimationTrack& operator=(AnimationTrack&& other) noexcept = default;

    ~AnimationTrack() override;

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

    HYP_FORCE_INLINE Span<const Keyframe> GetKeyframes() const
    {
        AssertDebug(m_keyframeData.raw != nullptr, "Keyframe data not loaded!");

        if (!m_keyframeData.raw)
        {
            return {};
        }

        return Span<const Keyframe>(reinterpret_cast<const Keyframe*>(m_keyframeData.raw), m_keyframeData.size / sizeof(Keyframe));
    }

    void SetKeyframes(Span<const Keyframe> keyframes);

    HYP_METHOD()
    float GetLength() const;

    HYP_METHOD()
    Keyframe GetKeyframe(float time) const;

protected:
    void PageBlobData() override;
    void UnpageBlobData() override;

    void CollectBlobDataReferences(Array<Tuple<const char*, uint16, BlobDataReference*>>& outReferences) override
    {
        outReferences.EmplaceBack("KEYF", 1, &m_keyframeData);
    }

private:
    HYP_FIELD()
    Name m_boneName;

    HYP_FIELD()
    BlobDataReference m_keyframeData;
};

HYP_CLASS(AssetBucket = "Animations")
class ENGINE_API Animation final : public AssetObject
{
    HYP_OBJECT_BODY(Animation);

public:
    Animation();
    explicit Animation(Name name);
    Animation(const Animation& other) = delete;
    Animation& operator=(const Animation& other) = delete;
    ~Animation() = default;

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

    void Init() override;

private:
    HYP_FIELD(Property = "Tracks")
    Array<Handle<AnimationTrack>> m_tracks;
};

} // namespace Hyperion
