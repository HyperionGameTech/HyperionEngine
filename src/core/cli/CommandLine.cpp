/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/cli/CommandLine.hpp>

#include <core/logging/Logger.hpp>

#include <core/containers/HashSet.hpp>

#include <core/utilities/StringUtil.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Core);
HYP_DEFINE_LOG_SUBCHANNEL(CommandLine, Core);

namespace cli {

static void AppendCommandLineArgumentValue(Array<Pair<String, CommandLineArgumentValue>>& values, const String& key, CommandLineArgumentValue&& value, bool allow_multiple);

static void AppendCommandLineArgumentValue(Array<Pair<String, CommandLineArgumentValue>>& values, const String& key, const CommandLineArgumentValue& value, bool allow_multiple)
{
    AppendCommandLineArgumentValue(values, key, CommandLineArgumentValue(value), allow_multiple);
}

static void AppendCommandLineArgumentValue(Array<Pair<String, CommandLineArgumentValue>>& values, const String& key, CommandLineArgumentValue&& value, bool allow_multiple)
{
    auto it = values.FindIf([key](const auto& item)
        {
            return item.first == key;
        });

    if (it != values.End())
    {
        // Don't overwrite or append the value if it is null or undefined
        if (value.IsNullOrUndefined())
        {
            return;
        }

        if (allow_multiple)
        {
            if (it->second.IsArray())
            {
                it->second.AsArray().PushBack(std::move(value));
            }
            else
            {
                json::JSONArray array;
                array.PushBack(std::move(it->second));
                array.PushBack(std::move(value));

                it->second = json::JSONValue(std::move(array));
            }
        }
        else
        {
            it->second = std::move(value);
        }

        return;
    }

    values.EmplaceBack(key, std::move(value));
}

#pragma region CommandLineArguments

const CommandLineArgumentValue& CommandLineArguments::operator[](UTF8StringView key) const
{
    static const CommandLineArgumentValue empty_value = json::JSONUndefined();

    const auto it = Find(key);

    if (it == End())
    {
        return empty_value;
    }

    return it->second;
}

CommandLineArguments CommandLineArguments::Merge(const CommandLineArgumentDefinitions& definitions, const CommandLineArguments& a, const CommandLineArguments& b)
{
    CommandLineArguments result = a;

    for (const Pair<String, CommandLineArgumentValue>& element : b)
    {
        bool allow_multiple = false;

        const auto definitions_it = definitions.Find(element.first);

        if (definitions_it != definitions.End())
        {
            allow_multiple = definitions_it->flags[CommandLineArgumentFlags::ALLOW_MULTIPLE];
        }

        AppendCommandLineArgumentValue(result.m_values, element.first, element.second, allow_multiple);
    }

    return result;
}

TResult<CommandLineArgumentValue> CommandLineArguments::ParseArgumentValue(const CommandLineArgumentDefinition& definition, const String& str)
{
    const CommandLineArgumentType type = definition.type;

    json::ParseResult parse_result = json::JSON::Parse(str);
    json::JSONValue value = std::move(parse_result.value);

    if (!parse_result.ok)
    {
        // If string, allow unquoted on parse error
        if (type == CommandLineArgumentType::STRING)
        {
            return json::JSONValue(json::JSONString(str));
        }

        HYP_LOG(CommandLine, Error, "Failed to parse argument \"{}\": {}",
            str, parse_result.message);

        return HYP_MAKE_ERROR(Error, "Failed to parse argument");
    }

    switch (type)
    {
    case CommandLineArgumentType::STRING:
        return json::JSONValue(value.ToString());
    case CommandLineArgumentType::INTEGER:
    {
        int32 value_int;

        if (value.IsNumber())
        {
            return json::JSONValue(value.ToInt32());
        }

        if (!StringUtil::Parse(value.ToString(), &value_int))
        {
            return json::JSONValue(value_int);
        }

        return HYP_MAKE_ERROR(Error, "Failed to parse integer argument");
    }
    case CommandLineArgumentType::FLOAT:
    {
        double value_double;

        if (value.IsNumber())
        {
            return json::JSONValue(value.ToDouble());
        }

        if (!StringUtil::Parse(value.ToString(), &value_double))
        {
            return json::JSONValue(value_double);
        }

        return HYP_MAKE_ERROR(Error, "Failed to parse float argument");
    }
    case CommandLineArgumentType::BOOLEAN:
        return json::JSONValue(value.ToBool());
    case CommandLineArgumentType::ENUM:
    {
        json::JSONString string_value = value.ToString();

        const Array<String>* enum_values = definition.enum_values.TryGet();

        if (!enum_values)
        {
            return HYP_MAKE_ERROR(Error, "Internal error parsing enum argument");
        }

        if (!enum_values->Contains(string_value))
        {
            return HYP_MAKE_ERROR(Error, "Not a valid value for argument");
        }

        return json::JSONValue(std::move(string_value));
    }
    }

    return HYP_MAKE_ERROR(Error, "Invalid argument");
}

#pragma endregion CommandLineArguments

#pragma region CommandLineArgumentDefinitionsImpl

struct CommandLineArgumentDefinitionsImpl
{
    using Iterator = typename Array<CommandLineArgumentDefinition>::Iterator;
    using ConstIterator = typename Array<CommandLineArgumentDefinition>::ConstIterator;

