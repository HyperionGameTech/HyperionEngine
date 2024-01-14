#ifndef HYPERION_V2_OCTREE_H
#define HYPERION_V2_OCTREE_H

#include <core/Containers.hpp>
#include <core/lib/Pair.hpp>
#include <core/lib/HashMap.hpp>
#include <scene/Entity.hpp>
#include <scene/VisibilityState.hpp>
#include <util/ByteUtil.hpp>

#include <math/Vector3.hpp>
#include <math/BoundingBox.hpp>
#include <math/Ray.hpp>

#include <Types.hpp>

#define HYP_OCTREE_DEBUG 1

namespace hyperion::v2 {

class Entity;
class EntityManager;
class Camera;

/*! \brief A 64-bit integer that represents an octant in an octree
 *  \details The bits are ordered as follows:
 *  - 0-2: index of topmost parent octant
 *  - 3-5: index of second parent octant
 *  - 6-8: index of third parent octant
 * ... and so on until the index of the octant itself.
 * 
 * The maximum depth of an octree using this ID scheme is 64 bits / 3 bits for index = 21 octants.
*/
struct OctantID
{
    static constexpr UInt64 invalid_bits = 1ull << 63;
    static constexpr SizeType max_depth = 64 / 3;

    static const OctantID invalid;  // special value for invalid octant

    UInt64  index_bits { 0 };
    UInt8   depth { 0 };

    OctantID() = default;

    explicit OctantID(UInt64 index_bits, UInt8 depth)
        : index_bits(index_bits), depth(depth) {}

    explicit OctantID(UInt8 child_index, OctantID parent_id)
        : index_bits(!parent_id.IsInvalid()
            ? parent_id.index_bits | (UInt64(child_index) << (UInt64(parent_id.GetDepth() + UInt8(1)) * 3ull))
            : child_index),
          depth(parent_id.GetDepth() + UInt8(1)) {}

    OctantID(const OctantID &other) = default;
    OctantID &operator=(const OctantID &other) = default;
    OctantID(OctantID &&other) noexcept = default;
    OctantID &operator=(OctantID &&other) noexcept = default;
    ~OctantID() = default;

    HYP_FORCE_INLINE
    Bool IsInvalid() const // This bit is reserved for invalid octants -- We use 3 bits for each index, leaving 1 bit left on a 64-bit integer
        { return index_bits & invalid_bits; }

    HYP_FORCE_INLINE
    Bool operator==(const OctantID &other) const
        { return index_bits == other.index_bits && depth == other.depth; }

    HYP_FORCE_INLINE
    Bool operator!=(const OctantID &other) const
        { return !(*this == other); }

    HYP_FORCE_INLINE
    UInt8 GetIndex(UInt8 depth) const
        { return (index_bits >> (UInt64(depth) * 3ull)) & 0x7; }

    HYP_FORCE_INLINE
    UInt8 GetIndex() const
        { return GetIndex(depth); }

    HYP_FORCE_INLINE
    UInt8 GetDepth() const
        { return depth; }

    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(index_bits);
        hc.Add(depth);

        return hc;
    }
};

class Octree
{
    friend class Entity;

    enum
    {
        DEPTH_SEARCH_INF = -1,
        DEPTH_SEARCH_ONLY_THIS = 0
    };

    static constexpr Float growth_factor = 1.5f;
    // the length value at which to stop recusively dividing
    // for a small enough object
    static constexpr Float min_aabb_size = 1.0f; 
    static const BoundingBox default_bounds;

    Octree(RC<EntityManager> entity_manager, const BoundingBox &aabb, Octree *parent, UInt8 index);

public:
    struct Result
    {
        enum
        {
            OCTREE_OK  = 0,
            OCTREE_ERR = 1
        } result;

        const char  *message;
        Int         error_code = 0;

        Result()
            : Result(OCTREE_OK) {}
        
        Result(decltype(result) result, const char *message = "", Int error_code = 0)
            : result(result), message(message), error_code(error_code) {}
        Result(const Result &other)
            : result(other.result), message(other.message), error_code(other.error_code) {}

        HYP_FORCE_INLINE
        operator Bool() const { return result == OCTREE_OK; }
    };

    using InsertResult = Pair<Result, OctantID>;

    struct Octant
    {
        UniquePtr<Octree>   octree;
        BoundingBox         aabb;
    };

    struct Node
    {
        ID<Entity>  id;
        BoundingBox aabb;

        HashCode GetHashCode() const
        {
            HashCode hc;

            hc.Add(id.GetHashCode());
            hc.Add(aabb.GetHashCode());

            // use .GetID() instead of ->GetID(), because they may not be set
            // hc.Add(entity->GetMesh().GetID().GetHashCode());
            // hc.Add(entity->GetMaterial().GetID().GetHashCode());

            return hc;
        }
    };

