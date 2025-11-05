#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region CommandLineArgumentDefinitions Reflection Data

HYP_BEGIN_STRUCT(CommandLineArgumentDefinitions, 234, 0, {}, ClassAttribute("size", 8))
HYP_END_STRUCT

#pragma endregion CommandLineArgumentDefinitions Reflection Data

static_assert(sizeof(CommandLineArgumentDefinitions) == 8, "Expected sizeof(CommandLineArgumentDefinitions) to be 8 bytes");
} // namespace hyperion


namespace hyperion {

#pragma region CommandLineArguments Reflection Data

HYP_BEGIN_STRUCT(CommandLineArguments, 235, 0, {})
HYP_END_STRUCT

#pragma endregion CommandLineArguments Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region CommandLineArgumentType Reflection Data

HYP_BEGIN_ENUM(CommandLineArgumentType, 236, 0, {})
    StaticField(NAME(HYP_STR(STRING)), CommandLineArgumentType::STRING),
    StaticField(NAME(HYP_STR(INTEGER)), CommandLineArgumentType::INTEGER),
    StaticField(NAME(HYP_STR(FLOAT)), CommandLineArgumentType::FLOAT),
    StaticField(NAME(HYP_STR(BOOLEAN)), CommandLineArgumentType::BOOLEAN),
    StaticField(NAME(HYP_STR(ENUM)), CommandLineArgumentType::ENUM)
HYP_END_ENUM

#pragma endregion CommandLineArgumentType Reflection Data

} // namespace hyperion