    Array<CommandLineArgumentDefinition> definitions;

    CommandLineArgumentDefinitionsImpl() = default;

    CommandLineArgumentDefinitionsImpl(const Array<CommandLineArgumentDefinition>& definitions)
        : definitions(definitions)
    {
    }

    CommandLineArgumentDefinitionsImpl(const CommandLineArgumentDefinitionsImpl& other) = default;
    CommandLineArgumentDefinitionsImpl& operator=(const CommandLineArgumentDefinitionsImpl& other) = default;

    CommandLineArgumentDefinitionsImpl(CommandLineArgumentDefinitionsImpl&& other) noexcept = default;
    CommandLineArgumentDefinitionsImpl& operator=(CommandLineArgumentDefinitionsImpl&& other) noexcept = default;

    ~CommandLineArgumentDefinitionsImpl() = default;

    HYP_FORCE_INLINE void Add(CommandLineArgumentDefinition&& definition)
    {
        definitions.PushBack(std::move(definition));
    }

    HYP_FORCE_INLINE Iterator Find(UTF8StringView key)
    {
        return definitions.FindIf([key](const CommandLineArgumentDefinition& definition)
            {
                return definition.name == key;
            });
    }

    HYP_FORCE_INLINE ConstIterator Find(UTF8StringView key) const
    {
        return definitions.FindIf([key](const CommandLineArgumentDefinition& definition)
            {
                return definition.name == key;
            });
    }

