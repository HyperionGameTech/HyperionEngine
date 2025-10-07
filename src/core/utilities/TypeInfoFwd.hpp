#pragma once

#include <core/Defines.hpp>
#include <core/Types.hpp>

namespace hyperion {
namespace utilities {

struct TypeInfo;
struct TypeId;

const TypeInfo& TypeInfo_Void();
const TypeId& TypeInfo_GetId(const TypeInfo& type_info);

template <class T>
const TypeInfo& TypeInfo_ForType();

} // namespace utilities

using utilities::TypeInfo;
using utilities::TypeInfo_ForType;
using utilities::TypeInfo_GetId;
using utilities::TypeInfo_Void;

} // namespace hyperion
