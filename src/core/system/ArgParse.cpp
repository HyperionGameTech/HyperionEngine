/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/system/ArgParse.hpp>

#include <util/StringUtil.hpp>

namespace hyperion {
namespace sys {

#pragma region CommandLineArguments

const CommandLineArgumentValue &CommandLineArguments::operator[](UTF8StringView key) const
{
    static const CommandLineArgumentValue empty_value = json::JSONUndefined();

    const auto it = Find(key);

    if (it == End()) {
        return empty_value;
    }

    return it->second;
}

#pragma endregion CommandLineArguments

#pragma region ArgParse

void ArgParse::Add(
    String name,
    String shorthand,
    EnumFlags<ArgFlags> flags,
    CommandLineArgumentType type,
    CommandLineArgumentValue default_value
)
{
    auto it = m_definitions.FindIf([&name](const auto &item)
    {
        return item.name == name;
    });

    if (it != m_definitions.End()) {
        *it = ArgumentDefinition {
            std::move(name),
            shorthand.Empty() ? Optional<String>() : std::move(shorthand),
            flags,
            type,
            std::move(default_value)
        };

        return;
    }

    m_definitions.PushBack(ArgumentDefinition {
        std::move(name),
        shorthand.Empty() ? Optional<String>() : std::move(shorthand),
        flags,
        type,
        std::move(default_value)
    });
}

void ArgParse::Add(
    String name,
    String shorthand,
    EnumFlags<ArgFlags> flags,
    Optional<Array<String>> enum_values,
    CommandLineArgumentValue default_value
)
{
    auto it = m_definitions.FindIf([&name](const auto &item)
    {
        return item.name == name;
    });

    if (it != m_definitions.End()) {
        *it = ArgumentDefinition {
            std::move(name),
            shorthand.Empty() ? Optional<String>() : std::move(shorthand),
            flags,
            CommandLineArgumentType::ENUM,
            std::move(default_value),
            std::move(enum_values)
        };

        return;
    }

    m_definitions.PushBack(ArgumentDefinition {
        std::move(name),
        shorthand.Empty() ? Optional<String>() : std::move(shorthand),
        flags,
        CommandLineArgumentType::ENUM,
        std::move(default_value),
        std::move(enum_values)
    });
}

ArgParse::ParseResult ArgParse::Parse(int argc, char **argv) const
{
    Array<String> args;

    for (int i = 1; i < argc; i++) {
        args.PushBack(argv[i]);
    }

    return Parse(argv[0], args);
}

ArgParse::ParseResult ArgParse::Parse(const String &command, const Array<String> &args) const
{
    ParseResult result;
    result.ok = true;
    result.result.command = command;

    FlatSet<String> used_arguments;

    auto ParseArgument = [](const ArgumentDefinition &argument_definition, const String &str) -> Variant<CommandLineArgumentValue, CommandLineArgumentError>
    {
        const CommandLineArgumentType type = argument_definition.type;

        json::ParseResult parse_result = json::JSON::Parse(str);

        if (!parse_result.ok) {
            return CommandLineArgumentError { "Failed to parse argument" };
        }

        switch (type) {
        case CommandLineArgumentType::STRING:
            return json::JSONValue(parse_result.value.ToString());
        case CommandLineArgumentType::INTEGER:
            return json::JSONValue(parse_result.value.ToInt32());
        case CommandLineArgumentType::FLOAT:
            return json::JSONValue(parse_result.value.ToFloat());
        case CommandLineArgumentType::BOOLEAN:
            return json::JSONValue(parse_result.value.ToBool());
        case CommandLineArgumentType::ENUM: {
            const json::JSONString string_value = parse_result.value.ToString();

            const Array<String> *enum_values = argument_definition.enum_values.TryGet();

            if (!enum_values) {
                return CommandLineArgumentError { "Internal error parsing enum argument" };
            }

            if (!enum_values->Contains(string_value)) {
                return CommandLineArgumentError { "Not a valid value for argument" };
            }

            return json::JSONValue(string_value);
        }
        }

        return CommandLineArgumentError { "Invalid argument" };
    };

    for (SizeType i = 0; i < args.Size(); i++) {
        String arg = args[i];

        Array<String> parts = arg.Split('=');
        arg = parts[0];

        if (arg.StartsWith("--")) {
            arg = arg.Substr(2);
        } else if (arg.StartsWith("-")) {
            arg = arg.Substr(1);
        } else {
            result.ok = false;
            result.message = "Invalid argument: " + arg;

            return result;
        }

        auto it = m_definitions.FindIf([&arg](const auto &item)
        {
            return item.name == arg;
        });

        if (it == m_definitions.End()) {
            // Unknown argument
            continue;
        }

        used_arguments.Insert(it->name);

        if (parts.Size() > 1) {
            Variant<CommandLineArgumentValue, CommandLineArgumentError> parsed_value = ParseArgument(*it, parts[1]);

            if (CommandLineArgumentError *error = parsed_value.TryGet<CommandLineArgumentError>()) {
                result.ok = false;
                result.message = error->message;

                return result;
            }

            result.result.values.EmplaceBack(arg, std::move(parsed_value.Get<CommandLineArgumentValue>()));

            continue;
        }

        if (it->type == CommandLineArgumentType::BOOLEAN) {
            result.result.values.EmplaceBack(arg, CommandLineArgumentValue { true });

            continue;
        }

        if (i + 1 >= args.Size()) {
            result.ok = false;
            result.message = "Missing value for argument: " + arg;

            return result;
        }

        Variant<CommandLineArgumentValue, CommandLineArgumentError> parsed_value = ParseArgument(*it, args[++i]);

        if (CommandLineArgumentError *error = parsed_value.TryGet<CommandLineArgumentError>()) {
            result.ok = false;
            result.message = error->message;

            return result;
        }

        result.result.values.EmplaceBack(arg, std::move(parsed_value.Get<CommandLineArgumentValue>()));
    }

    for (const ArgumentDefinition &def : m_definitions) {
        if (used_arguments.Contains(def.name)) {
            continue;
        }

        if (def.default_value.HasValue()) {
            result.result.values.EmplaceBack(def.name, *def.default_value);

            continue;
        }

        if (def.flags & ArgFlags::REQUIRED) {
            result.ok = false;
            result.message = "Missing required argument: " + def.name;

            return result;
        }
    }

    return result;
}

#pragma endregion ArgParse

} // namespace sys
} // namespace hyperion