/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Math/Transform.hpp>
#include <Core/Containers/SortedArray.hpp>

namespace Hyperion {

struct CameraTrackPivot
{
    double fraction;
    Transform transform;

    bool operator<(const CameraTrackPivot& other) const
    {
        return fraction < other.fraction;
    }
};

class ENGINE_API CameraTrack
{
public:
    CameraTrack(double duration = 10.0);
    CameraTrack(const CameraTrack& other) = default;
    CameraTrack& operator=(const CameraTrack& other) = default;
    CameraTrack(CameraTrack&& other) noexcept = default;
    CameraTrack& operator=(CameraTrack&& other) noexcept = default;
    ~CameraTrack() = default;

    double GetDuration() const
    {
        return m_duration;
    }

    void SetDuration(double duration)
    {
        m_duration = duration;
    }

    /*! \brief Get a blended CameraTrackPivot at \p timestamp */
    CameraTrackPivot GetPivotAt(double timestamp) const;

    void AddPivot(const CameraTrackPivot& pivot);

private:
    double m_duration;
    TSortedArray<CameraTrackPivot> m_pivots;
};

} // namespace Hyperion
