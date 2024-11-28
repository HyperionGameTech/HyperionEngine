/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_PROXY_HPP
#define HYPERION_RENDER_PROXY_HPP

#include <core/ID.hpp>
#include <core/utilities/UserData.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/containers/Bitset.hpp>
#include <core/containers/FlatMap.hpp>

#include <math/Transform.hpp>
#include <math/BoundingBox.hpp>
#include <math/Matrix4.hpp>

#include <rendering/RenderableAttributes.hpp>

namespace hyperion {

class Entity;
class Mesh;
class Material;
class Skeleton;

HYP_STRUCT(Size=104)
struct MeshInstanceData
{
    static constexpr uint32 max_buffers = 8;

    HYP_FIELD(Property="NumInstances", Serialize=true)
    uint32                          num_instances = 0;

    HYP_FIELD(Property="Buffers", Serialize=true)
    Array<Array<ubyte>, 0>          buffers;

    HYP_FIELD(Property="BufferStructSizes", Serialize=true)
    FixedArray<uint32, max_buffers> buffer_struct_sizes;

    HYP_FIELD(Property="BufferStructAlignments", Serialize=true)
    FixedArray<uint32, max_buffers> buffer_struct_alignments;

    HYP_FORCE_INLINE bool operator==(const MeshInstanceData &other) const
    {
        return num_instances == other.num_instances
            && buffers == other.buffers
            && buffer_struct_sizes == other.buffer_struct_sizes
            && buffer_struct_alignments == other.buffer_struct_alignments;
    }

    HYP_FORCE_INLINE bool operator!=(const MeshInstanceData &other) const
    {
        return num_instances != other.num_instances
            || buffers != other.buffers
            || buffer_struct_sizes != other.buffer_struct_sizes
            || buffer_struct_alignments != other.buffer_struct_alignments;
    }

    HYP_FORCE_INLINE uint32 NumInstances() const
        { return num_instances; }

    template <class StructType>
    void SetBufferData(int buffer_index, StructType *ptr, SizeType count)
    {
        static_assert(IsPODType<StructType>, "Struct type must a POD type");

        AssertThrowMsg(buffer_index < max_buffers, "Buffer index %d must be less than maximum number of buffers (%u)", buffer_index, max_buffers);

        if (buffers.Size() <= buffer_index) {
            buffers.Resize(buffer_index + 1);
        }

        buffer_struct_sizes[buffer_index] = sizeof(StructType);
        buffer_struct_alignments[buffer_index] = alignof(StructType);

        buffers[buffer_index].Resize(sizeof(StructType) * count);
        Memory::MemCpy(buffers[buffer_index].Data(), ptr, sizeof(StructType) * count);
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(num_instances);
        hc.Add(buffers);
        hc.Add(buffer_struct_sizes);
        hc.Add(buffer_struct_alignments);

        return hc;
    }
};

struct RenderProxy
{
    Handle<Entity>      entity;
    Handle<Mesh>        mesh;
    Handle<Material>    material;
    Handle<Skeleton>    skeleton;
    Matrix4             model_matrix;
    Matrix4             previous_model_matrix;
    BoundingBox         aabb;
    UserData<32, 16>    user_data;
    MeshInstanceData    instance_data;
    uint32              version = 0;

    HYP_FORCE_INLINE uint32 NumInstances() const
        { return MathUtil::Max(instance_data.NumInstances(), 1); }

    void ClaimRenderResources() const;
    void UnclaimRenderResources() const;

    bool operator==(const RenderProxy &other) const
    {
        return entity == other.entity
            && mesh == other.mesh
            && material == other.material
            && skeleton == other.skeleton
            && model_matrix == other.model_matrix
            && previous_model_matrix == other.previous_model_matrix
            && aabb == other.aabb
            && user_data == other.user_data
            && instance_data == other.instance_data
            && version == other.version;
    }

