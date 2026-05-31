/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Memory/Pimpl.hpp>

#include <Rendering/RenderTypes.hpp>
#include <Rendering/AccelerationStructure.hpp>

namespace Hyperion {

class Entity;
class Mesh;
class MaterialInstance;

using BLASRef = GpuBlasRef;

class BLASCache
{
public:
    BLASCache();

    BLASCache(const BLASCache& other) = delete;
    BLASCache& operator=(const BLASCache& other) = delete;

    BLASCache(BLASCache&& other) noexcept = delete;
    BLASCache& operator=(BLASCache&& other) noexcept = delete;

    ~BLASCache();

    void GetOrCreateBLAS(
        Entity* entity, Mesh* mesh, MaterialInstance* material,
        uint64& outNewKey, uint64& outOldKey,
        GpuBlas*& outBlas);

    void RunCleanupCycle(int maxIter);

private:
    Pimpl<class BLASCacheImpl> m_impl;
};

} // namespace Hyperion
