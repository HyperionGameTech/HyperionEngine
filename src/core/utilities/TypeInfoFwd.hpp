#pragma once

#include <core/Types.hpp>

namespace hyperion {
namespace utilities {

struct TypeInfo;

// Non-template accessor used by headers that can't/shouldn't include the full TypeInfo.hpp
const TypeInfo& TypeInfo_Void();

template <class T>
const TypeInfo& TypeInfo_ForType();

} // namespace utilities

using utilities::TypeInfo;
using utilities::TypeInfo_ForType;
using utilities::TypeInfo_Void;

} // namespace hyperion
