/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/containers/String.hpp>
#include <Core/containers/Array.hpp>
#include <Core/containers/FlatMap.hpp>

#include <Core/utilities/Pair.hpp>
#include <Core/utilities/Optional.hpp>
#include <Core/utilities/EnumFlags.hpp>
#include <Core/utilities/StringView.hpp>
#include <Core/utilities/Result.hpp>

#include <Core/memory/Pimpl.hpp>

#include <Core/reflection/ObjectMacros.hpp>

#include <Core/Defines.hpp>

#include <Core/json/JSON.hpp>

namespace Hyperion {

enum class CommandLineArgumentFlags : uint32
{
    NONE = 0x0,
    REQUIRED = 0x1,
    ALLOW_MULTIPLE = 0x2
};

HYP_MAKE_ENUM_FLAGS(CommandLineArgumentFlags)

enum class CommandLineParserFlags : uint32
{
    NONE = 0x0,
    ALLOW_UNKNOWN_ARGS = 0x1
};

HYP_MAKE_ENUM_FLAGS(CommandLineParserFlags)

HYP_ENUM()
enum class CommandLineArgumentType : uint8
{
    STRING,
    INTEGER,
    FLOAT,
    BOOLEAN,
    ENUM
};

namespace cli {

using CommandLineArgumentValue = JSON::Value;

class CommandLineParser;
struct CommandLineArgumentDefinition;

struct CommandLineArgumentDefinition
{
    String name;
    Optional<String> shorthand;
    Optional<String> description;
    EnumFlags<CommandLineArgumentFlags> flags;
    CommandLineArgumentType type;
    Optional<CommandLineArgumentValue> defaultValue;
    Optional<Array<String>> enumValues;
};

struct CommandLineArgumentDefinitionsImpl;

HYP_STRUCT(Size = 8)
struct HYP_API CommandLineArgumentDefinitions
{
    HYP_STRUCT_BODY(CommandLineArgumentDefinitions);

    Pimpl<CommandLineArgumentDefinitionsImpl> m_impl;

public:
    using Iterator = CommandLineArgumentDefinition*;
    using ConstIterator = CommandLineArgumentDefinition*;

    CommandLineArgumentDefinitions();
    CommandLineArgumentDefinitions(const Array<CommandLineArgumentDefinition>& definitions);

    CommandLineArgumentDefinitions(const CommandLineArgumentDefinitions& other);
    CommandLineArgumentDefinitions& operator=(const CommandLineArgumentDefinitions& other);

    CommandLineArgumentDefinitions(CommandLineArgumentDefinitions&& other) noexcept = default;
    CommandLineArgumentDefinitions& operator=(CommandLineArgumentDefinitions&& other) noexcept = default;

    ~CommandLineArgumentDefinitions();

    Span<CommandLineArgumentDefinition> GetDefinitions() const;

    // Add an argument - may be a string, int, float, bool.
    CommandLineArgumentDefinitions& Add(
        const String& name,
        const String& shorthand = String::empty,
        const String& description = String::empty,
        EnumFlags<CommandLineArgumentFlags> flags = CommandLineArgumentFlags::NONE,
        CommandLineArgumentType type = CommandLineArgumentType::STRING,
        const CommandLineArgumentValue& defaultValue = JSON::Undefined());

    // Add an enum argument
    CommandLineArgumentDefinitions& Add(
        const String& name,
        const String& shorthand = String::empty,
        const String& description = String::empty,
        EnumFlags<CommandLineArgumentFlags> flags = CommandLineArgumentFlags::NONE,
        const Optional<Array<String>>& enumValues = {},
        const CommandLineArgumentValue& defaultValue = JSON::Undefined());

    CommandLineArgumentDefinition* Find(UTF8StringView key) const;

    HYP_DEF_STL_BEGIN_END(GetDefinitions().Begin(), GetDefinitions().End())
};

HYP_STRUCT()
class HYP_API CommandLineArguments
{
    HYP_STRUCT_BODY(CommandLineArguments);

