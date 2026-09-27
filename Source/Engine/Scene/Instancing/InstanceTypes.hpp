/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Math/Transform.hpp>

#include <Core/Util.hpp>

namespace Hyperion {

enum class InstanceId : uint32;
static constexpr InstanceId InvalidInstanceId = Invalid<InstanceId>;

///Serialized
struct InstanceRecord
{
    uint32 id;
    float translation[3];
    float rotation[4];
    float scale[3];
    uint32 flags;

    static InstanceRecord FromTransform(InstanceId id, const Transform& transform)
    {
        const Vec3f& t = transform.GetTranslation();
        const Quat4f& r = transform.GetRotation();
        const Vec3f& s = transform.GetScale();

        return InstanceRecord {
            uint32(id),
            { t.x, t.y, t.z },
            { r.x, r.y, r.z, r.w },
            { s.x, s.y, s.z },
            0
        };
    }

    InstanceId GetId() const
    {
        return InstanceId(id);
    }

    Transform GetTransform() const
    {
        return Transform(
            Vec3f(translation[0], translation[1], translation[2]),
            Vec3f(scale[0], scale[1], scale[2]),
            Quat4f(rotation[0], rotation[1], rotation[2], rotation[3]));
    }
};

} // namespace Hyperion
