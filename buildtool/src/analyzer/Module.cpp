/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <analyzer/Module.hpp>

#include <core/functional/Proc.hpp>

namespace hyperion {
namespace buildtool {

Module::Module(const FilePath& path)
    : m_path(path)
{
}

Result Module::AddHypClassDefinition(HypClassDefinition&& hyp_class_definition)
{
    Mutex::Guard guard(m_mutex);

    auto it = m_hyp_classes.Find(hyp_class_definition.name);

    if (it != m_hyp_classes.End())
    {
        return HYP_MAKE_ERROR(Error, "HypClassDefinition already exists");
    }

    m_hyp_classes.Insert(hyp_class_definition.name, std::move(hyp_class_definition));

    return {};
}

const HypClassDefinition* Module::FindHypClassDefinition(UTF8StringView class_name) const
{
    Mutex::Guard guard(m_mutex);

    const auto it = m_hyp_classes.Find(class_name);

    if (it == m_hyp_classes.End())
    {
        return nullptr;
    }

    return &it->second;
}

bool Module::HasBaseClass(const HypClassDefinition& hyp_class_definition, UTF8StringView base_class_name) const
{
    Mutex::Guard guard(m_mutex);

    auto find_definition = [this](UTF8StringView name) -> const HypClassDefinition*
    {
        auto it = m_hyp_classes.Find(name);

        if (it != m_hyp_classes.End())
        {
            return &it->second;
        }

        return nullptr;
    };

    Proc<bool(const HypClassDefinition&, UTF8StringView)> perform_check;

    perform_check = [&perform_check, &find_definition](const HypClassDefinition& hyp_class_definition, UTF8StringView base_class_name) -> bool
    {
        auto it = hyp_class_definition.base_class_names.FindAs(base_class_name);

        if (it != hyp_class_definition.base_class_names.End())
        {
            return true;
        }

        for (const String& base_class : hyp_class_definition.base_class_names)
        {
            const HypClassDefinition* base_class_definition = find_definition(base_class);

            if (base_class_definition && perform_check(*base_class_definition, base_class_name))
            {
                return true;
            }
        }

        return false;
    };

    return perform_check(hyp_class_definition, base_class_name);
}

} // namespace buildtool
} // namespace hyperion