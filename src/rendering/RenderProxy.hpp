/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_PROXY_HPP
#define HYPERION_RENDER_PROXY_HPP

#include <core/ID.hpp>

#include <core/utilities/UserData.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/containers/Bitset.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/containers/FixedArray.hpp>

#include <core/math/Transform.hpp>
#include <core/math/BoundingBox.hpp>
#include <core/math/Matrix4.hpp>

#include <rendering/RenderableAttributes.hpp>
#include <rendering/backend/RenderObject.hpp>

namespace hyperion {

class Entity;
class Mesh;
class Material;
class Skeleton;
struct MeshInstanceData;

HYP_API extern void MeshInstanceData_PostLoad(MeshInstanceData &);

HYP_STRUCT(PostLoad="MeshInstanceData_PostLoad", Size=104)
struct MeshInstanceData
{
    static constexpr uint32 max_buffers = 8;

    HYP_FIELD(Property="NumInstances", Serialize=true)
    uint32                                  num_instances = 1;

    HYP_FIELD(Property="Buffers", Serialize=true)
    Array<Array<ubyte>, DynamicAllocator>   buffers;

    HYP_FIELD(Property="BufferStructSizes", Serialize=true)
    FixedArray<uint32, max_buffers>         buffer_struct_sizes;

    HYP_FIELD(Property="BufferStructAlignments", Serialize=true)
    FixedArray<uint32, max_buffers>         buffer_struct_alignments;

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

HYP_STRUCT()
struct MeshRaytracingData
{
    HYP_FIELD()
    FixedArray<BLASRef, max_frames_in_flight>   bottom_level_acceleration_structures;
};

/*! \brief A proxy for a renderable object in the world. This is used to store the renderable object and its
 *  associated data, such as the mesh, material, and skeleton. */
struct RenderProxy
{
    WeakHandle<Entity>  entity;
    Handle<Mesh>        mesh;
    Handle<Material>    material;
    Handle<Skeleton>    skeleton;
    Matrix4             model_matrix;
    Matrix4             previous_model_matrix;
    BoundingBox         aabb;
    UserData<32, 16>    user_data;
    MeshInstanceData    instance_data;
    uint32              version = 0;

    void ClaimRenderResource() const;
    void UnclaimRenderResource() const;

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

/*! \brief The action to take on call to \ref{RenderProxyList::Advance}. */
enum class RenderProxyListAdvanceAction : uint32
{
    CLEAR,    //! Clear the 'next' entities, so on next iteration, any entities that have not been re-added are marked for removal.
    PERSIST   //! Copy the previous entities over to next. To remove entities, `RemoveProxy` needs to be manually called.
};

template <class IDType, class ElementType>
class TRenderProxyListBase
{
public:
    using MapType = HashMap<IDType, ElementType>;

    static_assert(std::is_base_of_v<IDBase, IDType>, "IDType must be derived from IDBase (must use numeric ID)");

    TRenderProxyListBase(const TRenderProxyListBase &other)                 = delete;
    TRenderProxyListBase &operator=(const TRenderProxyListBase &other)      = delete;
    TRenderProxyListBase(TRenderProxyListBase &&other) noexcept             = default;
    TRenderProxyListBase &operator=(TRenderProxyListBase &&other) noexcept  = default;
    virtual ~TRenderProxyListBase()                                         = default;
    
    /*! \brief Checks if the RenderProxyList already has a proxy for the given entity
     *  from the previous frame */
    HYP_FORCE_INLINE bool HasElement(IDType id) const
        { return m_previous.Test(id.ToIndex()); }

    HYP_FORCE_INLINE const Bitset &GetCurrentBits() const
        { return m_previous; }

    HYP_FORCE_INLINE MapType &GetElementMap()
        { return m_proxies; }

    HYP_FORCE_INLINE const MapType &GetElementMap() const
        { return m_proxies; }

    HYP_FORCE_INLINE Bitset GetAdded() const
    {
        const SizeType new_num_bits = MathUtil::Max(m_previous.NumBits(), m_next.NumBits());

        return Bitset(m_next).Resize(new_num_bits) & ~Bitset(m_previous).Resize(new_num_bits);
    }

    HYP_FORCE_INLINE Bitset GetRemoved() const
    {
        const SizeType new_num_bits = MathUtil::Max(m_previous.NumBits(), m_next.NumBits());

        return Bitset(m_previous).Resize(new_num_bits) & ~Bitset(m_next).Resize(new_num_bits);
    }

    HYP_FORCE_INLINE const Bitset &GetChanged() const
        { return m_changed; }

    HYP_FORCE_INLINE void Reserve(SizeType capacity)
        { m_proxies.Reserve(capacity); }

