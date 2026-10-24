/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#ifdef HYP_TESTS

#include <Framework/Testing/EngineTest.hpp>

#include <Core/Containers/String.hpp>

#include <Core/Utilities/Format.hpp>
#include <Core/Utilities/FormatBufferWriter.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <Core/Math/Mat4f.hpp>
#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Logging/Logger.hpp>

#include <Core/Unicode.hpp>

#include <limits>
#include <string_view>
#include <type_traits>

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

class RecordingByteWriter final : public ByteWriter
{
public:
    virtual size_t Position() const override
    {
        return m_text.Size();
    }

    virtual void Seek(size_t position, bool truncate = false) override
    {
        (void)position;
        (void)truncate;
    }

    virtual void Close() override
    {
        m_text.Clear();
    }

    virtual void Flush() override
    {
        // do nothing
    }

    const String& GetText() const
    {
        return m_text;
    }

protected:
    virtual void WriteBytes(const char* ptr, size_t size) override
    {
        m_text.Append(ptr, ptr + size);
    }

private:
    String m_text;
};

struct CapturedLog
{
    const logging::LogChannel* channel = nullptr;
    Array<String> messages;
};

bool CaptureLogMessage(void* context, const logging::LogChannel& channel, const logging::LogMessage& message)
{
    CapturedLog* captured = static_cast<CapturedLog*>(context);

    if (&channel != captured->channel)
    {
        return true;
    }

    String text;

    for (const auto& chunk : message.chunks)
    {
        text.Append(chunk);
    }

    captured->messages.PushBack(std::move(text));

    return false;
}

template <class Arg>
Array<String> LogAndCapture(const Arg& arg)
{
    logging::Logger& logger = logging::GetLogger();

    logging::LogChannel channel(NAME("StringFormatLogTest"));
    logging::DynamicLogChannelHandle channelHandle = logger.CreateDynamicLogChannel(channel);

    CapturedLog captured;
    captured.channel = &channel;

    const int redirectId = logger.AddRedirect(channel.maskBitset, &captured, CaptureLogMessage, CaptureLogMessage);

    logging::LogStatic_Channel<logging::LogLevel::Info, HYP_STATIC_STRING("{}\n"), HYP_STATIC_STRING(""), 0>(logger, channel, arg);

    logger.RemoveRedirect(redirectId);

    return std::move(captured.messages);
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
    HYP_TEST_CHECK(std::string_view(buffer, sizeof(buffer)) == "abcdefgh");
}

