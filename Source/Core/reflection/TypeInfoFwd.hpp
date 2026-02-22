#pragma once

#include <core/Defines.hpp>
#include <core/Types.hpp>

namespace Hyperion {

class Class;
struct Name;

namespace utilities {

struct TypeInfo;
struct TypeId;

extern const TypeInfo& TypeInfo_Void();
extern const TypeInfo& TypeInfo_ForClass(const Class* cls);
extern const TypeId& TypeInfo_GetId(const TypeInfo& typeInfo);
extern const Name& TypeInfo_GetName(const TypeInfo& typeInfo);
extern SizeType TypeInfo_GetSize(const TypeInfo& typeInfo);
const Class* TypeInfo_GetClass(const TypeInfo& typeInfo);

template <class T>
extern const TypeInfo& TypeOf();

} // namespace utilities

using utilities::TypeInfo;
using utilities::TypeInfo_ForClass;
using utilities::TypeInfo_GetClass;
using utilities::TypeInfo_GetId;
using utilities::TypeInfo_GetName;
using utilities::TypeInfo_GetSize;
using utilities::TypeInfo_Void;
using utilities::TypeOf;

} // namespace Hyperion
