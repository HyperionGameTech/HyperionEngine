/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_KEYFRAME_HPP
#define HYPERION_KEYFRAME_HPP

#include <core/math/Transform.hpp>
#include <Types.hpp>

namespace hyperion {

HYP_STRUCT()
class HYP_API Keyframe
{
public:
    Keyframe();
    Keyframe(const Keyframe &other) = default;
    Keyframe &operator=(const Keyframe &other) = default;
    Keyframe(float time, const Transform &transform);

    HYP_METHOD()
    float GetTime() const
        { return m_time; }

    HYP_METHOD()
    void SetTime(float time)
        { m_time = time; }
   
    HYP_METHOD()
    const Transform &GetTransform() const
        { return m_transform; }

    HYP_METHOD()
    void SetTransform(const Transform &transform)
        { m_transform = transform; }

    HYP_METHOD()
    Keyframe Blend(const Keyframe &to, float blend) const;

private:
    float       m_time;
    Transform   m_transform;
};

} // namespace hyperion

#endif