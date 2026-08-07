/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

namespace Hyperion {

HYP_STRUCT()
struct CookManifest
{
    HYP_STRUCT_BODY(CookManifest)

    HYP_FIELD()
    uint64 cook_timestamp_ms;

    HYP_FIELD()
    Array<String> files;
};

} // namespace Hyperion