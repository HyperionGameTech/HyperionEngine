/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Asset/AssetObject.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/FlatMap.hpp>
#include <Core/Utilities/Span.hpp>

#include <Core/Memory/SharedPtr.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Resource/ResLock.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Core/Math/Vector2.hpp>

namespace Hyperion {

class TerrainCellData;
class TerrainGeneratorState;

class TerrainHeightsCache
{
public:
    /// sim thread only - the saved heights of the cell sampled last, kept paged in between samples
    struct SampleCache
    {
        Handle<TerrainCellData> cell;
        TSharedResLock<AssetObject> scope;
        Span<const float> heights;

        void Invalidate();
    };

    SharedPtr<const Array<float>> Find(const Vec2i& coord) const;

    /// returns what another thread cached first if it beat us to it. Nothing is cached once \p generationEpoch is stale
    SharedPtr<const Array<float>> Insert(
        const Vec2i& coord,
        const SharedPtr<Array<float>>& heights,
        const TerrainGeneratorState& generatorState,
        uint32 generationEpoch);

    /// false if \p coord is already being warmed
    bool TryBeginWarm(const Vec2i& coord);
    void EndWarm(const Vec2i& coord);

    /// call after bumping the generation epoch, so work from the previous epoch can't repopulate it
    void Clear();

    HYP_FORCE_INLINE SampleCache& GetSampleCache() const
    {
        return m_sampleCache;
    }

private:
    mutable Mutex m_heightsMutex;
    FlatMap<Vec2i, SharedPtr<Array<float>>> m_heights;

    mutable Mutex m_warmsMutex;
    FlatMap<Vec2i, bool> m_pendingWarms;

    mutable SampleCache m_sampleCache;
};

} // namespace Hyperion
