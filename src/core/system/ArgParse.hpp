/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ARG_PARSE_HPP
#define HYPERION_ARG_PARSE_HPP

#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/utilities/Pair.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/Variant.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/Defines.hpp>

namespace hyperion {

enum class ArgFlags : uint32
{
    NONE        = 0x0,
    REQUIRED    = 0x1
};

HYP_MAKE_ENUM_FLAGS(ArgFlags)

enum class CommandLineArgumentType
{
    STRING = 1,
    INTEGER,
    FLOAT,
    BOOLEAN,
    ENUM
};

namespace sys {

using CommandLineArgumentValue = Variant<String, int, float, bool>;

class ArgParse;

class CommandLineArguments
{
    String                                          command;
    Array<Pair<String, CommandLineArgumentValue>>   values;

public:
    friend class ArgParse;

    using Iterator = Array<Pair<String, CommandLineArgumentValue>>::Iterator;
    using ConstIterator = Array<Pair<String, CommandLineArgumentValue>>::ConstIterator;

    CommandLineArgumentValue operator[](const String &key) const
    {
        auto it = Find(key);

        if (it == values.End()) {
            return CommandLineArgumentValue { };
        }

        return it->second;
    }

    const String &GetCommand() const
        { return command; }

    const Array<Pair<String, CommandLineArgumentValue>> &GetValues() const
        { return values; }

    SizeType Size() const
        { return values.Size(); }

    ConstIterator Find(const String &key) const
        { return values.FindIf([key](const auto &pair) { return pair.first == key; }); }

    bool Contains(const String &key) const
        { return Find(key) != values.End(); }

    HYP_DEF_STL_BEGIN_END(
        values.Begin(),
        values.End()
    )
};

class ArgParse
{
public:
    struct ParseResult
    {
        CommandLineArguments    result;
        bool                    ok;
        Optional<String>        message;

        explicit operator bool() const
            { return ok; }
    };

    struct ArgumentDefinition
    {
        String                      name;
        Optional<String>            shorthand;
        ArgFlags                    flags;
        CommandLineArgumentType     type;
        CommandLineArgumentValue    default_value;
        Optional<Array<String>>     enum_values;
    };

    // Add an argument - may be a string, int, float, bool.
    HYP_API void Add(
        String name,
        String shorthand = String::empty,
        EnumFlags<ArgFlags> flags = ArgFlags::NONE,
        CommandLineArgumentType type = CommandLineArgumentType::STRING,
        CommandLineArgumentValue default_value = { }
    );

    // Add an enum argument
    HYP_API void Add(
        String name,
        String shorthand = String::empty,
        EnumFlags<ArgFlags> flags = ArgFlags::NONE,
        Optional<Array<String>> enum_values = { },
        CommandLineArgumentValue = { }
    );

    HYP_API ParseResult Parse(int argc, char **argv) const;
    HYP_API ParseResult Parse(const String &command, const Array<String> &args) const;

private:
    Array<ArgumentDefinition>   m_definitions;
};

} // namespace sys

using sys::ArgParse;

using sys::CommandLineArguments;
using sys::CommandLineArgumentValue;

} // namespace hyperion

#endif