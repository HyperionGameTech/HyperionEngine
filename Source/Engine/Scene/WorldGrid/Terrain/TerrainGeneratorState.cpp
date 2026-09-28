/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/TerrainGeneratorState.hpp>

#include <Core/Threading/Threads.hpp>

namespace Hyperion {

TerrainGeneratorState::TerrainGeneratorState()
    : m_generator(MakeShared<TerrainGenerator>())
{
}

void TerrainGeneratorState::BumpEpoch_Locked()
{
    m_epoch.Increment(1, MemoryOrder::ACQUIRE_RELEASE);
}

SharedPtr<TerrainGenerator> TerrainGeneratorState::Replace(SharedPtr<TerrainGenerator>&& generator, uint64 cellFingerprint)
{
    AssertOnThread(g_simThread);

    Mutex::Guard guard(m_mutex);

    SharedPtr<TerrainGenerator> previousGenerator = std::move(m_generator);

    m_generator = std::move(generator);
    m_cellFingerprint = cellFingerprint;

    BumpEpoch_Locked();

    return previousGenerator;
}

TerrainGenerationState TerrainGeneratorState::Snapshot(const WorldGridLayerInfo& layerInfo) const
{
    Mutex::Guard guard(m_mutex);

    return TerrainGenerationState {
        .generator = m_generator,
        .cellFingerprint = m_cellFingerprint,
        .epoch = GetEpoch(),
        .layerInfo = layerInfo
    };
}

} // namespace Hyperion
