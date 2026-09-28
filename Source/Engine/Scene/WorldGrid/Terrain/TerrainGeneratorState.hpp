/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/WorldGrid/WorldGridLayer.hpp>
#include <Scene/WorldGrid/Terrain/Generation/TerrainGenerator.hpp>

#include <Core/Memory/SharedPtr.hpp>

#include <Core/Threading/Mutex.hpp>
#include <Core/Threading/AtomicVar.hpp>

namespace Hyperion {

struct TerrainGenerationState
{
    SharedPtr<TerrainGenerator> generator;
    uint64 cellFingerprint = 0;
    uint32 epoch = 0;

    ///taken together with the epoch - cell geometry (size, scale, offset) changes bump the epoch
    WorldGridLayerInfo layerInfo;
};

/// written on the sim thread under GetMutex(), which also guards the layer info that snapshots copy
class TerrainGeneratorState
{
public:
    TerrainGeneratorState();

    HYP_FORCE_INLINE Mutex& GetMutex() const
    {
        return m_mutex;
    }

    /// sim thread only - other threads use Snapshot()
    HYP_FORCE_INLINE const SharedPtr<TerrainGenerator>& GetGenerator() const
    {
        return m_generator;
    }

    /// sim thread only
    HYP_FORCE_INLINE uint64 GetCellFingerprint() const
    {
        return m_cellFingerprint;
    }

    /// call with GetMutex() held
    HYP_FORCE_INLINE void SetCellFingerprint_Locked(uint64 cellFingerprint)
    {
        m_cellFingerprint = cellFingerprint;
    }

    HYP_FORCE_INLINE uint32 GetEpoch() const
    {
        return m_epoch.Get(MemoryOrder::ACQUIRE);
    }

    ///false once the generator \p epoch belongs to has been replaced or invalidated
    HYP_FORCE_INLINE bool IsCurrent(uint32 epoch) const
    {
        return GetEpoch() == epoch;
    }

    /// call with GetMutex() held - work started before this is discarded
    void BumpEpoch_Locked();

    /// sim thread only - returns the replaced generator
    SharedPtr<TerrainGenerator> Replace(SharedPtr<TerrainGenerator>&& generator, uint64 cellFingerprint);

    TerrainGenerationState Snapshot(const WorldGridLayerInfo& layerInfo) const;

private:
    mutable Mutex m_mutex;
    SharedPtr<TerrainGenerator> m_generator;
    uint64 m_cellFingerprint = 0;
    AtomicVar<uint32> m_epoch { 0 };
};

} // namespace Hyperion