    HYP_DEF_STL_BEGIN_END(definitions.Begin(), definitions.End())
};

#pragma endregion CommandLineArgumentDefinitionsImpl

#pragma region CommandLineArgumentDefinitions

CommandLineArgumentDefinitions::CommandLineArgumentDefinitions()
    : m_impl(MakePimpl<CommandLineArgumentDefinitionsImpl>())
{
}

CommandLineArgumentDefinitions::CommandLineArgumentDefinitions(const Array<CommandLineArgumentDefinition>& definitions)
    : m_impl(MakePimpl<CommandLineArgumentDefinitionsImpl>(definitions))
{
}

CommandLineArgumentDefinitions::~CommandLineArgumentDefinitions()
{
    m_impl.Reset();
}

Span<CommandLineArgumentDefinition> CommandLineArgumentDefinitions::GetDefinitions() const
{
    return m_impl ? m_impl->definitions.ToSpan() : Span<CommandLineArgumentDefinition>();
}

CommandLineArgumentDefinition* CommandLineArgumentDefinitions::Find(UTF8StringView key) const
{
    if (!m_impl)
    {
        return nullptr;
    }

    auto it = m_impl->Find(key);

    if (it == m_impl->End())
    {
        return nullptr;
    }

    return &(*it);
}

CommandLineArgumentDefinitions& CommandLineArgumentDefinitions::Add(
    const String& name,
    const String& shorthand,
    const String& description,
    EnumFlags<CommandLineArgumentFlags> flags,
    CommandLineArgumentType type,
    const CommandLineArgumentValue& default_value)
{
    if (!m_impl)
    {
        m_impl = MakePimpl<CommandLineArgumentDefinitionsImpl>();
    }

    auto it = m_impl->definitions.FindIf([&name](const auto& item)
        {
            return item.name == name;
        });

    if (it != m_impl->definitions.End())
    {
        *it = CommandLineArgumentDefinition {
            name,
            shorthand.Empty() ? Optional<String>() : shorthand,
            description.Empty() ? Optional<String>() : description,
            flags,
            type,
            default_value
        };

        return *this;
    }

    m_impl->definitions.PushBack(CommandLineArgumentDefinition {
        name,
        shorthand.Empty() ? Optional<String>() : shorthand,
        description.Empty() ? Optional<String>() : description,
        flags,
        type,
        default_value });

    return *this;
}

CommandLineArgumentDefinitions& CommandLineArgumentDefinitions::Add(
    const String& name,
    const String& shorthand,
    const String& description,
    EnumFlags<CommandLineArgumentFlags> flags,
    const Optional<Array<String>>& enum_values,
    const CommandLineArgumentValue& default_value)
{
    if (!m_impl)
    {
        m_impl = MakePimpl<CommandLineArgumentDefinitionsImpl>();
    }

    auto it = m_impl->definitions.FindIf([&name](const auto& item)
        {
            return item.name == name;
        });

    if (it != m_impl->definitions.End())
    {
        *it = CommandLineArgumentDefinition {
            name,
            shorthand.Empty() ? Optional<String>() : shorthand,
            description.Empty() ? Optional<String>() : description,
            flags,
            CommandLineArgumentType::ENUM,
            default_value,
            enum_values
        };

        return *this;
    }

    m_impl->definitions.PushBack(CommandLineArgumentDefinition {
        name,
        shorthand.Empty() ? Optional<String>() : shorthand,
        description.Empty() ? Optional<String>() : description,
        flags,
        CommandLineArgumentType::ENUM,
        default_value,
        enum_values });

    return *this;
}

#pragma endregion CommandLineArgumentDefinitions

#pragma region CommandLineParser

TResult<CommandLineArguments> CommandLineParser::Parse(const String& command_line) const
{
    String command;
    Array<String> args;

    int current_string_index = 0;
    String current_string;

    auto add_current_string = [&]()
    {
        if (current_string.Any())
        {
            if (current_string_index++ == 0)
            {
                command = std::move(current_string);
            }
            else
            {
                args.PushBack(std::move(current_string));
            }
        }
    };

    for (SizeType char_index = 0; char_index < command_line.Length(); char_index++)
    {
        uint32 current_char = command_line.GetChar(char_index);

        if (std::isspace(int(current_char)))
        {
            add_current_string();

            continue;
        }

        if (current_char == uint32('"'))
        {
            do
            {
                current_string.Append(current_char);

                ++char_index;
            }
            while (char_index < command_line.Length() && (current_char = command_line.GetChar(char_index)) != uint32('"'));
        }
        else if (current_char == uint32('\''))
        {
            do
            {
                current_string.Append(current_char);

                ++char_index;
            }
            while (char_index < command_line.Length() && (current_char = command_line.GetChar(char_index)) != uint32('\''));
        }

        current_string.Append(current_char);
    }

    add_current_string();

    return Parse(command, args);
}

TResult<CommandLineArguments> CommandLineParser::Parse(int argc, char** argv) const
{
    if (argc < 1)
    {
        return HYP_MAKE_ERROR(Error, "No command line arguments provided");
    }

    Array<String> args;

    for (int i = 1; i < argc; i++)
    {
        args.PushBack(argv[i]);
    }

    return Parse(argv[0], args);
}

TResult<CommandLineArguments> CommandLineParser::Parse(const String& command, const Array<String>& args) const
{
    if (!m_definitions)
    {
        return HYP_MAKE_ERROR(Error, "No command line argument definitions");
    }

    CommandLineArguments result;

    HashSet<String> used_arguments;

    for (SizeType i = 0; i < args.Size(); i++)
    {
        String arg = args[i];

        Array<String> parts = arg.Split('=');
        arg = parts[0];

        if (arg.StartsWith("--"))
        {
            arg = arg.Substr(2);
        }
        else if (arg.StartsWith("-"))
        {
            arg = arg.Substr(1);
        }
        else
        {
            return HYP_MAKE_ERROR(Error, "Invalid argument");
        }

        const auto it = m_definitions->Find(arg);

        if (it == m_definitions->End())
        {
            // Unknown argument
            continue;
        }

        used_arguments.Insert(it->name);

        const bool allow_multiple = it->flags[CommandLineArgumentFlags::ALLOW_MULTIPLE];

        if (parts.Size() > 1)
        {
            TResult<CommandLineArgumentValue> parsed_value = CommandLineArguments::ParseArgumentValue(*it, parts[1]);

            if (parsed_value.HasError())
            {
                return parsed_value.GetError();
            }

            AppendCommandLineArgumentValue(result.m_values, arg, std::move(parsed_value.GetValue()), allow_multiple);

            continue;
        }

        if (it->type == CommandLineArgumentType::BOOLEAN)
        {
            AppendCommandLineArgumentValue(result.m_values, arg, CommandLineArgumentValue { true }, allow_multiple);

            continue;
        }

        if (i + 1 >= args.Size())
        {
            return HYP_MAKE_ERROR(Error, "Missing value for argument");
        }

        TResult<CommandLineArgumentValue> parsed_value = CommandLineArguments::ParseArgumentValue(*it, args[++i]);

        if (parsed_value.HasError())
        {
            return parsed_value.GetError();
        }

        AppendCommandLineArgumentValue(result.m_values, arg, std::move(parsed_value.GetValue()), allow_multiple);
    }

    for (const CommandLineArgumentDefinition& def : *m_definitions)
    {
        const bool allow_multiple = def.flags[CommandLineArgumentFlags::ALLOW_MULTIPLE];

        if (used_arguments.Contains(def.name))
        {
            continue;
        }

        if (def.default_value.HasValue())
        {
            AppendCommandLineArgumentValue(result.m_values, def.name, *def.default_value, allow_multiple);

            continue;
        }

        if (def.flags[CommandLineArgumentFlags::REQUIRED])
        {
            return HYP_MAKE_ERROR(Error, "Missing required argument");
        }
    }

    return result;
}

#pragma endregion CommandLineParser

} // namespace cli
} // namespace hyperion