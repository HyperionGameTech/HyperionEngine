/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/system/ArgParse.hpp>

#include <util/StringUtil.hpp>

namespace hyperion {
namespace sys {

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

    auto ParseArgument = [](const ArgumentDefinition &argument_definition, const String &str) -> Optional<CommandLineArgumentValue>
    {
        const CommandLineArgumentType type = argument_definition.type;

        switch (type) {
        case CommandLineArgumentType::STRING:
            return CommandLineArgumentValue { str };
        case CommandLineArgumentType::INTEGER: {
            int i = 0;

            if (!StringUtil::Parse(str, &i)) {
                return { };
            }

            return CommandLineArgumentValue { i };
        }
        case CommandLineArgumentType::FLOAT: {
            float f = 0.0f;

            if (!StringUtil::Parse(str, &f)) {
                return { };
            }

            return CommandLineArgumentValue { f };
        }
        case CommandLineArgumentType::BOOLEAN:
            return CommandLineArgumentValue { str == "true" };
        case CommandLineArgumentType::ENUM: {
            const Array<String> *enum_values = argument_definition.enum_values.TryGet();

            if (!enum_values) {
                return { };
            }

            if (!enum_values->Contains(str)) {
                return { };
            }

            return CommandLineArgumentValue { str };
        }
        }

        return { };
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
            Optional<CommandLineArgumentValue> parsed_value = ParseArgument(*it, parts[1]);

            if (!parsed_value.HasValue()) {
                result.ok = false;
                result.message = "Invalid value for argument: " + arg;

                return result;
            }

            result.result.values.PushBack({ arg, std::move(parsed_value.Get()) });

            continue;
        }

        if (it->type == CommandLineArgumentType::BOOLEAN) {
            result.result.values.PushBack({ arg, CommandLineArgumentValue { true } });

            continue;
        }

        if (i + 1 >= args.Size()) {
            result.ok = false;
            result.message = "Missing value for argument: " + arg;

            return result;
        }

        Optional<CommandLineArgumentValue> parsed_value = ParseArgument(*it, args[++i]);

        if (!parsed_value.HasValue()) {
            result.ok = false;
            result.message = "Invalid value for argument: " + arg;

            return result;
        }

        result.result.values.PushBack({ arg, std::move(parsed_value.Get()) });
    }

    for (const ArgumentDefinition &def : m_definitions) {
        if (used_arguments.Contains(def.name)) {
            continue;
        }

        if (def.default_value.HasValue()) {
            result.result.values.PushBack({ def.name, def.default_value });

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

} // namespace sys
} // namespace hyperion