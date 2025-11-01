#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region ComponentRWFlags Reflection Data

HYP_BEGIN_ENUM(ComponentRWFlags, 352, 0, {})
    HypConstant(NAME(HYP_STR(NONE)), ComponentRWFlags::NONE),
    HypConstant(NAME(HYP_STR(READ)), ComponentRWFlags::READ),
    HypConstant(NAME(HYP_STR(WRITE)), ComponentRWFlags::WRITE),
    HypConstant(NAME(HYP_STR(READ_WRITE)), ComponentRWFlags::READ_WRITE)
HYP_END_ENUM

#pragma endregion ComponentRWFlags Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region ComponentInfo Reflection Data

HYP_BEGIN_STRUCT(ComponentInfo, 353, 0, {}, HypClassAttribute("size", 12))
    HypField(NAME(HYP_STR(TypeId)), &ComponentInfo::typeId, offsetof(ComponentInfo, typeId)),
    HypField(NAME(HYP_STR(RwFlags)), &ComponentInfo::rwFlags, offsetof(ComponentInfo, rwFlags)),
    HypField(NAME(HYP_STR(ReceivesEvents)), &ComponentInfo::receivesEvents, offsetof(ComponentInfo, receivesEvents))
HYP_END_STRUCT

#pragma endregion ComponentInfo Reflection Data

static_assert(sizeof(ComponentInfo) == 12, "Expected sizeof(ComponentInfo) to be 12 bytes");
} // namespace hyperion