    bool operator!=(const RenderProxy &other) const
    {
        return entity != other.entity
            || mesh != other.mesh
            || material != other.material
            || skeleton != other.skeleton
            || model_matrix != other.model_matrix
            || previous_model_matrix != other.previous_model_matrix
            || aabb != other.aabb
            || user_data != other.user_data
            || instance_data != other.instance_data
            || version != other.version;
    }
};

using RenderProxyEntityMap = HashMap<ID<Entity>, RenderProxy>;

/*! \brief The action to take on call to \ref{RenderProxyList::Advance}. */
enum class RenderProxyListAdvanceAction : uint32
{
    CLEAR,    //! Clear the 'next' entities, so on next iteration, any entities that have not been re-added are marked for removal.
    PERSIST   //! Copy the previous entities over to next. To remove entities, `RemoveProxy` needs to be manually called.
};

class RenderProxyList
{
public:
    void Add(ID<Entity> entity, const RenderProxy &proxy);

    /*! \brief Mark to keep a proxy from the previous iteration for this iteration.
     *  Usable when \ref{Advance} is called with \ref{RenderProxyListAdvanceAction::CLEAR}. Returns true if the
     *  proxy was successfully taken from the previous iteration, false otherwise.
     *  \param entity The entity to keep the proxy from the previous iteration for
     *  \returns Whether or not the proxy could be taken from the previous iteration */
    bool MarkToKeep(ID<Entity> entity);

    /*! \brief Mark the proxy to be removed in the next call to \ref{Advance} */
    void MarkToRemove(ID<Entity> entity);

    void GetRemovedEntities(Array<ID<Entity>> &out_entities);
    void GetAddedEntities(Array<RenderProxy *> &out_entities, bool include_changed = false);

    RenderProxy *GetProxyForEntity(ID<Entity> entity);

    const RenderProxy *GetProxyForEntity(ID<Entity> entity) const
        { return const_cast<RenderProxyList *>(this)->GetProxyForEntity(entity); }

    void Advance(RenderProxyListAdvanceAction action);

    /*! \brief Checks if the RenderProxyList already has a proxy for the given entity
     *  from the previous frame */
    HYP_FORCE_INLINE bool HasProxyForEntity(ID<Entity> entity) const
        { return m_previous_entities.Test(entity.ToIndex()); }

    HYP_FORCE_INLINE const Bitset &GetEntities() const
        { return m_previous_entities; }

    HYP_FORCE_INLINE const Bitset &GetNextEntities() const
        { return m_next_entities; }

    HYP_FORCE_INLINE RenderProxyEntityMap &GetRenderProxies()
        { return m_proxies; }

    HYP_FORCE_INLINE const RenderProxyEntityMap &GetRenderProxies() const
        { return m_proxies; }

    HYP_FORCE_INLINE RenderProxyEntityMap &GetChangedRenderProxies()
        { return m_changed_proxies; }

    HYP_FORCE_INLINE const RenderProxyEntityMap &GetChangedRenderProxies() const
        { return m_changed_proxies; }

    HYP_FORCE_INLINE Bitset GetAddedEntities() const
    {
        const SizeType new_num_bits = MathUtil::Max(m_previous_entities.NumBits(), m_next_entities.NumBits());

        return Bitset(m_next_entities).Resize(new_num_bits) & ~Bitset(m_previous_entities).Resize(new_num_bits);
    }

    HYP_FORCE_INLINE Bitset GetRemovedEntities() const
    {
        const SizeType new_num_bits = MathUtil::Max(m_previous_entities.NumBits(), m_next_entities.NumBits());

        return Bitset(m_previous_entities).Resize(new_num_bits) & ~Bitset(m_next_entities).Resize(new_num_bits);
    }

    HYP_FORCE_INLINE const Bitset &GetChangedEntities() const
        { return m_changed_entities; }

private:
    RenderProxyEntityMap    m_proxies;
    RenderProxyEntityMap    m_changed_proxies;

    Bitset                  m_previous_entities;
    Bitset                  m_next_entities;
    Bitset                  m_changed_entities;
};

} // namespace hyperion

#endif