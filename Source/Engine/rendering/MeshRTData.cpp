/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/MeshRTData.hpp>
#include <rendering/MeshBlasBuilder.hpp>
#include <rendering/util/DeletionQueue.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>

#include <Core/threading/Mutex.hpp>
#include <Core/containers/HashMap.hpp>

#include <scene/Entity.hpp>

namespace Hyperion {

class MeshRTDataImpl
{
public:
    MeshRTDataImpl() = default;
    
    ~MeshRTDataImpl()
    {
        for (auto& pair : blasMap)
        {
            EnqueueDeletion(std::move(pair.second));
        }
        
        blasMap.Clear();
    }

    mutable Mutex mutex;
    HashMap<ObjId<Entity>, GpuBlasRef> blasMap;
};

MeshRTData::MeshRTData()
    : m_impl(MakePimpl<MeshRTDataImpl>())
{
}

MeshRTData::~MeshRTData() = default;

const BLASRef& MeshRTData::GetOrCreateBLAS(Entity* entity, Mesh* mesh, Material* material)
{
    static BLASRef nullRef;
    
    if (!entity || !mesh)
    {
        return nullRef;
    }

    const ObjId<Entity> entityId(entity->Id());
    
    Mutex::Guard guard(m_impl->mutex);
    
    auto it = m_impl->blasMap.Find(entityId);
    
    if (it != m_impl->blasMap.End())
    {
        BLASRef& existingBlas = it->second;
        
        // Check if material changed - if so, we need to rebuild
        const bool materialsDiffer = existingBlas != nullptr
            && existingBlas->GetMaterial() != material;
        
        if (!materialsDiffer && existingBlas != nullptr)
        {
            return existingBlas;
        }
        
        // Material changed or BLAS is null, need to rebuild
        if (existingBlas != nullptr)
        {
            EnqueueDeletion(std::move(existingBlas));
        }
    }
    
    // Build new BLAS
    BLASRef blas = MeshBlasBuilder::Build(mesh, material);
    
    auto insertResult = m_impl->blasMap.Set(entityId, std::move(blas));
    return insertResult.first->second;
}

void MeshRTData::InvalidateBLAS(Entity* entity)
{
    if (!entity)
    {
        return;
    }

    const ObjId<Entity> entityId(entity->Id());
    
    Mutex::Guard guard(m_impl->mutex);
    
    auto it = m_impl->blasMap.Find(entityId);
    
    if (it != m_impl->blasMap.End())
    {
        EnqueueDeletion(std::move(it->second));
        m_impl->blasMap.Erase(entityId);
    }
}

} // namespace Hyperion
