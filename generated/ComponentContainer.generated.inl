#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region ComponentRWFlags Reflection Data

HYP_BEGIN_ENUM(ComponentRWFlags, 347, 0, {})
    StaticField(NAME(HYP_STR(NONE)), ComponentRWFlags::NONE),
    StaticField(NAME(HYP_STR(READ)), ComponentRWFlags::READ),
    StaticField(NAME(HYP_STR(WRITE)), ComponentRWFlags::WRITE),
    StaticField(NAME(HYP_STR(READ_WRITE)), ComponentRWFlags::READ_WRITE)
HYP_END_ENUM

#pragma endregion ComponentRWFlags Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region ComponentInfo Reflection Data

HYP_BEGIN_STRUCT(ComponentInfo, 348, 0, {}, ClassAttribute("size", 12))
    Field(NAME(HYP_STR(TypeId)), &ComponentInfo::typeId, offsetof(ComponentInfo, typeId)),
    Field(NAME(HYP_STR(RwFlags)), &ComponentInfo::rwFlags, offsetof(ComponentInfo, rwFlags)),
    Field(NAME(HYP_STR(ReceivesEvents)), &ComponentInfo::receivesEvents, offsetof(ComponentInfo, receivesEvents))
HYP_END_STRUCT

#pragma endregion ComponentInfo Reflection Data

static_assert(sizeof(ComponentInfo) == 12, "Expected sizeof(ComponentInfo) to be 12 bytes");
} // namespace hyperion

