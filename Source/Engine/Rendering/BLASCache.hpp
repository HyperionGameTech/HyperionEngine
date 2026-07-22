/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Memory/Pimpl.hpp>
#include <Core/Reflection/ObjId.hpp>

#include <Rendering/RenderTypes.hpp>
#include <Rendering/AccelerationStructure.hpp>

namespace Hyperion {

class Entity;
class Mesh;
class Material;

class BLASCache
{
public:
    static constexpr uint32 InvalidStorageId = UINT32_MAX;

    BLASCache();

    BLASCache(const BLASCache& other) = delete;
    BLASCache& operator=(const BLASCache& other) = delete;

    BLASCache(BLASCache&& other) noexcept = delete;
    BLASCache& operator=(BLASCache&& other) noexcept = delete;

    ~BLASCache();

    void GetOrCreateBLAS(
        ObjId<Entity> entityId, Mesh* mesh, Material* material,
        uint64& outNewKey, uint64& outOldKey,
        BottomLevelAS*& outBlas);

    static uint64 MakeKey(ObjId<Entity> entityId, ObjId<Mesh> meshId, ObjId<Material> materialId);

    bool RemoveBLAS(ObjId<Entity> entityId, uint64 expectedKey);

    uint32 TranslateBLASKeyToStorageId(uint64 key) const;

    HYP_NODISCARD uint32 AllocateStorageId(uint64 key);
    bool ReleaseStorageIdForBLASKey(uint64 key, uint32& outStorageId, uint32& outNewRefCount);

    void OnFrameEnd(uint32 prevFrameIndex);

private:
    Pimpl<class BLASCacheImpl> m_impl;
};

} // namespace Hyperion
