/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Math/Transform.hpp>

#ifndef HYP_TOOL
#include <Transform.generated.inl>
#endif

namespace Hyperion {

const Transform Transform::identity {};

Transform::Transform()
    : translation(Vec3f::Zero()),
      scale(Vec3f::One()),
      rotation(Quat4f::Identity())
{
}

Transform::Transform(const Vec3f& translation, const Vec3f& scale)
    : translation(translation),
      scale(scale),
      rotation(Quat4f::Identity())
{
}

Transform::Transform(const Vec3f& translation, const Vec3f& scale, const Quat4f& rotation)
    : translation(translation),
      scale(scale),
      rotation(rotation)
{
}

Transform::Transform(const Vec3f& translation)
    : Transform(translation, Vec3f::One(), Quat4f::Identity())
{
}

Transform Transform::GetInverse() const
{
    return {
        -translation,
        Vec3f(1.0f) / scale,
        rotation.Inverse()
    };
}

Transform Transform::operator*(const Transform& other) const
{
    // Transform::rotation is stored inverted relative to the standard quaternion
    // convention (see Mat4f::Rotation), so composing two transforms needs the
    // rotation applied to the translation, and the quaternion product, reversed
    // to stay consistent with GetMatrix().
    return {
        translation + (rotation.Inverse().RotateVector(scale * other.translation)),
        scale * other.scale,
        other.rotation * rotation
    };
}

Transform& Transform::operator*=(const Transform& other)
{
    return *this = *this * other;
}

} // namespace Hyperion
