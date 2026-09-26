/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Types.hpp>
#include <Core/Name/Name.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Scene/Animation/Keyframe.hpp>

#include <Asset/AssetObject.hpp>

namespace Hyperion {

class Bone;
class Skeleton;
class Animation;

struct ApplyAnimParams
{
    float time = 0.0f;
    const Animation* layerAnimation = nullptr;
    float layerTime = 0.0f;
    float layerWeight = 1.0f;
    float blend = 0.5f;
    Name layerExcludedBone;

    // optional stuff below
    const Animation* secondLayerAnimation = nullptr;
    float secondLayerTime = 0.0f;
    float secondLayerWeight = 0.0f;

    // mixed over the result, only on these bones and the bones beneath them
    const Animation* overlayAnimation = nullptr;
    float overlayTime = 0.0f;
    float overlayWeight = 0.0f;
    Name overlayRootBone;
    Name overlaySecondRootBone;
};

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
        float length = 0.0f;

        for (const Handle<AnimationTrack>& track : m_tracks)
        {
            length = MathUtil::Max(length, track->GetLength());
        }

        return length;
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

    void Apply(Skeleton& skeleton, const ApplyAnimParams& params);
    void ApplyBlended(Skeleton& skeleton, const ApplyAnimParams& params);
    void ApplyLayered(Skeleton& skeleton, const ApplyAnimParams& params);

    HYP_METHOD()
    AnimationTrack* FindTrack(Name boneName) const;

private:
    HYP_FIELD(Property = "Tracks")
    Array<Handle<AnimationTrack>> m_tracks;
};

} // namespace Hyperion
