#pragma once

#include <core/Defines.hpp>
#include <core/Types.hpp>

namespace hyperion {

class HypClass;

namespace utilities {

struct TypeInfo;
struct TypeId;

const TypeInfo& TypeInfo_Void();
const TypeInfo& TypeInfo_ForHypClass(const HypClass* hypClass);
const TypeId& TypeInfo_GetId(const TypeInfo& type_info);

template <class T>
const TypeInfo& TypeInfo_ForType();

} // namespace utilities

using utilities::TypeInfo;
using utilities::TypeInfo_ForType;
using utilities::TypeInfo_ForHypClass;
using utilities::TypeInfo_GetId;
using utilities::TypeInfo_Void;

} // namespace hyperion
