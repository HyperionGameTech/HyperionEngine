/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <ScenePch.hpp>

#include <scene/animation/Keyframe.hpp>

#include <Core/math/MathUtil.hpp>

#include <Keyframe.generated.inl>

namespace Hyperion {

Keyframe Keyframe::Blend(const Keyframe& to, float blend) const
{
    const float newTime = MathUtil::Lerp(time, to.time, blend);

    Transform newTransform = transform;
    newTransform.translation = newTransform.translation.Lerp(to.transform.translation, blend);
    newTransform.scale = newTransform.scale.Lerp(to.transform.scale, blend);
    newTransform.rotation = newTransform.rotation.Slerp(to.transform.rotation, blend);

    return { newTime, newTransform };
}

} // namespace Hyperion
