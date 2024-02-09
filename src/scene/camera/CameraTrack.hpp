#ifndef HYPERION_V2_CAMERA_TRACK_HPP
#define HYPERION_V2_CAMERA_TRACK_HPP

#include <math/Transform.hpp>
#include <core/lib/SortedArray.hpp>
#include <core/lib/Optional.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

struct CameraTrackPivot
{
    double fraction;
    Transform transform;

    bool operator<(const CameraTrackPivot &other) const
        { return fraction < other.fraction; }
};

class CameraTrack
{
public:
    CameraTrack(double duration = 10.0);
    CameraTrack(const CameraTrack &other) = default;
    CameraTrack &operator=(const CameraTrack &other) = default;
    CameraTrack(CameraTrack &&other) noexcept = default;
    CameraTrack &operator=(CameraTrack &&other) noexcept = default;
    ~CameraTrack() = default;

    double GetDuration() const
        { return m_duration; }

    void SetDuration(double duration)
        { m_duration = duration; }

    /**
     * \brief Get a blended CameraTrackPivot at {timestamp}
     */
    CameraTrackPivot GetPivotAt(double timestamp) const;

    void AddPivot(const CameraTrackPivot &pivot);

private:
    double                          m_duration;
    SortedArray<CameraTrackPivot>   m_pivots;
};

} // namespace hyperion::v2

#endif