    typename MapType::Iterator Add(IDType id, ElementType &&element)
    {
        typename MapType::Iterator iter = m_proxies.End();

        AssertThrowMsg(!m_next.Test(id.ToIndex()), "ID #%u already marked to be added for this iteration!", id.Value());

        if (HasElement(id)) {
            iter = m_proxies.FindAs(id);
        }

        if (iter != m_proxies.End()) {
            // Advance if version has changed
            if (element.version != iter->second.version) {
                // Sanity check - must not contain duplicates or it will mess up safe releasing the previous RenderProxy objects
                AssertDebug(!m_changed.Test(id.ToIndex()));

                // Mark as changed if it is found in the previous iteration
                m_changed.Set(id.ToIndex(), true);

                iter->second = std::move(element);
            }
        } else {
            // sanity check - if not in previous iteration, it must not be in the changed list
            AssertDebug(!m_changed.Test(id.ToIndex()));

            iter = m_proxies.Insert(id, std::move(element)).first;
        }

        m_next.Set(id.ToIndex(), true);

        return iter;
    }

    bool MarkToKeep(IDType id)
    {
        if (!m_previous.Test(id.ToIndex())) {
            return false;
        }

        m_next.Set(id.ToIndex(), true);

        return true;
    }

    void MarkToRemove(IDType id)
    {
        m_next.Set(id.ToIndex(), false);
    }

    void GetElementIDs(Array<IDType> &out_ids)
    {
        HYP_SCOPE;

        Bitset ids = m_previous | m_next;

        out_ids.Reserve(ids.Count());

        SizeType first_set_bit_index;

        while ((first_set_bit_index = ids.FirstSetBitIndex()) != -1) {
            out_ids.PushBack(IDType::FromIndex(first_set_bit_index));

            ids.Set(first_set_bit_index, false);
        }
    }

    void GetRemoved(Array<IDType> &out_entities, bool include_changed)
    {
        HYP_SCOPE;

        Bitset removed_bits = GetRemoved();

        if (include_changed) {
            removed_bits |= GetChanged();
        }

        out_entities.Reserve(removed_bits.Count());

        SizeType first_set_bit_index;

        while ((first_set_bit_index = removed_bits.FirstSetBitIndex()) != -1) {
            out_entities.PushBack(IDType::FromIndex(first_set_bit_index));

            removed_bits.Set(first_set_bit_index, false);
        }
    }

    void GetAdded(Array<ElementType *> &out, bool include_changed)
    {
        HYP_SCOPE;
        
        Bitset newly_added_bits = GetAdded();

        if (include_changed) {
            newly_added_bits |= GetChanged();
        }

        out.Reserve(newly_added_bits.Count());

        Bitset::BitIndex first_set_bit_index;

        while ((first_set_bit_index = newly_added_bits.FirstSetBitIndex()) != Bitset::not_found) {
            const IDType id = IDType::FromIndex(first_set_bit_index);

            auto it = m_proxies.Find(id);
            AssertThrow(it != m_proxies.End());

            out.PushBack(&it->second);

            newly_added_bits.Set(first_set_bit_index, false);
        }
    }

    ElementType *GetElement(IDType id)
    {
        const auto it = m_proxies.Find(id);

        if (it == m_proxies.End()) {
            return nullptr;
        }

        return &it->second;
    }

    void Advance(RenderProxyListAdvanceAction action)
    {
        HYP_SCOPE;

        { // Remove proxies for removed bits
            for (Bitset::BitIndex index : GetRemoved()) {
                const IDType id = IDType::FromIndex(index);

                const auto it = m_proxies.Find(id);

                if (it != m_proxies.End()) {
                    // g_safe_deleter->SafeRelease(std::move(it->second));

                    m_proxies.Erase(it);
                }
            }
        }

        switch (action) {
        case RenderProxyListAdvanceAction::CLEAR:
            // Next state starts out zeroed out -- and next call to Advance will remove proxies for these objs
            m_previous = std::move(m_next);

            break;
        case RenderProxyListAdvanceAction::PERSIST:
            m_previous = m_next;

            break;
        default:
            HYP_UNREACHABLE();

            break;
        }

        m_changed.Clear();
    }

protected:
    // Protected constructor to prevent instantiation without a derived class
    TRenderProxyListBase() = default;

    MapType m_proxies;

    Bitset  m_previous;
    Bitset  m_next;
    Bitset  m_changed;
};

class RenderProxyList final : public TRenderProxyListBase<ID<Entity>, RenderProxy>
{
public:
    RenderProxyList()                                               = default;
    RenderProxyList(const RenderProxyList &other)                   = delete;
    RenderProxyList &operator=(const RenderProxyList &other)        = delete;
    RenderProxyList(RenderProxyList &&other) noexcept               = default;
    RenderProxyList &operator=(RenderProxyList &&other) noexcept    = default;
    virtual ~RenderProxyList() override                             = default;
};

} // namespace hyperion

#endif