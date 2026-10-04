/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/Containers/Map.hpp>

#include <Core/Reflection/TypeId.hpp>

#include <Framework/DeviceDetails.hpp>

namespace Hyperion {

struct DeviceFacts
{
    bool isMobilePlatform = false;
    bool hasBattery = false;
    uint32 logicalCores = 0;
    uint64 systemMemoryBytes = 0;

    bool hasGpu = false;
    GpuInfo gpu;

    int score = 0;

    Map<TypeId, uint64> resolvedAxes;
    Map<TypeId, bool> forcedAxes;

    template <class EnumType>
    HYP_FORCE_INLINE bool IsResolved() const
    {
        return resolvedAxes.Contains(TypeId::ForType<EnumType>());
    }

    template <class EnumType>
    HYP_FORCE_INLINE EnumType Get() const
    {
        const auto it = resolvedAxes.Find(TypeId::ForType<EnumType>());

        return it != resolvedAxes.End() ? EnumType(it->second) : EnumType(0);
    }

    HYP_FORCE_INLINE bool TryGetResolved(TypeId axisTypeId, uint64& outValue) const
    {
        const auto it = resolvedAxes.Find(axisTypeId);

        if (it == resolvedAxes.End())
        {
            return false;
        }

        outValue = it->second;

        return true;
    }
};

} // namespace Hyperion
