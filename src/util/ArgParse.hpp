#ifndef HYPERION_ARG_PARSE_HPP
#define HYPERION_ARG_PARSE_HPP

#include <core/lib/String.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/Pair.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/Optional.hpp>
#include <core/lib/Variant.hpp>

#include <util/Defines.hpp>

namespace hyperion {

class ArgParse
{
public:
    using ArgumentValue = Variant<String, int, float, bool>;

    enum ArgumentType
    {
        ARGUMENT_TYPE_STRING = 1,
        ARGUMENT_TYPE_INT,
        ARGUMENT_TYPE_FLOAT,
        ARGUMENT_TYPE_BOOL,
        ARGUMENT_TYPE_ENUM
    };

    using ArgFlags = uint32;

    enum ArgFlagBits : ArgFlags
    {
        ARG_FLAGS_NONE      = 0x0,
        ARG_FLAGS_REQUIRED  = 0x1
    };

    struct ArgumentDefinition
    {
        String                  name;
        Optional<String>        shorthand;
        ArgFlags                flags;
        ArgumentType            type;
        ArgumentValue           default_value;
        Optional<Array<String>> enum_values;
    };

    struct Result
    {
        Array<Pair<String, ArgumentValue>>  values;
        bool                                ok;
        Optional<String>                    message;

        explicit operator bool() const
            { return ok; }

        HYP_API const ArgumentValue &operator[](const String &key) const;
    };

    // Add an argument - may be a string, int, float, bool.
    HYP_API void Add(
        String name,
        String shorthand = String::empty,
        ArgFlags flags = ARG_FLAGS_NONE,
        ArgumentType type = ARGUMENT_TYPE_STRING,
        ArgumentValue default_value = { }
    );

    // Add an enum argument
    HYP_API void Add(
        String name,
        String shorthand = String::empty,
        ArgFlags flags = ARG_FLAGS_NONE,
        Optional<Array<String>> enum_values = { },
        ArgumentValue = { }
    );

    HYP_API Result Parse(int argc, char **argv) const;
    HYP_API Result Parse(const Array<String> &args) const;

private:
    Array<ArgumentDefinition>   m_definitions;
};

} // namespace hyperion

#endif