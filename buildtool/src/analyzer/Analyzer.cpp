/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <analyzer/Analyzer.hpp>

namespace hyperion {
namespace buildtool {
    
Module *Analyzer::AddModule(const FilePath &path)
{
    Mutex::Guard guard(m_mutex);

    return m_modules.PushBack(MakeUnique<Module>(path)).Get();
}

} // namespace buildtool
} // namespace hyperion