HYP_ENGINE_TEST(StringFormat, FormatIntoWriterWritesWindowAtOffset)
{
    char buffer[8];
    utilities::FormatBufferWriter writer(buffer, sizeof(buffer), 8);
    const size_t size = HYP_FORMAT_INTO(writer, "{} {}", "abcdefghij", 12345);

    HYP_TEST_CHECK(size == 16);
    HYP_TEST_CHECK(std::string_view(buffer, sizeof(buffer)) == "ij 12345");
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

HYP_ENGINE_TEST(StringFormat, FormatBufferWriterIsAByteWriter)
{
    HYP_TEST_CHECK((std::is_base_of_v<ByteWriter, utilities::FormatBufferWriter>));
}

HYP_ENGINE_TEST(StringFormat, FormatIntoWorksWithMemoryByteWriter)
{
    MemoryByteWriter<DynamicAllocator> writer;
    const size_t size = HYP_FORMAT_INTO(writer, "{} {} {}", "abc", -42, 'z');

    const auto view = writer.GetBuffer().ToByteView();

    HYP_TEST_CHECK(size == 9);
    HYP_TEST_CHECK(std::string_view(reinterpret_cast<const char*>(view.Data()), size) == "abc -42 z");
}

HYP_ENGINE_TEST(StringFormat, FormatIntoWorksWithCustomByteWriter)
{
    RecordingByteWriter writer;
    const size_t size = HYP_FORMAT_INTO(writer, "{} {}", 7, true);

    HYP_TEST_CHECK(size == 6);
    HYP_TEST_CHECK(writer.GetText() == "7 true");
}

HYP_ENGINE_TEST(StringFormat, FormatIntoWithByteWriterWindowAtOffset)
{
    char buffer[4];
    utilities::FormatBufferWriter writer(buffer, sizeof(buffer), 4);
    const size_t size = HYP_FORMAT_INTO(writer, "{} {}", "abcdefghij", 12345);

    HYP_TEST_CHECK(size == 16);
    HYP_TEST_CHECK(std::string_view(buffer, sizeof(buffer)) == "efgh");
}

HYP_ENGINE_TEST(StringFormat, FormatIntoStringViewSliceStopsAtSliceEnd)
{
    const char text[] = "hello world";
    const utilities::StringView<containers::StringType::UTF8> slice(text, text + 5);

    String buffer("[");
    HYP_FORMAT_INTO(buffer, "{}]", slice);

    HYP_TEST_CHECK(buffer == "[hello]");
}

HYP_ENGINE_TEST(StringFormat, FormatIntoStringViewSliceInWindow)
{
    const char text[] = "hello world";
    const utilities::StringView<containers::StringType::UTF8> slice(text, text + 5);

    char buffer[4];
    utilities::FormatBufferWriter writer(buffer, sizeof(buffer), 1);
    const size_t size = HYP_FORMAT_INTO(writer, "[{}]", slice);

    HYP_TEST_CHECK(size == 7);
    HYP_TEST_CHECK(std::string_view(buffer, sizeof(buffer)) == "hell");
}

HYP_ENGINE_TEST(StringFormat, FormatIntoEmptyStringViewSliceWritesNothing)
{
    const char text[] = "hello";
    const utilities::StringView<containers::StringType::UTF8> empty(text, text);

    RecordingByteWriter writer;
    const size_t size = HYP_FORMAT_INTO(writer, "<{}>", empty);

    HYP_TEST_CHECK(size == 2);
    HYP_TEST_CHECK(writer.GetText() == "<>");
}

HYP_ENGINE_TEST(StringFormat, FormatIntoMathTypesMatchPrintfOutput)
{
    String buffer;
    HYP_FORMAT_INTO(buffer, "{} {} {}", Vec3f(1.5f, -2.0f, 3.0f), Vec4f(0.0f, 1.0f, 2.0f, 3.0f), NAME("hello"));

    HYP_TEST_CHECK(buffer == "[1.500000 -2.000000 3.000000] [0.000000 1.000000 2.000000 3.000000] hello");
}

HYP_ENGINE_TEST(StringFormat, FormatIntoMat4fIdentityMatchesPrintfOutput)
{
    String buffer;
    HYP_FORMAT_INTO(buffer, "{}", Mat4f::Identity());

    HYP_TEST_CHECK(buffer == "Mat4f([1.000000 0.000000 0.000000 0.000000], [0.000000 1.000000 0.000000 0.000000], [0.000000 0.000000 1.000000 0.000000], [0.000000 0.000000 0.000000 1.000000])");
}

HYP_ENGINE_TEST(StringFormat, FormatIntoVectorWithByteWriter)
{
    RecordingByteWriter writer;
    const size_t size = HYP_FORMAT_INTO(writer, "{}", Vec2f(1.0f, 2.5f));

    HYP_TEST_CHECK(size == 19);
    HYP_TEST_CHECK(writer.GetText() == "[1.000000 2.500000]");
}

HYP_ENGINE_TEST(StringFormat, RemoveInvalidUtf8KeepsValidSequences)
{
    char text[] = "a\xE9" "b\x80" "c\xE2\x82\xAC" "d\xE2\x82";
    const size_t size = utf::RemoveInvalidUtf8(text, sizeof(text) - 1);

    HYP_TEST_CHECK(std::string_view(text, size) == "abc\xE2\x82\xAC" "d");
}

HYP_ENGINE_TEST(StringFormat, TrimIncompleteUtf8DropsPartialSequence)
{
    HYP_TEST_CHECK(utf::TrimIncompleteUtf8("ab\xC3", 3) == 2);
    HYP_TEST_CHECK(utf::TrimIncompleteUtf8("a\xE2\x82", 3) == 1);
    HYP_TEST_CHECK(utf::TrimIncompleteUtf8("a\xE2\x82\xAC", 4) == 4);
    HYP_TEST_CHECK(utf::TrimIncompleteUtf8("abc", 3) == 3);
    HYP_TEST_CHECK(utf::TrimIncompleteUtf8("\xE2\x82", 2) == 2);
}

HYP_ENGINE_TEST(StringFormat, LogLongMessageWindowsReassemble)
{
    String text("a");

    for (int i = 0; i < 1000; i++)
    {
        text.Append("\xC3\xA9");
    }

    const Array<String> messages = LogAndCapture(text);

    String joined;

    for (const String& message : messages)
    {
        HYP_TEST_CHECK(!message.Empty());

        joined.Append(message);
    }

    HYP_TEST_CHECK(messages.Size() > 1);
    HYP_TEST_CHECK(joined == HYP_FORMAT("StringFormatLogTest [Info]: {}\n", text));
}

HYP_ENGINE_TEST(StringFormat, LogInvalidUtf8KeepsMessage)
{
    const Array<String> messages = LogAndCapture("bad\xE9!");

    HYP_TEST_CHECK(messages.Size() == 1);
    HYP_TEST_CHECK(messages.Size() == 1 && messages[0] == "StringFormatLogTest [Info]: bad!\n");
}

} // namespace

} // namespace core
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS
