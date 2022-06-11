#ifndef KEYWORDS_HPP
#define KEYWORDS_HPP

#include <string>
#include <map>

namespace hyperion::compiler {
enum Keywords {
    Keyword_module,
    Keyword_import,
    Keyword_export,
    Keyword_use,
    Keyword_let,
    Keyword_const,
    Keyword_static,
    Keyword_generic,
    Keyword_ref,
    Keyword_val,
    Keyword_func,
    Keyword_type,
    Keyword_alias,
    Keyword_mixin,
    Keyword_enum,
    Keyword_as,
    Keyword_has,
    Keyword_new,
    Keyword_print,
    Keyword_self,
    Keyword_if,
    Keyword_else,
    Keyword_for,
    Keyword_in,
    Keyword_while,
    Keyword_do,
    Keyword_on,
    Keyword_try,
    Keyword_catch,
    Keyword_throw,
    Keyword_null,
    Keyword_void,
    Keyword_true,
    Keyword_false,
    Keyword_return,
    Keyword_yield,
    Keyword_break,
    Keyword_continue,
    Keyword_async,
    Keyword_pure,
    Keyword_impure,
    Keyword_valueof,
    Keyword_typeof,
    Keyword_meta,
    Keyword_syntax
};

class Keyword {
/* Static class members */
public:
    static bool IsKeyword(const std::string &str);
    static std::string ToString(Keywords keyword);

private:
    static const std::map<std::string, Keywords> keyword_strings;
};

} // namespace hyperion::compiler

#endif
