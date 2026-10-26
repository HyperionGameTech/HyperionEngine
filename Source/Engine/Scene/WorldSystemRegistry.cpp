/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Scene/WorldSystemRegistry.hpp>

#include <Scene/Systems/VisibilityStateUpdaterSystem.hpp>
#include <Scene/Systems/LightmapSystem.hpp>
#include <Scene/Systems/AnimationSystem.hpp>
#include <Scene/Systems/BoneAttachmentSystem.hpp>
#include <Scene/Systems/CharacterModelSystem.hpp>
#include <Scene/Systems/AudioSystem.hpp>
#include <Scene/Systems/PhysicsSystem.hpp>
#include <Scene/Systems/CameraSystem.hpp>
#include <Scene/Systems/CharacterControllerSystem.hpp>
#include <Scene/Systems/PlayerSystem.hpp>
#include <Scene/Systems/ScriptSystem.hpp>
#include <Scene/Systems/InstanceGroupSystem.hpp>
#include <Scene/Systems/TerrainLodSystem.hpp>
#include <Scene/Systems/SwatchOverrideSystem.hpp>

#include <Scene/Systems/Glimmer/GlimmerSystem.hpp>

#include <Framework/EngineGlobals.hpp>

namespace Hyperion {

WorldSystemRegistry& WorldSystemRegistry::GetInstance()
{
    static WorldSystemRegistry s_instance;

    return s_instance;
}

WorldSystemRegistry::WorldSystemRegistry()
{
    // Default systems for all worlds
    Register<PhysicsSystem>(WorldFlags::HasPhysics);
    Register<VisibilityStateUpdaterSystem>();
    Register<LightmapSystem>();
    Register<CharacterModelSystem>();
    Register<AnimationSystem>();
    Register<BoneAttachmentSystem>();
    Register<AudioSystem>();
    Register<ScriptSystem>();
    Register<InstanceGroupSystem>();
    Register<TerrainLodSystem>();
    Register<GlimmerSystem>(WorldFlags::None, WorldFlags::None, [](const World&)
        {
            return !EngineGlobals::IsHeadless();
        });
    Register<CameraSystem>();
    Register<SwatchOverrideSystem>();
    Register<CharacterControllerSystem>(WorldFlags::None, WorldFlags::Editor);
    Register<PlayerSystem>(WorldFlags::None, WorldFlags::Editor);
}

uint32 WorldSystemRegistry::Register(WorldSystemEntry&& entry)
{
    Mutex::Guard guard(m_mutex);

    entry.id = m_nextId++;

    const uint32 id = entry.id;

    m_entries.PushBack(std::move(entry));

    return id;
}

void WorldSystemRegistry::Unregister(uint32 id)
{
    Mutex::Guard guard(m_mutex);

    auto it = m_entries.FindIf([id](const WorldSystemEntry& entry)
        {
            return entry.id == id;
        });

    if (it != m_entries.End())
    {
        m_entries.Erase(it);
    }
}

bool WorldSystemRegistry::IsRegistered(const Class* systemClass) const
{
    Mutex::Guard guard(m_mutex);

    return m_entries.FindIf([systemClass](const WorldSystemEntry& entry)
               {
                   return entry.systemClass == systemClass;
               })
        != m_entries.End();
}

void WorldSystemRegistry::ForEach(const Proc<void(const WorldSystemEntry&)>& proc) const
{
    Mutex::Guard guard(m_mutex);

    for (const WorldSystemEntry& entry : m_entries)
    {
        proc(entry);
    }
}

} // namespace Hyperion
