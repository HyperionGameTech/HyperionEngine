/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Animation/Keyframe.hpp>

#include <Core/Math/MathUtil.hpp>

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
