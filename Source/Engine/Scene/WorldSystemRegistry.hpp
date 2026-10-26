/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Functional/Proc.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Core/Utilities/EnumFlags.hpp>

#include <Scene/System.hpp>
#include <Scene/World.hpp>

namespace Hyperion {

class Class;

struct WorldSystemEntry
{
    uint32 id = 0;
    const Class* systemClass = nullptr;

    EnumFlags<WorldFlags> requiredFlags = WorldFlags::None;
    EnumFlags<WorldFlags> excludedFlags = WorldFlags::None;

    Proc<bool(const World&)> predicate;

    bool AppliesTo(const World& world, EnumFlags<WorldFlags> worldFlags) const
    {
        return (worldFlags & requiredFlags) == requiredFlags
            && !(worldFlags & excludedFlags)
            && (!predicate.IsValid() || predicate(world));
    }
};

/// Registry for the systems a World adds upon World::Initialize()
class ENGINE_API WorldSystemRegistry
{
public:
    static WorldSystemRegistry& GetInstance();

    WorldSystemRegistry();

    template <class SystemType>
    uint32 Register(EnumFlags<WorldFlags> requiredFlags = WorldFlags::None, EnumFlags<WorldFlags> excludedFlags = WorldFlags::None, Proc<bool(const World&)>&& predicate = {})
    {
        WorldSystemEntry entry;
        entry.systemClass = SystemType::StaticClass();
        entry.requiredFlags = requiredFlags;
        entry.excludedFlags = excludedFlags;
        entry.predicate = std::move(predicate);

        return Register(std::move(entry));
    }

    uint32 Register(WorldSystemEntry&& entry);
    void Unregister(uint32 id);

    bool IsRegistered(const Class* systemClass) const;

    void ForEach(const Proc<void(const WorldSystemEntry&)>& proc) const;

private:
    mutable Mutex m_mutex;
    Array<WorldSystemEntry> m_entries;
    uint32 m_nextId = 1;
};

} // namespace Hyperion
