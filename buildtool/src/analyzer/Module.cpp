/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <analyzer/Module.hpp>

#include <core/functional/Proc.hpp>

namespace hyperion {
namespace buildtool {

Module::Module(const FilePath& path)
    : m_path(path)
{
}

Result Module::AddHypClassDefinition(HypClassDefinition&& hypClassDefinition)
{
    Mutex::Guard guard(m_mutex);

    auto it = m_hypClasses.Find(hypClassDefinition.name);

    if (it != m_hypClasses.End())
    {
        return HYP_MAKE_ERROR(Error, "HypClassDefinition already exists");
    }

    m_hypClasses.Insert(hypClassDefinition.name, std::move(hypClassDefinition));

    return {};
}

const HypClassDefinition* Module::FindHypClassDefinition(UTF8StringView className) const
{
    Mutex::Guard guard(m_mutex);

    const auto it = m_hypClasses.Find(className);

    if (it == m_hypClasses.End())
    {
        return nullptr;
    }

    return &it->second;
}

} // namespace buildtool
} // namespace hyperion