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

HYP_ENGINE_TEST(StringFormat, FormatIntoAppendsToExistingString)
{
    String buffer("Tris: ");
    HYP_FORMAT_INTO(buffer, "{}, {} and {}", 42, -7, true);

    HYP_TEST_CHECK(buffer == "Tris: 42, -7 and true");
}

HYP_ENGINE_TEST(StringFormat, FormatIntoHandlesExtremeIntegers)
{
    String buffer;
    HYP_FORMAT_INTO(buffer, "{}|{}|{}", std::numeric_limits<int64>::min(), std::numeric_limits<uint64>::max(), std::numeric_limits<int8>::min());

    HYP_TEST_CHECK(buffer == "-9223372036854775808|18446744073709551615|-128");
}

HYP_ENGINE_TEST(StringFormat, FormatIntoMatchesFormat)
{
    String buffer;
    HYP_FORMAT_INTO(buffer, "{} [{}]: {} {} {}", "Name", 0, 1.5f, uint8(255), 'c');

    HYP_TEST_CHECK(buffer == HYP_FORMAT("{} [{}]: {} {} {}", "Name", 0, 1.5f, uint8(255), 'c'));
}

HYP_ENGINE_TEST(StringFormat, FormatIntoWriterReturnsFullSizeWhenTruncated)
{
    char buffer[8];
    utilities::FormatBufferWriter writer(buffer, sizeof(buffer));
    const size_t size = HYP_FORMAT_INTO(writer, "{} {}", "abcdefghij", 12345);

    HYP_TEST_CHECK(size == 16);
    HYP_TEST_CHECK(UTF8StringView(buffer, sizeof(buffer)) == "abcdefgh");
}

HYP_ENGINE_TEST(StringFormat, FormatIntoWriterWritesWindowAtOffset)
{
    char buffer[8];
    utilities::FormatBufferWriter writer(buffer, sizeof(buffer), 8);
    const size_t size = HYP_FORMAT_INTO(writer, "{} {}", "abcdefghij", 12345);

    HYP_TEST_CHECK(size == 16);
    HYP_TEST_CHECK(UTF8StringView(buffer, sizeof(buffer)) == "ij 12345");
}

HYP_ENGINE_TEST(StringFormat, FormatIntoWindowsReassembleToFullOutput)
{
    String result;
    size_t offset = 0;
    size_t total = 0;

    do
    {
        char window[5];
        utilities::FormatBufferWriter writer(window, sizeof(window), offset);
        total = HYP_FORMAT_INTO(writer, "{} {}", "abcdefghij", 12345);

        const size_t windowSize = std::min(sizeof(window), total - offset);
        result.Append(window, window + windowSize);
        offset += windowSize;
    } while (offset < total);

    HYP_TEST_CHECK(result == "abcdefghij 12345");
}

} // namespace

} // namespace core
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS
