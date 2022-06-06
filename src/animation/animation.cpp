#include "animation.h"
#include "bone.h"
#include <math/math_util.h>

namespace hyperion::v2 {

Keyframe AnimationTrack::GetKeyframe(float time) const
{
    int first = 0,
        second = -1;

    if (keyframes.empty()) {
        return {time, Transform()};
    }

    for (int i = 0; i < static_cast<int>(keyframes.size() - 1); i++) {
        if (MathUtil::InRange(time, {keyframes[i].GetTime(), keyframes[i + 1].GetTime()})) {
            first  = i;
            second = i + 1;

            break;
        }
    }

    const auto &current = keyframes[first];

    Transform transform = current.GetTransform();

    if (second > first) {
        const auto &next = keyframes[second];

        const float delta = (time - current.GetTime()) / (next.GetTime() - current.GetTime());

        transform.GetTranslation().Lerp(next.GetTransform().GetTranslation(), delta);
        transform.GetRotation().Slerp(next.GetTransform().GetRotation(), delta);
        transform.UpdateMatrix();
    }

    return {time, transform};
}

Animation::Animation(const std::string &name)
    : m_name(name)
{
}

void Animation::Apply(float time)
{
    for (auto &track : m_tracks) {
        if (track.bone == nullptr) {
            continue;
        }

        track.bone->ClearPose();
        track.bone->SetKeyframe(track.GetKeyframe(time));
    }
}

void Animation::ApplyBlended(float time, float blend)
{
    for (auto &track : m_tracks) {
        if (track.bone == nullptr) {
            continue;
        }

        if (blend <= MathUtil::epsilon<float>) {
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
