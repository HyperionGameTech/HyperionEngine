/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CAMERA_TRACK_HPP
#define HYPERION_CAMERA_TRACK_HPP

#include <core/math/Transform.hpp>
#include <core/containers/SortedArray.hpp>

namespace hyperion {

struct CameraTrackPivot
{
    double fraction;
    Transform transform;

    bool operator<(const CameraTrackPivot &other) const
        { return fraction < other.fraction; }
};

class HYP_API CameraTrack
{
public:
    CameraTrack(double duration = 10.0);
    CameraTrack(const CameraTrack &other)                   = default;
    CameraTrack &operator=(const CameraTrack &other)        = default;
    CameraTrack(CameraTrack &&other) noexcept               = default;
    CameraTrack &operator=(CameraTrack &&other) noexcept    = default;
    ~CameraTrack()                                          = default;

    double GetDuration() const
        { return m_duration; }

    void SetDuration(double duration)
        { m_duration = duration; }

    /*! \brief Get a blended CameraTrackPivot at \ref{timestamp} */
    CameraTrackPivot GetPivotAt(double timestamp) const;

    void AddPivot(const CameraTrackPivot &pivot);

private:
    double                          m_duration;
    SortedArray<CameraTrackPivot>   m_pivots;
};

} // namespace hyperion

#endif