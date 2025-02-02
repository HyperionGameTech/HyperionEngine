/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <analyzer/Module.hpp>

namespace hyperion {
namespace buildtool {

Module::Module(const FilePath &path)
    : m_path(path)
{
}

Result<void> Module::AddHypClassDefinition(HypClassDefinition &&hyp_class_definition)
{
    Mutex::Guard guard(m_mutex);

    auto it = m_hyp_classes.Find(hyp_class_definition.name);

    if (it != m_hyp_classes.End()) {
        return HYP_MAKE_ERROR(Error, "HypClassDefinition already exists");
    }

    m_hyp_classes.Insert(hyp_class_definition.name, std::move(hyp_class_definition));

    return { };
}

const HypClassDefinition *Module::FindHypClassDefinition(UTF8StringView class_name) const
{
    Mutex::Guard guard(m_mutex);

    const auto it = m_hyp_classes.Find(class_name);

    if (it == m_hyp_classes.End()) {
        return nullptr;
    }

    return &it->second;
}

} // namespace buildtool
} // namespace hyperion