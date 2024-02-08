#include <util/ArgParse.hpp>
#include <util/StringUtil.hpp>

namespace hyperion {

ArgParse::ArgumentValue ArgParse::Result::operator[](const String &key) const
{
    auto it = values.FindIf([&key](const auto &item)
    {
        return item.first == key;
    });

    if (it == values.End()) {
        return { };
    }

    return it->second;
}

void ArgParse::Add(
    String name,
    String shorthand,
    ArgFlags flags,
    ArgumentType type,
    ArgumentValue default_value
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
    ArgFlags flags,
    Optional<Array<String>> enum_values,
    ArgumentValue default_value
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
            ArgumentType::ARGUMENT_TYPE_ENUM,
            std::move(default_value),
            std::move(enum_values)
        };

        return;
    }

    m_definitions.PushBack(ArgumentDefinition {
        std::move(name),
        shorthand.Empty() ? Optional<String>() : std::move(shorthand),
        flags,
        ArgumentType::ARGUMENT_TYPE_ENUM,
        std::move(default_value),
        std::move(enum_values)
    });
}

ArgParse::Result ArgParse::Parse(Int argc, char **argv) const
{
    Array<String> args;

    for (Int i = 1; i < argc; i++) {
        args.PushBack(argv[i]);
    }

    return Parse(args);
}

ArgParse::Result ArgParse::Parse(const Array<String> &args) const
{
    Result result;
    result.ok = true;

    FlatSet<String> used_arguments;

    auto ParseArgument = [](const ArgumentDefinition &argument_definition, const String &str) -> Optional<ArgumentValue>
    {
        const ArgumentType type = argument_definition.type;

        switch (type) {
        case ArgumentType::ARGUMENT_TYPE_STRING:
            return ArgumentValue { str };
        case ArgumentType::ARGUMENT_TYPE_INT: {
            Int i = 0;

            if (!StringUtil::Parse(str, &i)) {
                return { };
            }

            return ArgumentValue { i };
        }
        case ArgumentType::ARGUMENT_TYPE_FLOAT: {
            Float f = 0.0f;

            if (!StringUtil::Parse(str, &f)) {
                return { };
            }

            return ArgumentValue { f };
        }
        case ArgumentType::ARGUMENT_TYPE_BOOL:
            return ArgumentValue { str == "true" };
        case ArgumentType::ARGUMENT_TYPE_ENUM: {
            const Array<String> *enum_values = argument_definition.enum_values.TryGet();

            if (!enum_values) {
                return { };
            }

            if (!enum_values->Contains(str)) {
                return { };
            }

            return ArgumentValue { str };
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
            Optional<ArgumentValue> parsed_value = ParseArgument(*it, parts[1]);

            if (!parsed_value.HasValue()) {
                result.ok = false;
                result.message = "Invalid value for argument: " + arg;

                return result;
            }

            result.values.PushBack({ arg, std::move(parsed_value.Get()) });

            continue;
        }

        if (it->type == ArgumentType::ARGUMENT_TYPE_BOOL) {
            result.values.PushBack({ arg, ArgumentValue { true } });

            continue;
        }

        if (i + 1 >= args.Size()) {
            result.ok = false;
            result.message = "Missing value for argument: " + arg;

            return result;
        }

        Optional<ArgumentValue> parsed_value = ParseArgument(*it, args[++i]);

        if (!parsed_value.HasValue()) {
            result.ok = false;
            result.message = "Invalid value for argument: " + arg;

            return result;
        }

        result.values.PushBack({ arg, std::move(parsed_value.Get()) });
    }

    for (const ArgumentDefinition &def : m_definitions) {
        if (used_arguments.Contains(def.name)) {
            continue;
        }

        if (def.default_value.HasValue()) {
            result.values.PushBack({ def.name, def.default_value });

            continue;
        }

        if (def.flags & ARG_FLAGS_REQUIRED) {
            result.ok = false;
            result.message = "Missing required argument: " + def.name;

            return result;
        }
    }

    return result;
}

} // namespace hyperion