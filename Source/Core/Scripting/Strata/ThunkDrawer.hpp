/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>

#include <Core/Containers/Map.hpp>

#include <Core/Name/Name.hpp>

namespace Hyperion {
namespace Strata {

/// A registry for C-style bindings (thunks) used by the strata language to call into the engine.
/// ---
/// Methods are registered once at static initialization.
/// Thread safe to use Resolve() only, permitted that no post-init Register() calls happen.
class CORE_API ThunkDrawer
{
public:
    static ThunkDrawer& GetInstance();

    static bool Register(StringHash nameHash, void* fn)
    {
        GetInstance().DoInsert(nameHash, fn);

        return true;
    }

    static void* Resolve(StringHash nameHash)
    {
        return GetInstance().DoLookup(nameHash);
    }

private:
    ThunkDrawer() = default;

    void DoInsert(StringHash nameHash, void* fn)
    {
        m_entries[nameHash] = fn;
    }

    void* DoLookup(StringHash nameHash) const
    {
        if (auto it = m_entries.Find(nameHash); it != m_entries.End())
        {
            return it->second;
        }

        return nullptr;
    }

    Map<StringHash, void*> m_entries;
};

} // namespace Strata
} // namespace Hyperion