    struct Root
    {
        HashMap<ID<Entity>, Octree *>   node_to_octree;
        std::atomic_uint8_t             visibility_cursor { 0u };
    };

    static Bool IsVisible(
        const Octree *parent,
        const Octree *child
    );

    static Bool IsVisible(
        const Octree *parent,
        const Octree *child,
        UInt8 cursor
    );

    Octree(RC<EntityManager> entity_manager, const BoundingBox &aabb = default_bounds);
    Octree(const Octree &other) = delete;
    Octree &operator=(const Octree &other) = delete;
    Octree(Octree &&other) noexcept = delete;
    Octree &operator=(Octree &&other) noexcept = delete;
    ~Octree();

    const VisibilityState &GetVisibilityState() const
        { return m_visibility_state; }

    BoundingBox &GetAABB()
        { return m_aabb; }

    const BoundingBox &GetAABB() const
        { return m_aabb; }

    const Array<Node> &GetNodes() const
        { return m_nodes; }

    OctantID GetOctantID() const
        { return m_octant_id; }

    const FixedArray<Octant, 8> &GetOctants() const
        { return m_octants; }

    /*! \brief Get the child (nested) octant with the specified index
     *  \param octant_id The OctantID to use to find the octant (see OctantID struct)
     *  \return The octant with the specified index, or nullptr if it doesn't exist
    */
    Octree *GetChildOctant(OctantID octant_id);

    Bool IsDivided() const
        { return m_is_divided; }

    /*! \brief Get a hashcode of all entities currently in this Octant (child octants affect this too) */
    HashCode GetNodesHash() const
    {
        HashCode hc;

        hc.Add(m_nodes_hash);

        if (m_is_divided) {
            for (const Octant &octant : m_octants) {
                AssertThrow(octant.octree != nullptr);

                hc.Add(octant.octree->GetNodesHash());
            }
        }

        return hc;
    }
        
    void Clear();
    InsertResult Insert(ID<Entity> id, const BoundingBox &aabb);
    Result Remove(ID<Entity> id);
    InsertResult Update(ID<Entity> id, const BoundingBox &aabb);
    InsertResult Rebuild(const BoundingBox &new_aabb);

    void CollectEntities(Array<ID<Entity>> &out) const;
    void CollectEntitiesInRange(const Vector3 &position, Float radius, Array<ID<Entity>> &out) const;
    Bool GetNearestOctants(const Vector3 &position, FixedArray<Octree *, 8> &out) const;
    Bool GetNearestOctant(const Vector3 &position, Octree const *&out) const;
    Bool GetFittingOctant(const BoundingBox &aabb, Octree const *&out) const;

    void NextVisibilityState();
    void CalculateVisibility(Camera *camera);
    UInt8 LoadVisibilityCursor() const;
    UInt8 LoadPreviousVisibilityCursor() const;

    Bool TestRay(const Ray &ray, RayTestResults &out_results) const;

private:
    void ResetNodesHash();
    void RebuildNodesHash(UInt level = 0);

    void ClearInternal(Array<Node> &out_nodes);
    void Clear(Array<Node> &out_nodes);
    InsertResult Move(ID<Entity> id, const BoundingBox &aabb, const Array<Node>::Iterator *it = nullptr);

    void ForceVisibilityStates();
    void CopyVisibilityState(const VisibilityState &visibility_state);

    auto FindNode(ID<Entity> id)
    {
        return m_nodes.FindIf([id](const Node &item)
        {
            return item.id == id;
        });
    }

    Bool IsRoot() const { return m_parent == nullptr; }
    Bool Empty() const { return m_nodes.Empty(); }
    Root *GetRoot() const { return m_root; }
    
    void SetParent(Octree *parent);
    Bool EmptyDeep(Int depth = DEPTH_SEARCH_INF, UInt8 octant_mask = 0xff) const;

    void InitOctants();
    void Divide();
    void Undivide();

    /* Remove any potentially empty octants above the node */
    void CollapseParents();
    InsertResult InsertInternal(ID<Entity> id, const BoundingBox &aabb);
    InsertResult UpdateInternal(ID<Entity> id, const BoundingBox &aabb);
    Result RemoveInternal(ID<Entity> id);
    InsertResult RebuildExtendInternal(const BoundingBox &extend_include_aabb);
    void UpdateVisibilityState(Camera *camera, UInt8 cursor);

    /* Called from entity - remove the pointer */
    // void OnEntityRemoved(Entity *entity);

    RC<EntityManager>       m_entity_manager;
    
    Array<Node>             m_nodes;
    HashCode                m_nodes_hash;
    Octree                  *m_parent;
    BoundingBox             m_aabb;
    FixedArray<Octant, 8>   m_octants;
    Bool                    m_is_divided;
    Root                    *m_root;
    VisibilityState         m_visibility_state;
    OctantID                m_octant_id;
};

} // namespace hyperion::v2

#endif