    ANSIString m_command;
    Array<Pair<String, CommandLineArgumentValue>> m_values;

public:
    friend class CommandLineParser;

    using Iterator = Array<Pair<String, CommandLineArgumentValue>>::Iterator;
    using ConstIterator = Array<Pair<String, CommandLineArgumentValue>>::ConstIterator;

    CommandLineArguments() = default;

    CommandLineArguments(const ANSIString& command)
        : m_command(command)
    {
    }

    CommandLineArguments(const CommandLineArguments& other) = default;
    CommandLineArguments& operator=(const CommandLineArguments& other) = default;
    CommandLineArguments(CommandLineArguments&& other) noexcept = default;
    CommandLineArguments& operator=(CommandLineArguments&& other) noexcept = default;

    ~CommandLineArguments() = default;

    const CommandLineArgumentValue& operator[](UTF8StringView key) const;

    HYP_FORCE_INLINE const ANSIString& GetCommand() const
    {
        return m_command;
    }

    HYP_FORCE_INLINE Array<Pair<String, CommandLineArgumentValue>>& GetValues()
    {
        return m_values;
    }

    HYP_FORCE_INLINE const Array<Pair<String, CommandLineArgumentValue>>& GetValues() const
    {
        return m_values;
    }

    HYP_FORCE_INLINE size_t Size() const
    {
        return m_values.Size();
    }

    HYP_FORCE_INLINE Iterator Find(UTF8StringView key)
    {
        return m_values.FindIf([key](auto& pair)
            {
                return pair.first == key;
            });
    }

    HYP_FORCE_INLINE ConstIterator Find(UTF8StringView key) const
    {
        return m_values.FindIf([key](const auto& pair)
            {
                return pair.first == key;
            });
    }

    HYP_FORCE_INLINE bool Contains(UTF8StringView key) const
    {
        return Find(key) != m_values.End();
    }

    bool Delete(UTF8StringView key)
    {
        const auto it = Find(key);
        if (it == m_values.End())
        {
            return false;
        }

        m_values.Erase(it);

        return true;
    }

    HYP_NODISCARD static CommandLineArguments Merge(const CommandLineArgumentDefinitions& definitions, const CommandLineArguments& a, const CommandLineArguments& b);
    HYP_NODISCARD static TResult<CommandLineArgumentValue> ParseArgumentValue(const CommandLineArgumentDefinition& definition, const String& str);

    HYP_DEF_STL_BEGIN_END(m_values.Begin(), m_values.End())
};

class CommandLineParser
{
public:
    CommandLineParser()
        : m_definitions(nullptr),
          m_flags(CommandLineParserFlags::NONE)
    {
    }

    CommandLineParser(const CommandLineArgumentDefinitions* definitions, EnumFlags<CommandLineParserFlags> flags = CommandLineParserFlags::NONE)
        : m_definitions(definitions),
          m_flags(flags)
    {
    }

    CommandLineParser(const CommandLineParser& other) = delete;
    CommandLineParser& operator=(const CommandLineParser& other) = delete;
    CommandLineParser(CommandLineParser&& other) noexcept = default;
    CommandLineParser& operator=(CommandLineParser&& other) noexcept = default;
    ~CommandLineParser() = default;

    HYP_FORCE_INLINE const CommandLineArgumentDefinitions* GetDefinitions() const
    {
        return m_definitions;
    }

    HYP_API TResult<CommandLineArguments> Parse(const String& commandLine, bool fillDefaults = true) const;
    HYP_API TResult<CommandLineArguments> Parse(int argc, char** argv, bool fillDefaults = true) const;
    HYP_API TResult<CommandLineArguments> Parse(ANSIStringView command, const Array<String>& args, bool fillDefaults = true) const;

    void ApplyDefaults(CommandLineArguments& args) const;

private:
    const CommandLineArgumentDefinitions* m_definitions;
    EnumFlags<CommandLineParserFlags> m_flags;
};

} // namespace cli

using cli::CommandLineArgumentDefinitions;
using cli::CommandLineArguments;
using cli::CommandLineArgumentValue;
using cli::CommandLineParser;

} // namespace Hyperion
