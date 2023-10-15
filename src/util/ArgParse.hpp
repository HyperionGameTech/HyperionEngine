#ifndef HYPERION_ARG_PARSE_HPP
#define HYPERION_ARG_PARSE_HPP

#include <core/lib/String.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/Pair.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/Optional.hpp>
#include <core/lib/Variant.hpp>

namespace hyperion {

class ArgParse
{
public:
    struct NullArgument { };

    using ArgumentValue = Variant<String, Int, Float, Bool>;

    enum ArgumentType
    {
        ARGUMENT_TYPE_STRING = 1,
        ARGUMENT_TYPE_INT,
        ARGUMENT_TYPE_FLOAT,
        ARGUMENT_TYPE_BOOL
    };

    struct ArgumentDefinition
    {
        String              name;
        Optional<String>    shorthand;
        ArgumentType        type;
        Bool                required;
    };

    struct Result
    {
        Array<Pair<String, ArgumentValue>>  values;
        Bool                                ok;
        Optional<String>                    message;

        ArgumentValue operator[](const String &key) const;
    };

    void Add(String name, String shorthand = String::empty, ArgumentType type = ARGUMENT_TYPE_STRING, Bool required = true);

    Result Parse(Int argc, char **argv) const;
    Result Parse(const Array<String> &args) const;

private:
    Array<ArgumentDefinition> m_definitions;
};

} // namespace hyperion

#endif