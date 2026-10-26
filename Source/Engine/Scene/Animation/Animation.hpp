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

#include <Core/Utilities/Span.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Scene/Animation/Keyframe.hpp>

#include <Asset/AssetObject.hpp>
#include <Asset/AssetImportInfo.hpp>

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
    // per-slot multipliers on layerWeight (a BoneMask); empty applies the layer evenly, apart from layerExcludedBone
    Span<const float> layerBoneWeights;

    // optional stuff below
    const Animation* secondLayerAnimation = nullptr;
    float secondLayerTime = 0.0f;
    float secondLayerWeight = 0.0f;
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

    HYP_METHOD(Property = "RotationConvention", Editor = false, NoScriptBindings, Serialize)
    uint32 GetRotationConvention_Serialized() const
    {
        return m_keyframeData.raw != nullptr ? s_currentRotationConvention : m_storedRotationConvention;
    }

    HYP_METHOD(Property = "RotationConvention", NoScriptBindings)
    void SetRotationConvention_Serialized(uint32 rotationConvention)
    {
        m_storedRotationConvention = rotationConvention;
    }

private:
    void ConvertLegacyKeyframes();

    HYP_FIELD()
    Name m_boneName;

    HYP_FIELD()
    BlobDataReference m_keyframeData;

    static constexpr uint32 s_currentRotationConvention = 1;
    uint32 m_storedRotationConvention = 0;
};

HYP_STRUCT()
struct AnimationNotify
{
    HYP_STRUCT_BODY(AnimationNotify);

    HYP_FIELD(Property = "Name", Serialize = true, Editor = true)
    Name name;

    HYP_FIELD(Property = "Time", Serialize = true, Editor = true)
    float time = 0.0f;

    HYP_FIELD(Property = "Duration", Serialize = true, Editor = true)
    float duration = 0.0f;

    HYP_FORCE_INLINE bool operator==(const AnimationNotify& other) const
    {
        return name == other.name && time == other.time && duration == other.duration;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(name);
        hc.Add(time);
        hc.Add(duration);

        return hc;
    }
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

    HYP_METHOD(Property = "Loop")
    bool IsLooping() const
    {
        return m_isLooping;
    }

    HYP_METHOD(Property = "Loop")
    void SetLooping(bool isLooping)
    {
        m_isLooping = isLooping;
    }

    HYP_METHOD(Property = "RootMotion")
    bool HasRootMotion() const
    {
        return m_hasRootMotion;
    }

    HYP_METHOD(Property = "RootMotion")
    void SetRootMotion(bool hasRootMotion)
    {
        m_hasRootMotion = hasRootMotion;
    }

    HYP_FORCE_INLINE const AssetImportInfo& GetImportInfo() const
    {
        return m_importInfo;
    }

    HYP_FORCE_INLINE void SetImportInfo(const AssetImportInfo& importInfo)
    {
        m_importInfo = importInfo;
    }

    // bumped whenever the tracks change, so cached track bindings know to rebuild
    HYP_FORCE_INLINE uint32 GetRevision() const
    {
        return m_revision;
    }

    HYP_METHOD(Property = "Notifies", NoScriptBindings)
    const Array<AnimationNotify>& GetNotifies() const
    {
        return m_notifies;
    }

    HYP_METHOD(Property = "Notifies", NoScriptBindings)
    void SetNotifies(const Array<AnimationNotify>& notifies)
    {
        m_notifies = notifies;
    }

private:
    HYP_FIELD(Property = "Tracks")
    Array<Handle<AnimationTrack>> m_tracks;

    HYP_FIELD(Property = "Notifies")
    Array<AnimationNotify> m_notifies;

    HYP_FIELD(Property = "Loop")
    bool m_isLooping = true;

    HYP_FIELD(Property = "RootMotion")
    bool m_hasRootMotion = false;

    HYP_FIELD(Property = "ImportInfo", Serialize, Editor = false)
    AssetImportInfo m_importInfo;

    uint32 m_revision = 0;
};

} // namespace Hyperion
