/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/camera/CameraTrack.hpp>
#include <core/math/MathUtil.hpp>

namespace hyperion {

CameraTrack::CameraTrack(double duration)
    : m_duration(duration)
{
}

CameraTrackPivot CameraTrack::GetPivotAt(double timestamp) const
{
    const double fraction = timestamp / MathUtil::Max(m_duration, 0.00001);

    int first = 0, second = -1;

    if (m_pivots.Empty())
    {
        return CameraTrackPivot { fraction };
    }

    for (int i = 0; i < int(m_pivots.Size() - 1); i++)
    {
        if (MathUtil::InRange(fraction, { m_pivots[i].fraction, m_pivots[i + 1].fraction }))
        {
            first = i;
            second = i + 1;

            break;
        }
    }

    const CameraTrackPivot& current = m_pivots[first];

    Transform transform = current.transform;

    if (second > first)
    {
        const CameraTrackPivot& next = m_pivots[second];

        const double delta = (fraction - current.fraction) / (next.fraction - current.fraction);

        transform.GetTranslation().Lerp(next.transform.GetTranslation(), float(delta));
        transform.GetRotation().Slerp(next.transform.GetRotation(), float(delta));
        transform.UpdateMatrix();
    }

    return { fraction, transform };
}

void CameraTrack::AddPivot(const CameraTrackPivot& pivot)
{
    m_pivots.Insert(pivot);
}

} // namespace hyperion