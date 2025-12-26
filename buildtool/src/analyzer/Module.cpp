/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <analyzer/Module.hpp>

#include <core/functional/Proc.hpp>

#include <core/logging/Logger.hpp>

namespace Hyperion {
namespace CodeGen {

HYP_DECLARE_LOG_CHANNEL(BuildTool);

Module::Module(const FilePath& path)
    : m_path(path)
{
}

Result Module::AddClassDefinition(ClassDefinition&& classDefinition)
{
    Mutex::Guard guard(m_mutex);

    auto it = m_classes.Find(classDefinition.name);

    if (it != m_classes.End())
    {
        return HYP_MAKE_ERROR(Error, "ClassDefinition already exists");
    }

    it = m_classes.Insert(classDefinition.name, std::move(classDefinition)).first;
    it->second.declModule = this;

    return {};
}

const ClassDefinition* Module::FindClassDefinition(UTF8StringView className) const
{
    Mutex::Guard guard(m_mutex);

    const auto it = m_classes.Find(className);

    if (it == m_classes.End())
    {
        return nullptr;
    }

    return &it->second;
}

} // namespace CodeGen
} // namespace Hyperion