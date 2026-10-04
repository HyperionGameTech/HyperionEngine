/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

namespace Hyperion {

HYP_ENUM()
enum class DeviceType : uint8
{
    Desktop = 0,
    Laptop = 1,
    Mobile = 2
};

HYP_ENUM()
enum class DeviceGrade : uint8
{
    Low = 0,
    Medium = 1,
    High = 2,
    Ultra = 3
};

} // namespace Hyperion
