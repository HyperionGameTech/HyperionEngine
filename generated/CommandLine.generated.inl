#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region CommandLineArgumentDefinitions Reflection Data

HYP_BEGIN_STRUCT(CommandLineArgumentDefinitions, 233, 0, {}, HypClassAttribute("size", 8))
HYP_END_STRUCT

#pragma endregion CommandLineArgumentDefinitions Reflection Data

static_assert(sizeof(CommandLineArgumentDefinitions) == 8, "Expected sizeof(CommandLineArgumentDefinitions) to be 8 bytes");
} // namespace hyperion


namespace hyperion {

#pragma region CommandLineArguments Reflection Data

HYP_BEGIN_STRUCT(CommandLineArguments, 234, 0, {})
HYP_END_STRUCT

#pragma endregion CommandLineArguments Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region CommandLineArgumentType Reflection Data

HYP_BEGIN_ENUM(CommandLineArgumentType, 235, 0, {})
    HypConstant(NAME(HYP_STR(STRING)), CommandLineArgumentType::STRING),
    HypConstant(NAME(HYP_STR(INTEGER)), CommandLineArgumentType::INTEGER),
    HypConstant(NAME(HYP_STR(FLOAT)), CommandLineArgumentType::FLOAT),
    HypConstant(NAME(HYP_STR(BOOLEAN)), CommandLineArgumentType::BOOLEAN),
    HypConstant(NAME(HYP_STR(ENUM)), CommandLineArgumentType::ENUM)
HYP_END_ENUM

#pragma endregion CommandLineArgumentType Reflection Data

} // namespace hyperion

