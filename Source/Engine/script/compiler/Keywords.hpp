#pragma once

#include <Core/containers/String.hpp>
#include <Core/containers/HashMap.hpp>
#include <Core/utilities/Optional.hpp>

#include <map>

namespace Hyperion {
enum Keywords
{
    Keyword_module,
    Keyword_import,
    Keyword_export,
    Keyword_const,
    Keyword_static,
    Keyword_public,
    Keyword_private,
    Keyword_protected,
    Keyword_extern,
    Keyword_ref,
    Keyword_val,
    Keyword_func,
    Keyword_class,
    Keyword_struct,
    Keyword_proxy,
    Keyword_mixin,
    Keyword_enum,
    Keyword_as,
    Keyword_has,
    Keyword_new,
    Keyword_self,
    Keyword_if,
    Keyword_else,
    Keyword_for,
    Keyword_in,
    Keyword_is,
    Keyword_while,
    Keyword_do,
    Keyword_on,
    Keyword_try,
    Keyword_catch,
    Keyword_throw,
    Keyword_null,
    Keyword_true,
    Keyword_false,
    Keyword_return,
    Keyword_yield,
    Keyword_break,
    Keyword_continue,
    Keyword_typeof,
    Keyword_end
};

class Keyword
{
    /* Static class members */
public:
    static bool IsKeyword(const String& str);
    static Optional<String> ToString(Keywords keyword);

private:
    static const HashMap<String, Keywords> keywordStrings;
};

} // namespace Hyperion
