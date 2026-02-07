/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/cli/CommandLine.hpp>

#include <core/logging/Logger.hpp>

#include <core/containers/HashSet.hpp>

#include <core/utilities/StringUtil.hpp>

#ifndef HYP_BUILDTOOL
#include <CommandLine.generated.inl>
#endif

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Core);
HYP_DEFINE_LOG_SUBCHANNEL(CommandLine, Core);

namespace cli {

static void AppendCommandLineArgumentValue(
    Array<Pair<String, CommandLineArgumentValue>>& values,
    const String& key,
    CommandLineArgumentValue&& value,
    bool allowMultiple)
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

        if (allowMultiple)
        {
            if (it->second.IsArray())
            {
                it->second.AsArray().PushBack(std::move(value));
            }
            else
            {
                JSON::JArray array;
                array.PushBack(std::move(it->second));
                array.PushBack(std::move(value));

                it->second = JSON::Value(std::move(array));
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

static void AppendCommandLineArgumentValue(
    Array<Pair<String, CommandLineArgumentValue>>& values,
    const String& key,
    const CommandLineArgumentValue& value,
    bool allowMultiple)
{
    AppendCommandLineArgumentValue(values, key, CommandLineArgumentValue(value), allowMultiple);
}

#pragma region CommandLineArguments

const CommandLineArgumentValue& CommandLineArguments::operator[](UTF8StringView key) const
{
    static const CommandLineArgumentValue emptyValue = JSON::JSUndefined();

    const auto it = Find(key);

    if (it == End())
    {
        return emptyValue;
    }

    return it->second;
}

CommandLineArguments CommandLineArguments::Merge(const CommandLineArgumentDefinitions& definitions, const CommandLineArguments& a, const CommandLineArguments& b)
{
    CommandLineArguments result = a;

    for (const Pair<String, CommandLineArgumentValue>& element : b)
    {
        bool allowMultiple = false;

        const auto definitionsIt = definitions.Find(element.first);

        if (definitionsIt != definitions.End())
        {
            allowMultiple = definitionsIt->flags[CommandLineArgumentFlags::ALLOW_MULTIPLE];
        }

        AppendCommandLineArgumentValue(result.m_values, element.first, element.second, allowMultiple);
    }

    return result;
}

TResult<CommandLineArgumentValue> CommandLineArguments::ParseArgumentValue(const CommandLineArgumentDefinition& definition, const String& str)
{
    const CommandLineArgumentType type = definition.type;

    JSON::ParseResult parseResult = JSON::Parse(str);
    JSON::Value value = std::move(parseResult.value);

    if (!parseResult.ok)
    {
        // If string, allow unquoted on parse error
        if (type == CommandLineArgumentType::STRING)
        {
            return JSON::Value(JSON::JString(str));
        }

        HYP_LOG(CommandLine, Error, "Failed to parse argument \"{}\": {}", str, parseResult.message);

        return HYP_MAKE_ERROR(Error, "Failed to parse argument");
    }

    switch (type)
    {
    case CommandLineArgumentType::STRING:
        return JSON::Value(value.ToString());
    case CommandLineArgumentType::INTEGER:
    {
        int32 valueInt;

        if (value.IsNumber())
        {
            return JSON::Value(value.ToInt32());
        }

        if (!StringUtil::Parse(value.ToString(), &valueInt))
        {
            return JSON::Value(valueInt);
        }

        return HYP_MAKE_ERROR(Error, "Failed to parse integer argument");
    }
    case CommandLineArgumentType::FLOAT:
    {
        double valueDouble;

        if (value.IsNumber())
        {
            return JSON::Value(value.ToDouble());
        }

        if (!StringUtil::Parse(value.ToString(), &valueDouble))
        {
            return JSON::Value(valueDouble);
        }

        return HYP_MAKE_ERROR(Error, "Failed to parse float argument");
    }
    case CommandLineArgumentType::BOOLEAN:
        return JSON::Value(value.ToBool());
    case CommandLineArgumentType::ENUM:
    {
        JSON::JString stringValue = value.ToString();

        const Array<String>* enumValues = definition.enumValues.TryGet();

        if (!enumValues)
        {
            return HYP_MAKE_ERROR(Error, "Internal error parsing enum argument");
        }

        if (!enumValues->Contains(stringValue))
        {
            return HYP_MAKE_ERROR(Error, "Not a valid value for argument");
        }

        return JSON::Value(std::move(stringValue));
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

CommandLineArgumentDefinitions::CommandLineArgumentDefinitions(const CommandLineArgumentDefinitions& other)
    : m_impl(MakePimpl<CommandLineArgumentDefinitionsImpl>(other.m_impl ? *other.m_impl : CommandLineArgumentDefinitionsImpl()))
{
}

CommandLineArgumentDefinitions& CommandLineArgumentDefinitions::operator=(const CommandLineArgumentDefinitions& other)
{
    if (this == &other)
    {
        return *this;
    }

    if (other.m_impl)
    {
        if (!m_impl)
        {
            m_impl = MakePimpl<CommandLineArgumentDefinitionsImpl>();
        }

        *m_impl = *other.m_impl;
    }
    else
    {
        m_impl.Reset();
    }

    return *this;
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
    const CommandLineArgumentValue& defaultValue)
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
            defaultValue
        };

        return *this;
    }

    m_impl->definitions.PushBack(CommandLineArgumentDefinition {
        name,
        shorthand.Empty() ? Optional<String>() : shorthand,
        description.Empty() ? Optional<String>() : description,
        flags,
        type,
        defaultValue });

    return *this;
}

CommandLineArgumentDefinitions& CommandLineArgumentDefinitions::Add(
    const String& name,
    const String& shorthand,
    const String& description,
    EnumFlags<CommandLineArgumentFlags> flags,
    const Optional<Array<String>>& enumValues,
    const CommandLineArgumentValue& defaultValue)
{
    if (!m_impl)
    {
        m_impl = MakePimpl<CommandLineArgumentDefinitionsImpl>();
    }

    auto it = m_impl->definitions.FindIf([&name](const auto& item)
        {
            return item.name == name;
        });

    if (it == m_impl->definitions.End())
    {
        it = &m_impl->definitions.EmplaceBack();
    }

    *it = {};
    it->name = name;
    it->type = CommandLineArgumentType::ENUM;
    it->shorthand = shorthand.Empty() ? Optional<String>() : shorthand;
    it->description = description.Empty() ? Optional<String>() : description;
    it->flags = flags;
    it->defaultValue = defaultValue;
    it->enumValues = enumValues;

    return *this;
}

#pragma endregion CommandLineArgumentDefinitions

#pragma region CommandLineParser

TResult<CommandLineArguments> CommandLineParser::Parse(const String& commandLine) const
{
    String command;
    Array<String> args;

    int currentStringIndex = 0;
    String currentString;

    auto addCurrentString = [&]()
    {
        if (currentString.Any())
        {
            if (currentStringIndex++ == 0)
            {
                command = std::move(currentString);
            }
            else
            {
                args.PushBack(std::move(currentString));
            }
        }
    };

    for (SizeType charIndex = 0; charIndex < commandLine.Length(); charIndex++)
    {
        uint32 currentChar = commandLine.GetChar(charIndex);

        if (std::isspace(int(currentChar)))
        {
            addCurrentString();

            continue;
        }

        if (currentChar == uint32('"'))
        {
            do
            {
                currentString.Append(currentChar);

                ++charIndex;
            }
            while (charIndex < commandLine.Length() && (currentChar = commandLine.GetChar(charIndex)) != uint32('"'));
        }
        else if (currentChar == uint32('\''))
        {
            do
            {
                currentString.Append(currentChar);

                ++charIndex;
            }
            while (charIndex < commandLine.Length() && (currentChar = commandLine.GetChar(charIndex)) != uint32('\''));
        }

        currentString.Append(currentChar);
    }

    addCurrentString();

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

    CommandLineArguments result { command };

    HashSet<String> usedArguments;

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

        usedArguments.Insert(it->name);

        const bool allowMultiple = it->flags[CommandLineArgumentFlags::ALLOW_MULTIPLE];

        if (parts.Size() > 1)
        {
            TResult<CommandLineArgumentValue> parsedValue = CommandLineArguments::ParseArgumentValue(*it, parts[1]);

            if (parsedValue.HasError())
            {
                return parsedValue.GetError();
            }

            AppendCommandLineArgumentValue(result.m_values, arg, std::move(parsedValue.GetValue()), allowMultiple);

            continue;
        }

        if (it->type == CommandLineArgumentType::BOOLEAN)
        {
            AppendCommandLineArgumentValue(result.m_values, arg, CommandLineArgumentValue { true }, allowMultiple);

            continue;
        }

        if (i + 1 >= args.Size())
        {
            return HYP_MAKE_ERROR(Error, "Missing value for argument: {}", arg);
        }

        TResult<CommandLineArgumentValue> parsedValue = CommandLineArguments::ParseArgumentValue(*it, args[++i]);

        if (parsedValue.HasError())
        {
            return parsedValue.GetError();
        }

        AppendCommandLineArgumentValue(result.m_values, arg, std::move(parsedValue.GetValue()), allowMultiple);
    }

    for (const CommandLineArgumentDefinition& def : *m_definitions)
    {
        const bool allowMultiple = def.flags[CommandLineArgumentFlags::ALLOW_MULTIPLE];

        if (usedArguments.Contains(def.name))
        {
            continue;
        }

        if (def.flags[CommandLineArgumentFlags::REQUIRED] && (!def.defaultValue.HasValue() || def.defaultValue->IsNullOrUndefined()))
        {
            return HYP_MAKE_ERROR(Error, "Missing value for required argument: {}", def.name);
        }

        if (def.defaultValue.HasValue())
        {
            AppendCommandLineArgumentValue(result.m_values, def.name, *def.defaultValue, allowMultiple);

            continue;
        }
    }

    return result;
}

#pragma endregion CommandLineParser

} // namespace cli
} // namespace Hyperion
