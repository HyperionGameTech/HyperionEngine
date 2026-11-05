/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#ifdef HYP_TESTS

#include <Framework/Testing/EngineTest.hpp>

#include <Core/Containers/String.hpp>

#include <Core/Utilities/StringView.hpp>

#include <Core/Memory/ByteBuffer.hpp>

namespace Hyperion {
namespace tests {
namespace core {

HYP_ENGINE_TEST(StringView, EqualityNeedsTheWholeString)
{
    HYP_TEST_CHECK(ANSIStringView("abc") == ANSIStringView("abc"));
    HYP_TEST_CHECK(ANSIStringView("abcdef") != ANSIStringView("abc"));
    HYP_TEST_CHECK(ANSIStringView("abc") != ANSIStringView("abcdef"));
    HYP_TEST_CHECK(UTF8StringView("abcdef") != UTF8StringView("abc"));

    HYP_TEST_CHECK(ANSIStringView() == ANSIStringView(""));
    HYP_TEST_CHECK(ANSIStringView("") != ANSIStringView("a"));
    HYP_TEST_CHECK(ANSIStringView() != ANSIStringView("a"));

    // a view over a buffer runs to the buffer's end; its string stops at the terminator
    const char padded[] = { 'a', 'b', 'c', '\0', 'z', 'z' };
    const ANSIStringView view(ConstByteView(reinterpret_cast<const ubyte*>(padded), sizeof(padded)));

    HYP_TEST_CHECK(view == ANSIStringView("abc"));
    HYP_TEST_CHECK(view != ANSIStringView("ab"));
}

HYP_ENGINE_TEST(StringView, ByteViewNeedNotBeTerminated)
{
    // the bytes after the view aren't a terminator, so counting past its end would make the string longer
    struct
    {
        char text[3] = { 'a', 'b', 'c' };
        char after[4] = { 'x', 'y', 'z', '\0' };
    } bytes;

    const ConstByteView byteView(reinterpret_cast<const ubyte*>(bytes.text), sizeof(bytes.text));

    const ANSIStringView ansi(byteView);
    HYP_TEST_CHECK(ansi.Size() == 3 && ansi.Length() == 3);
    HYP_TEST_CHECK(ansi == ANSIStringView("abc"));

    const UTF8StringView utf8(byteView);
    HYP_TEST_CHECK(utf8.Size() == 3 && utf8.Length() == 3);

    const ByteBuffer buffer(byteView);
    HYP_TEST_CHECK(ANSIStringView(buffer.ToByteView()).Length() == 3);
}

HYP_ENGINE_TEST(StringView, FindFirstIndexFrom)
{
    const ANSIStringView text = "a,b,c";

    HYP_TEST_CHECK(text.FindFirstIndex(',', 0) == 1);
    HYP_TEST_CHECK(text.FindFirstIndex(',', 1) == 1);
    HYP_TEST_CHECK(text.FindFirstIndex(',', 2) == 3);
    HYP_TEST_CHECK(text.FindFirstIndex(',', 4) == ANSIStringView::NotFound);
    HYP_TEST_CHECK(text.FindFirstIndex(',', 100) == ANSIStringView::NotFound);

    const ANSIStringView ron = "texture: \"leaf\", bark_texture: \"bark\"";
    const ANSIStringView key = "texture: \"";

    Array<String> found;

    for (size_t at = ron.FindFirstIndex(key); at != ANSIStringView::NotFound; at = ron.FindFirstIndex(key, at + key.Length()))
    {
        const size_t start = at + key.Length();
        const size_t end = ron.FindFirstIndex('"', start);

        HYP_TEST_REQUIRE(end != ANSIStringView::NotFound);

        found.PushBack(String(ron.Substr(start, end)));
    }

    HYP_TEST_REQUIRE(found.Size() == 2);
    HYP_TEST_CHECK(found[0] == "leaf" && found[1] == "bark");

    // UTF-8 counts in code points, from and result alike
    const UTF8StringView accents = "\xc3\xa9,\xc3\xa9,";

    HYP_TEST_CHECK(accents.FindFirstIndex(',', 0) == 1);
    HYP_TEST_CHECK(accents.FindFirstIndex(',', 2) == 3);
    HYP_TEST_CHECK(accents.FindFirstIndex(UTF8StringView(","), 2) == 3);

    const String string = "one two one";

    HYP_TEST_CHECK(string.FindFirstIndex(UTF8StringView("one"), 1) == 8);
    HYP_TEST_CHECK(string.FindFirstIndex('o', 1) == 6);
}

} // namespace core
} // namespace tests
} // namespace Hyperion

#endif
