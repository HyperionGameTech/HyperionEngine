/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/animation/Keyframe.hpp>

#include <core/math/MathUtil.hpp>

namespace hyperion {

Keyframe Keyframe::Blend(const Keyframe& to, float blend) const
{
    const float newTime = MathUtil::Lerp(time, to.time, blend);

    Transform newTransform = transform;
    newTransform.translation = newTransform.translation.Lerp(to.transform.translation, blend);
    newTransform.scale = newTransform.scale.Lerp(to.transform.scale, blend);
    newTransform.rotation = newTransform.rotation.Slerp(to.transform.rotation, blend);
    newTransform.UpdateMatrix();

    return { newTime, newTransform };
}

} // namespace hyperion
