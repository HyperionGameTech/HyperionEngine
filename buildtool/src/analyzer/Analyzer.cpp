/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <analyzer/Analyzer.hpp>

namespace hyperion {
namespace buildtool {

const HypClassDefinition *Analyzer::FindHypClassDefinition(UTF8StringView class_name) const
{
    Mutex::Guard guard(m_mutex);

    for (const UniquePtr<Module> &module : m_modules) {
        const HypClassDefinition *hyp_class = module->FindHypClassDefinition(class_name);

        if (hyp_class) {
            return hyp_class;
        }
    }

    return nullptr;
}

Module *Analyzer::AddModule(const FilePath &path)
{
    Mutex::Guard guard(m_mutex);

    return m_modules.PushBack(MakeUnique<Module>(path)).Get();
}

} // namespace buildtool
} // namespace hyperion