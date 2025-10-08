#pragma once

#include <core/Defines.hpp>
#include <core/Types.hpp>

namespace hyperion {

class HypClass;
struct Name;

namespace utilities {

struct TypeInfo;
struct TypeId;

extern const TypeInfo& TypeInfo_Void();
extern const TypeInfo& TypeInfo_ForHypClass(const HypClass* hypClass);
extern const TypeId& TypeInfo_GetId(const TypeInfo& type_info);
extern const Name& TypeInfo_GetName(const TypeInfo& type_info);

template <class T>
extern const TypeInfo& TypeInfo_ForType();

} // namespace utilities

using utilities::TypeInfo;
using utilities::TypeInfo_ForHypClass;
using utilities::TypeInfo_ForType;
using utilities::TypeInfo_GetId;
using utilities::TypeInfo_GetName;
using utilities::TypeInfo_Void;

} // namespace hyperion
