#include "Animation.hpp"
#include "Bone.hpp"
#include <math/MathUtil.hpp>

namespace hyperion::v2 {

Keyframe AnimationTrack::GetKeyframe(float time) const
{
    int first = 0, second = -1;

    if (keyframes.Empty()) {
        return { time, Transform() };
    }

    for (int i = 0; i < int(keyframes.Size() - 1); i++) {
        if (MathUtil::InRange(time, { keyframes[i].GetTime(), keyframes[i + 1].GetTime() })) {
            first = i;
            second = i + 1;

            break;
        }
    }

    const Keyframe &current = keyframes[first];

    Transform transform = current.GetTransform();

    if (second > first) {
        const Keyframe &next = keyframes[second];

        const float delta = (time - current.GetTime()) / (next.GetTime() - current.GetTime());

        transform.GetTranslation().Lerp(next.GetTransform().GetTranslation(), delta);
        transform.GetRotation().Slerp(next.GetTransform().GetRotation(), delta);
        transform.UpdateMatrix();
    }

    return { time, transform };
}

Animation::Animation() = default;

Animation::Animation(const String &name)
    : m_name(name)
{
}

void Animation::Apply(float time)
{
    for (AnimationTrack &track : m_tracks) {
        if (track.bone == nullptr) {
            continue;
        }

        track.bone->ClearPose();
        track.bone->SetKeyframe(track.GetKeyframe(time));
    }
}

void Animation::ApplyBlended(float time, float blend)
{
    for (AnimationTrack &track : m_tracks) {
        if (track.bone == nullptr) {
            continue;
        }

        if (blend <= MathUtil::epsilon_f) {
            track.bone->ClearPose();
        }

        auto frame = track.GetKeyframe(time);
        auto blended = track.bone->GetKeyframe().Blend(
            frame, 
            MathUtil::Clamp(blend, 0.0f, 1.0f)
        );

        track.bone->SetKeyframe(blended);
    }
}

} // namespace hyperion::v2
