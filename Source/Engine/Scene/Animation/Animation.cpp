/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/Assets.hpp>
#include <Asset/BlobStorage.hpp>

#include <Scene/Animation/Animation.hpp>
#include <Scene/Animation/Bone.hpp>
#include <Scene/Animation/Skeleton.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/IO/ByteWriter.hpp>
#include <Core/IO/ByteReader.hpp>

#include <Animation.generated.inl>

namespace Hyperion {

#pragma region AnimationTrack

AnimationTrack::AnimationTrack()
{
}

AnimationTrack::AnimationTrack(Name name, Name boneName)
    : AssetObject(name),
      m_boneName(boneName)
{
}

AnimationTrack::~AnimationTrack()
{
    FreeBlobData(m_keyframeData);
}

void AnimationTrack::PageBlobData()
{
    if (m_keyframeData.raw == nullptr
        && m_keyframeData.key
        && m_keyframeData.size != 0)
    {
        Handle<AssetRegistry> registry = GetAssetRegistry();
        AssertDebug(registry.IsValid());

        if (!registry.IsValid())
        {
            return;
        }

        if (EngineGlobals::IsCooking() || EngineGlobals::IsEditor() || !EngineGlobals::GetBlobStorage()->GetData(m_keyframeData.key, m_keyframeData.size, m_keyframeData.raw))
        {
            // check if failed; if so, try to import from raw data blob in project directory
            const Name blobKey = m_keyframeData.key;
            const uint64 expectedSize = m_keyframeData.size;

            FileByteReader stream { registry->GetRootPath() / AssetBuckets::AnimationTracks.GetName() / (String(*GetName()) + ".KEYF.raw.blob") };
            if (!stream.Eof())
            {
                if (stream.Max() != expectedSize)
                {
                    HYP_LOG(Engine, Error, "Local blob data for animation track '{}' keyframes is {} bytes but the manifest expects {}, ignoring it",
                            GetName(), stream.Max(), expectedSize);

                    return;
                }

                ByteBuffer buffer = stream.Read(stream.Max());

                AllocateBlobData(m_keyframeData, buffer.Data(), buffer.Size(), alignof(Keyframe));
                m_keyframeData.key = blobKey;

                return;
            }
            
            HYP_LOG(Engine, Error, "Data corruption detected for {} due to missing blob data", GetPath().ToString());
        }
        else
        {
            m_keyframeData.readOnly = true;
        }
    }
}

void AnimationTrack::UnpageBlobData()
{
    if (m_keyframeData.readOnly)
    {
        m_keyframeData.raw = nullptr;
    }
}

float AnimationTrack::GetLength() const
{
    if (m_keyframeData.size == 0)
    {
        return 0.0f;
    }

    Span<const Keyframe> keyframes = GetKeyframes();
    AssertDebug(keyframes.Size() > 0);

    if (HYP_UNLIKELY(keyframes.Size() == 0))
    {
        return 0.0f;
    }

    return keyframes[keyframes.Size() - 1].time;
}

void AnimationTrack::SetKeyframes(Span<const Keyframe> keyframes)
{
    FreeBlobData(m_keyframeData);
    AllocateBlobData(m_keyframeData, keyframes.Data(), sizeof(Keyframe) * keyframes.Size());
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

    if (HYP_UNLIKELY(keyframes.Size() == 0))
    {
        return { time, Transform() };
    }

    for (int i = 0; i < int(keyframes.Size()) - 1; i++)
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
    : AssetObject(name)
{
}

void Animation::AddTrack(const Handle<AnimationTrack>& track)
{
    HYP_SCOPE;

    if (!track)
    {
        return;
    }

    track->SetPersistentRequested(true);

    m_tracks.PushBack(track);
}

void Animation::SetTracks(const Array<Handle<AnimationTrack>>& tracks)
{
    for (const Handle<AnimationTrack>& track : tracks)
    {
        track->SetPersistentRequested(true);
    }

    m_tracks = tracks;
}

void Animation::Apply(Skeleton* skeleton, float time)
{
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
