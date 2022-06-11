#include <script/compiler/Keywords.hpp>

namespace hyperion::compiler {
const std::map<std::string, Keywords> Keyword::keyword_strings = {
    { "module",   Keyword_module },
    { "import",   Keyword_import },
    { "export",   Keyword_export },
    { "use",      Keyword_use },
    { "let",      Keyword_let },
    { "const",    Keyword_const },
    { "static",   Keyword_static },
    { "generic",  Keyword_generic },
    { "ref",      Keyword_ref },
    { "val",      Keyword_val },
    { "func",     Keyword_func },
    { "type",     Keyword_type },
    { "alias",    Keyword_alias },
    { "mixin",    Keyword_mixin },
    { "enum",     Keyword_enum },
    { "as",       Keyword_as },
    { "has",      Keyword_has },
    { "new",      Keyword_new },
    { "print",    Keyword_print },
    { "self",     Keyword_self },
    { "if",       Keyword_if },
    { "else",     Keyword_else },
    { "for",      Keyword_for },
    { "in",       Keyword_in },
    { "while",    Keyword_while },
    { "do",       Keyword_do },
    { "on",       Keyword_on },
    { "try",      Keyword_try },
    { "catch",    Keyword_catch },
    { "throw",    Keyword_throw },
    { "null",     Keyword_null },
    { "void",     Keyword_void },
    { "true",     Keyword_true },
    { "false",    Keyword_false },
    { "return",   Keyword_return },
    { "yield",    Keyword_yield },
    { "break",    Keyword_break },
    { "continue", Keyword_continue },
    { "async",    Keyword_async },
    { "pure",     Keyword_pure },
    { "impure",   Keyword_impure },
    { "valueof",  Keyword_valueof },
    { "typeof",   Keyword_typeof },
    { "$meta",    Keyword_meta },
    { "syntax",   Keyword_syntax }
};

bool Keyword::IsKeyword(const std::string &str)
{
    return keyword_strings.find(str) != keyword_strings.end();
}

std::string Keyword::ToString(Keywords keyword)
{
    for (auto &it : keyword_strings) {
        if (it.second == keyword) {
            return it.first;
        }
    }
    return "";
}

} // namespace hyperion::compiler
