#ifndef HYPERION_V2_KEYFRAME_H
#define HYPERION_V2_KEYFRAME_H

#include <math/Transform.hpp>

namespace hyperion::v2 {

class Keyframe {
public:
    Keyframe();
    Keyframe(const Keyframe &other);
    Keyframe(float time, const Transform &transform);

    float GetTime() const { return m_time; }
    void SetTime(float time) { m_time = time; }
   
    const Transform &GetTransform() const { return m_transform; }
    void SetTransform(const Transform &transform) { m_transform = transform; }

    Keyframe Blend(const Keyframe &to, float blend) const;

private:
    float     m_time;
    Transform m_transform;
};

} // namespace hyperion::v2

#endif