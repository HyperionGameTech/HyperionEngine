/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_COMMAND_LINE_HPP
#define HYPERION_COMMAND_LINE_HPP

#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/FlatMap.hpp>

#include <core/utilities/Pair.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/StringView.hpp>
#include <core/utilities/Result.hpp>

#include <core/memory/Pimpl.hpp>

#include <core/object/HypObject.hpp>

#include <core/Defines.hpp>

#include <core/json/JSON.hpp>

namespace hyperion {

enum class CommandLineArgumentFlags : uint32
{
    NONE            = 0x0,
    REQUIRED        = 0x1,
    ALLOW_MULTIPLE  = 0x2
};

HYP_MAKE_ENUM_FLAGS(CommandLineArgumentFlags)

enum class CommandLineParserFlags : uint32
{
    NONE                = 0x0,
    ALLOW_UNKNOWN_ARGS  = 0x1
};

HYP_MAKE_ENUM_FLAGS(CommandLineParserFlags)

HYP_ENUM()
enum class CommandLineArgumentType
{
    STRING,
    INTEGER,
    FLOAT,
    BOOLEAN,
    ENUM
};

namespace cli {

using CommandLineArgumentValue = json::JSONValue;

class CommandLineParser;
struct CommandLineArgumentDefinition;

struct CommandLineArgumentDefinition
{
    String                              name;
    Optional<String>                    shorthand;
    Optional<String>                    description;
    EnumFlags<CommandLineArgumentFlags> flags;
    CommandLineArgumentType             type;
    Optional<CommandLineArgumentValue>  default_value;
    Optional<Array<String>>             enum_values;
};

struct CommandLineArgumentDefinitionsImpl;

HYP_STRUCT(Size=16)
struct HYP_API CommandLineArgumentDefinitions
{
    Pimpl<CommandLineArgumentDefinitionsImpl>   m_impl;

public:
    using Iterator = CommandLineArgumentDefinition *;
    using ConstIterator = CommandLineArgumentDefinition *;

    CommandLineArgumentDefinitions();
    CommandLineArgumentDefinitions(const Array<CommandLineArgumentDefinition> &definitions);

    CommandLineArgumentDefinitions(const CommandLineArgumentDefinitions &other)                 = delete;
    CommandLineArgumentDefinitions &operator=(const CommandLineArgumentDefinitions &other)      = delete;

    CommandLineArgumentDefinitions(CommandLineArgumentDefinitions &&other) noexcept             = default;
    CommandLineArgumentDefinitions &operator=(CommandLineArgumentDefinitions &&other) noexcept  = default;

    ~CommandLineArgumentDefinitions();

    Span<CommandLineArgumentDefinition> GetDefinitions() const;

    // Add an argument - may be a string, int, float, bool.
    CommandLineArgumentDefinitions &Add(
        const String &name,
        const String &shorthand = String::empty,
        const String &description = String::empty,
        EnumFlags<CommandLineArgumentFlags> flags = CommandLineArgumentFlags::NONE,
        CommandLineArgumentType type = CommandLineArgumentType::STRING,
        const CommandLineArgumentValue &default_value = { }
    );

    // Add an enum argument
    CommandLineArgumentDefinitions &Add(
        const String &name,
        const String &shorthand = String::empty,
        const String &description = String::empty,
        EnumFlags<CommandLineArgumentFlags> flags = CommandLineArgumentFlags::NONE,
        const Optional<Array<String>> &enum_values = { },
        const CommandLineArgumentValue &default_value = { }
    );
    
    CommandLineArgumentDefinition *Find(UTF8StringView key) const;

    HYP_DEF_STL_BEGIN_END(GetDefinitions().Begin(), GetDefinitions().End())
};

HYP_STRUCT()
class HYP_API CommandLineArguments
{
    String                                          command;
    Array<Pair<String, CommandLineArgumentValue>>   values;

public:
    friend class CommandLineParser;

    using Iterator = Array<Pair<String, CommandLineArgumentValue>>::Iterator;
    using ConstIterator = Array<Pair<String, CommandLineArgumentValue>>::ConstIterator;

    const CommandLineArgumentValue &operator[](UTF8StringView key) const;

    HYP_FORCE_INLINE const String &GetCommand() const
        { return command; }

    HYP_FORCE_INLINE const Array<Pair<String, CommandLineArgumentValue>> &GetValues() const
        { return values; }

    HYP_FORCE_INLINE SizeType Size() const
        { return values.Size(); }

    HYP_FORCE_INLINE Iterator Find(UTF8StringView key)
        { return values.FindIf([key](auto &pair) { return pair.first == key; }); }

    HYP_FORCE_INLINE ConstIterator Find(UTF8StringView key) const
        { return values.FindIf([key](const auto &pair) { return pair.first == key; }); }

    HYP_FORCE_INLINE bool Contains(UTF8StringView key) const
        { return Find(key) != values.End(); }

    HYP_NODISCARD static CommandLineArguments Merge(const CommandLineArgumentDefinitions &definitions, const CommandLineArguments &a, const CommandLineArguments &b);
    HYP_NODISCARD static TResult<CommandLineArgumentValue> ParseArgumentValue(const CommandLineArgumentDefinition &definition, const String &str);

    HYP_DEF_STL_BEGIN_END(values.Begin(), values.End())
};

class CommandLineParser
{
public:
    CommandLineParser()
        : m_definitions(nullptr),
          m_flags(CommandLineParserFlags::NONE)
    {
    }

    CommandLineParser(const CommandLineArgumentDefinitions *definitions, EnumFlags<CommandLineParserFlags> flags = CommandLineParserFlags::NONE)
        : m_definitions(definitions),
          m_flags(flags)
    {
    }

    CommandLineParser(const CommandLineParser &other)                   = delete;
    CommandLineParser &operator=(const CommandLineParser &other)        = delete;
    CommandLineParser(CommandLineParser &&other) noexcept               = default;
    CommandLineParser &operator=(CommandLineParser &&other) noexcept    = default;
    ~CommandLineParser()                                                = default;

    HYP_FORCE_INLINE const CommandLineArgumentDefinitions *GetDefinitions() const
        { return m_definitions; }

    HYP_API TResult<CommandLineArguments> Parse(const String &command_line) const;
    HYP_API TResult<CommandLineArguments> Parse(int argc, char **argv) const;
    HYP_API TResult<CommandLineArguments> Parse(const String &command, const Array<String> &args) const;

private:
    const CommandLineArgumentDefinitions    *m_definitions;
    EnumFlags<CommandLineParserFlags>       m_flags;
};

} // namespace cli

using cli::CommandLineParser;
using cli::CommandLineArguments;
using cli::CommandLineArgumentValue;
using cli::CommandLineArgumentDefinitions;

} // namespace hyperion

#endif