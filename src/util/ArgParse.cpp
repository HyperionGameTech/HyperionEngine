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

void ArgParse::Add(String name, String shorthand, ArgumentType type, Bool required)
{
    auto it = m_definitions.FindIf([&name](const auto &item)
    {
        return item.name == name;
    });

    if (it != m_definitions.End()) {
        *it = ArgumentDefinition {
            std::move(name),
            shorthand.Empty() ? Optional<String>() : std::move(shorthand),
            type,
            required
        };

        return;
    }

    m_definitions.PushBack(ArgumentDefinition {
        std::move(name),
        shorthand.Empty() ? Optional<String>() : std::move(shorthand),
        type,
        required
    });
}

ArgParse::Result ArgParse::Parse(Int argc, char **argv) const
{
    Array<String> args;

    for (Int i = 0; i < argc; i++) {
        args.PushBack(argv[i]);
    }

    return Parse(args);
}

ArgParse::Result ArgParse::Parse(const Array<String> &args) const
{
    Result result;
    result.ok = true;

    FlatSet<String> used_arguments;

    auto ParseArgument = [](const String &str, ArgumentType type) -> Optional<ArgumentValue>
    {
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
            result.message = "Unknown argument: " + arg;

            return result;
        }

        auto it = m_definitions.FindIf([&arg](const auto &item)
        {
            return item.name == arg;
        });

        if (it == m_definitions.End()) {
            result.ok = false;
            result.message = "Unknown argument: " + arg;

            return result;
        }

        used_arguments.Insert(it->name);

        if (parts.Size() > 1) {
            Optional<ArgumentValue> parsed_value = ParseArgument(parts[1], it->type);

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

        Optional<ArgumentValue> parsed_value = ParseArgument(args[++i], it->type);

        if (!parsed_value.HasValue()) {
            result.ok = false;
            result.message = "Invalid value for argument: " + arg;

            return result;
        }

        result.values.PushBack({ arg, std::move(parsed_value.Get()) });
    }

    for (const ArgumentDefinition &def : m_definitions) {
        if (def.required && !used_arguments.Contains(def.name)) {
            result.ok = false;
            result.message = "Missing required argument: " + def.name;

            return result;
        }
    }

    return result;
}

} // namespace hyperion