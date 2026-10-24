/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#ifdef HYP_TESTS

#include <Framework/Testing/EngineTest.hpp>

#include <Core/Containers/String.hpp>

#include <Core/Utilities/Format.hpp>

#include <limits>

namespace Hyperion {
namespace tests {
namespace core {

namespace {

template <class T>
void CheckToString(EngineTestContext& context, T value, const char* expected)
{
    const String text = String::ToString(value);

    context.Check(expected, text == expected, HYP_FORMAT("got '{}'", text));
}

HYP_ENGINE_TEST(StringFormat, IntegersToString)
{
    CheckToString<int32>(context, 0, "0");
    CheckToString<int32>(context, 7, "7");
    CheckToString<int32>(context, -7, "-7");
    CheckToString<int32>(context, 10, "10");
    CheckToString<int32>(context, 1000000000, "1000000000");
    CheckToString<int32>(context, -1000000000, "-1000000000");
    CheckToString<uint8>(context, 255, "255");
    CheckToString<uint16>(context, 65535, "65535");
    CheckToString<uint32>(context, 4294967295u, "4294967295");
    CheckToString<uint64>(context, 18446744073709551615ull, "18446744073709551615");
}

HYP_ENGINE_TEST(StringFormat, MinimumSignedValuesToString)
{
    CheckToString<int8>(context, std::numeric_limits<int8>::min(), "-128");
    CheckToString<int16>(context, std::numeric_limits<int16>::min(), "-32768");
    CheckToString<int32>(context, std::numeric_limits<int32>::min(), "-2147483648");
    CheckToString<int32>(context, std::numeric_limits<int32>::max(), "2147483647");
    CheckToString<int64>(context, std::numeric_limits<int64>::min(), "-9223372036854775808");
    CheckToString<int64>(context, std::numeric_limits<int64>::max(), "9223372036854775807");
}

HYP_ENGINE_TEST(StringFormat, FormatsIntegerArguments)
{
    HYP_TEST_CHECK(HYP_FORMAT("{}", std::numeric_limits<int32>::min()) == "-2147483648");
    HYP_TEST_CHECK(HYP_FORMAT("{} and {}", -5, 12) == "-5 and 12");
}

} // namespace

} // namespace core
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS
