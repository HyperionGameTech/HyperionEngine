#ifndef HYPERION_V2_OCTREE_H
#define HYPERION_V2_OCTREE_H

#include <core/containers.h>
#include "scene.h"
#include "spatial.h"
#include "visibility_state.h"

#include <math/vector3.h>
#include <math/bounding_box.h>
#include <math/ray.h>

#include <types.h>

#include <array>

#define HYP_OCTREE_DEBUG 0

namespace hyperion {

class Camera;

} // namespace hyperion

namespace hyperion::v2 {

class Spatial;

class Octree {
    friend class Engine;
    friend class Spatial;

    enum {
        DEPTH_SEARCH_INF = -1,
        DEPTH_SEARCH_ONLY_THIS = 0
    };

    struct Callback {
        using CallbackFunction = std::function<void(Engine *, Octree *, Spatial *)>;
    };

    static constexpr float growth_factor = 1.5f;
    static const BoundingBox default_bounds;

    Octree(Octree *parent, const BoundingBox &aabb, UInt8 index);

public:
    struct Octant {
        std::unique_ptr<Octree> octree;
        BoundingBox             aabb;
    };

    struct Node {
        Spatial     *spatial;
        BoundingBox  aabb;

        VisibilityState *visibility_state = nullptr;
    };

    struct Root {
        struct Events : ComponentEvents<Callback> {
            CallbackGroup on_insert_octant,
                          on_remove_octant,
                          on_insert_node,
                          on_remove_node;
        } events;

        std::unordered_map<Spatial *, Octree *> node_to_octree;
        std::atomic_uint32_t                    visibility_cursor{0};
    };

    static bool IsVisible(const Octree *parent, const Octree *child);

    Octree(const BoundingBox &aabb = default_bounds);
    ~Octree();

    Root *GetRoot() const              { return m_root; }

    BoundingBox &GetAabb()             { return m_aabb; }
    const BoundingBox &GetAabb() const { return m_aabb; }

    auto &GetCallbacks()
    {
        AssertThrow(m_root != nullptr);

        return m_root->events;
    }

    const auto &GetCallbacks() const
        { return const_cast<Octree *>(this)->GetCallbacks(); }

    VisibilityState &GetVisibilityState()             { return m_visibility_state; }
    const VisibilityState &GetVisibilityState() const { return m_visibility_state; }

    void Clear(Engine *engine);
    bool Insert(Engine *engine, Spatial *spatial);
    bool Remove(Engine *engine, Spatial *spatial);
    bool Update(Engine *engine, Spatial *spatial);
    bool Rebuild(Engine *engine, const BoundingBox &new_aabb);

    void CalculateVisibility(Scene *scene);

    /*! \brief Performs a ray test on the octree, only taking into account octants (not nodes).
     * Any RayHits have user_data set to be the pointer to the octant hit. */
    bool TestRay(const Ray &ray, RayTestResults &out_results) const;

private:
    void ClearInternal(Engine *engine, std::vector<Node> &out_nodes);
    void Clear(Engine *engine, std::vector<Node> &out_nodes);
    bool Move(Engine *engine, Spatial *spatial, const std::vector<Node>::iterator *it = nullptr);

    auto FindNode(Spatial *spatial)
    {
        return std::find_if(m_nodes.begin(), m_nodes.end(), [spatial](const Node &node) {
            return node.spatial == spatial;
        });
    }

    bool IsRoot() const { return m_parent == nullptr; }
    bool Empty() const  { return m_nodes.empty(); }
    
    void SetParent(Octree *parent);
    bool EmptyDeep(int depth = DEPTH_SEARCH_INF, UInt8 octant_mask = 0xff) const;

    void InitOctants();
    void Divide(Engine *engine);
    void Undivide(Engine *engine);

    /* Remove any potentially empty octants above the node */
    void CollapseParents(Engine *engine);
    bool InsertInternal(Engine *engine, Spatial *spatial);
    bool UpdateInternal(Engine *engine, Spatial *spatial);
    bool RemoveInternal(Engine *engine, Spatial *spatial);
    bool RebuildExtendInternal(Engine *engine, const BoundingBox &extend_include_aabb);
    void UpdateVisibilityState(Scene *scene);

    /* Called from Spatial - remove the pointer */
    void OnSpatialRemoved(Engine *engine, Spatial *spatial);
    
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
