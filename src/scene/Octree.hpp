#ifndef HYPERION_V2_OCTREE_H
#define HYPERION_V2_OCTREE_H

#include <core/Containers.hpp>
#include "Entity.hpp"
#include "VisibilityState.hpp"

#include <math/Vector3.hpp>
#include <math/BoundingBox.hpp>
#include <math/Ray.hpp>

#include <Types.hpp>

#include <array>

#define HYP_OCTREE_DEBUG 0

namespace hyperion::v2 {

class Entity;
class Camera;

class Octree
{
    friend class Entity;

    enum
    {
        DEPTH_SEARCH_INF = -1,
        DEPTH_SEARCH_ONLY_THIS = 0
    };

    struct Callback
    {
        using CallbackFunction = std::function<void(Octree *, Entity *)>;
    };

    static constexpr float growth_factor = 1.5f;
    // the length value at which to stop recusively dividing
    // for a small enough object
    static constexpr float min_aabb_size = 1.0f; 
    static const BoundingBox default_bounds;

    Octree(Octree *parent, const BoundingBox &aabb, UInt8 index);

public:
    struct Result
    {
        enum
        {
            OCTREE_OK  = 0,
            OCTREE_ERR = 1
        } result;

        const char *message;
        int error_code = 0;

        Result()
            : Result(OCTREE_OK) {}
        
        Result(decltype(result) result, const char *message = "", int error_code = 0)
            : result(result), message(message), error_code(error_code) {}
        Result(const Result &other)
            : result(other.result), message(other.message), error_code(other.error_code) {}

        HYP_FORCE_INLINE
        operator bool() const { return result == OCTREE_OK; }
    };

    struct Octant
    {
        std::unique_ptr<Octree> octree;
        BoundingBox aabb;
    };

    struct Node
    {
        Entity *entity;
        BoundingBox aabb;
    };

    struct Root
    {
        struct Events : ComponentEvents<Callback>
        {
            CallbackGroup on_insert_octant,
                on_remove_octant,
                on_insert_node,
                on_remove_node;
        } events;

        std::unordered_map<Entity *, Octree *> node_to_octree;
        std::atomic_uint8_t visibility_cursor { 0u };
    };

    static bool IsVisible(
        const Octree *parent,
        const Octree *child
    );

    static bool IsVisible(
        const Octree *parent,
        const Octree *child,
        UInt8 cursor
    );

    Octree(const BoundingBox &aabb = default_bounds);
    Octree(const Octree &other) = delete;
    Octree &operator=(const Octree &other) = delete;
    Octree(Octree &&other) noexcept = delete;
    Octree &operator=(Octree &&other) noexcept = delete;
    ~Octree();

    const VisibilityState &GetVisibilityState() const { return m_visibility_state; }

    BoundingBox &GetAABB() { return m_aabb; }
    const BoundingBox &GetAABB() const { return m_aabb; }

    void Clear();
    Result Insert(Entity *entity);
    Result Remove(Entity *entity);
    Result Update(Entity *entity);
    Result Rebuild(const BoundingBox &new_aabb);

    void CollectEntities(std::vector<Entity *> &out) const;
    void CollectEntitiesInRange(const Vector3 &position, float radius, std::vector<Entity *> &out) const;
    bool GetNearestOctants(const Vector3 &position, std::array<Octree *, 8> &out) const;

    void NextVisibilityState();
    void CalculateVisibility(Scene *scene);
    UInt8 LoadVisibilityCursor() const;
    UInt8 LoadPreviousVisibilityCursor() const;

    bool TestRay(const Ray &ray, RayTestResults &out_results) const;

private:
    void ClearInternal(std::vector<Node> &out_nodes);
    void Clear(std::vector<Node> &out_nodes);
    Result Move(Entity *entity, const std::vector<Node>::iterator *it = nullptr);

    void CopyVisibilityState(const VisibilityState &visibility_state);

    auto FindNode(Entity *entity)
    {
        return std::find_if(
            m_nodes.begin(),
            m_nodes.end(),
            [entity](const Node &node) {
                return node.entity == entity;
            }
        );
    }

    bool IsRoot() const { return m_parent == nullptr; }
    bool Empty() const { return m_nodes.empty(); }
    Root *GetRoot() const { return m_root; }
    
    void SetParent(Octree *parent);
    bool EmptyDeep(int depth = DEPTH_SEARCH_INF, UInt8 octant_mask = 0xff) const;

    void InitOctants();
    void Divide();
    void Undivide();

    /* Remove any potentially empty octants above the node */
    void CollapseParents();
    Result InsertInternal(Entity *entity);
    Result UpdateInternal(Entity *entity);
    Result RemoveInternal(Entity *entity);
    Result RebuildExtendInternal(const BoundingBox &extend_include_aabb);
    void UpdateVisibilityState(Scene *scene, UInt8 cursor);

    /* Called from entity - remove the pointer */
    void OnEntityRemoved(Entity *entity);
    
    std::vector<Node>      m_nodes;
    Octree                *m_parent;
    BoundingBox            m_aabb;
    std::array<Octant, 8>  m_octants;
    bool                   m_is_divided;
    Root                  *m_root;
    VisibilityState        m_visibility_state;
    UInt8                  m_index;
};

} // namespace hyperion::v2

#endif
