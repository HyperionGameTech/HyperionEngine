/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/memory/Pimpl.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/AccelerationStructure.hpp>

namespace Hyperion {

class Entity;
class Mesh;
class Material;

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
        Entity* entity, Mesh* mesh, Material* material,
        uint64& outNewKey, uint64& outOldKey,
        GpuBlas*& outBlas);

    void RunCleanupCycle(int maxIter);

private:
    Pimpl<class BLASCacheImpl> m_impl;
};

} // namespace Hyperion
