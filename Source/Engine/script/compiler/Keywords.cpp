#include <script/compiler/Keywords.hpp>

namespace Hyperion {
const HashMap<String, Keywords> Keyword::keywordStrings = {
    { "module", Keyword_module },
    { "import", Keyword_import },
    { "export", Keyword_export },
    { "from", Keyword_from },
    { "const", Keyword_const },
    { "static", Keyword_static },
    { "public", Keyword_public },
    { "private", Keyword_private },
    { "protected", Keyword_protected },
    { "extern", Keyword_extern },
    { "ref", Keyword_ref },
    { "func", Keyword_func },
    { "class", Keyword_class },
    { "struct", Keyword_struct },
    { "proxy", Keyword_proxy },
    { "mixin", Keyword_mixin },
    { "enum", Keyword_enum },
    { "as", Keyword_as },
    { "has", Keyword_has },
    { "new", Keyword_new },
    { "self", Keyword_self },
    { "if", Keyword_if },
    { "else", Keyword_else },
    { "for", Keyword_for },
    { "in", Keyword_in },
    { "is", Keyword_is },
    { "while", Keyword_while },
    { "do", Keyword_do },
    { "on", Keyword_on },
    { "try", Keyword_try },
    { "catch", Keyword_catch },
    { "throw", Keyword_throw },
    { "null", Keyword_null },
    { "true", Keyword_true },
    { "false", Keyword_false },
    { "return", Keyword_return },
    { "yield", Keyword_yield },
    { "break", Keyword_break },
    { "continue", Keyword_continue },
    { "typeof", Keyword_typeof },
    { "end", Keyword_end }
};

bool Keyword::IsKeyword(const String& str)
{
    return keywordStrings.Find(str) != keywordStrings.End();
}

Optional<String> Keyword::ToString(Keywords keyword)
{
    for (auto& it : keywordStrings)
    {
        if (it.second == keyword)
        {
            return it.first;
        }
    }

    return {};
}

} // namespace Hyperion
