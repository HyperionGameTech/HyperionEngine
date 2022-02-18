#ifndef RENDERER_H
#define RENDERER_H

#include <vector>
#include <map>
#include <cstdlib>

#include "../util.h"
#include "../core_engine.h"
#include "../entity.h"
#include "../hash_code.h"
#include "math/bounding_box.h"
#include "render_window.h"
#include "material.h"
#include "camera/camera.h"
#include "framebuffer_2d.h"
#include "postprocess/post_processing.h"

#define RENDERER_SHADER_GROUPING 1
#define RENDERER_FRUSTUM_CULLING 0 // something is off with culling + batching

namespace hyperion {

struct MemoizedFrustumCheckKey {
    size_t frustum_hash_code;
    size_t aabb_hash_code;
};


struct BucketItem {
    Renderable *renderable;
    Material *material;
    BoundingBox aabb;
    Transform transform;
    size_t hash_code;
    bool frustum_culled;
    bool alive;

    BucketItem()
        : renderable(nullptr),
          material(nullptr),
          aabb(),
          transform(),
          hash_code(0),
          frustum_culled(false),
          alive(true)
    {
    }

    BucketItem(const BucketItem &other)
        : renderable(other.renderable),
          material(other.material),
          aabb(other.aabb),
          transform(other.transform),
          hash_code(other.hash_code),
          frustum_culled(other.frustum_culled),
          alive(other.alive)
    {
    }
};

struct Bucket {
    bool enable_culling;

    Bucket()
        : enable_culling(true)
    {
    }

    Bucket(const Bucket &other)
        : enable_culling(other.enable_culling),
          items(other.items),
          hash_to_item_index(other.hash_to_item_index)
    {
    }

    inline bool IsEmpty() const { return items.empty(); }
    inline const std::vector<BucketItem> &GetItems() const { return items; }

    std::size_t GetIndex(size_t at)
    {
        const auto it = hash_to_item_index.find(at);
        AssertThrow(it != hash_to_item_index.end());

        return it->second;
    }

    BucketItem *GetItemPtr(size_t at)
    {
        const auto it = hash_to_item_index.find(at);

        if (it == hash_to_item_index.end())
        {
            return nullptr;
        }

        const size_t index = it->second;
        AssertThrow(index < items.size());

        return &items[index];
    }

    BucketItem &GetItem(size_t at)
    {
        const auto it = hash_to_item_index.find(at);
        AssertThrow(it != hash_to_item_index.end());

        const size_t index = it->second;
        AssertThrow(index < items.size());

        return items[index];
    }

    void AddItem(const BucketItem &bucket_item)
    {
        const auto it = hash_to_item_index.find(bucket_item.hash_code);
        AssertThrow(it == hash_to_item_index.end());

        AssertSoft(bucket_item.renderable != nullptr);

        // For optimization: try to find an existing slot that
        // where the same shader is used previously or next
        // this will save us from having to switch shaders
        // as much.
        // if no slots are found where the shaders match,
        // use an existing (non alive) slot anyway,
        // that way we don't have to increase the array size.
        // it comes free anyway

        size_t slot_index = 0;
        bool slot_found = false;

#if RENDERER_SHADER_GROUPING
        for (size_t i = 0; i < items.size(); i++) {
            if (!items[i].alive) {
                slot_index = i;
                slot_found = true;

                AssertBreakMsg(bucket_item.renderable->GetShader() != nullptr, "No shader set, using first found inactive slot");

                continue;
            }

            // it is alive -- check if "next" slot is empty
            bool is_next_empty = (i < items.size() - 1) && !items[i + 1].alive;
            bool is_prev_empty = (i > 0) && !items[i - 1].alive;

            if (!is_next_empty && !is_prev_empty) {
                continue;
            }

            // next one is empty, so we compare shaders for this,
            // checking if we should insert the item into the next slot

            AssertExit(items[i].renderable != nullptr);

            if (items[i].renderable->GetShader() != nullptr) {
                // doing ID check for now... not sure how portable it will be
                if (items[i].renderable->GetShader()->GetId() == bucket_item.renderable->GetShader()->GetId()) {
                    slot_index = is_next_empty ? (i + 1) : (i - 1);
                    slot_found = true;

                    std::cout << "CHA CHING, saved 1 switch of shaders, woo\n";

                    break;
                }
            }
        }

        if (slot_found) {
            items[slot_index] = bucket_item;
        } else {
#endif
            slot_index = items.size();
            items.push_back(bucket_item);
#if RENDERER_SHADER_GROUPING
        }
#endif

        hash_to_item_index[bucket_item.hash_code] = slot_index;
    }

    void SetItem(size_t at, const BucketItem &bucket_item)
    {
        const auto it = hash_to_item_index.find(at);
        AssertThrow(it != hash_to_item_index.end());

        const size_t index = it->second;
        AssertThrow(index < items.size());

        items[index] = bucket_item;

        if (bucket_item.hash_code != at) {
            hash_to_item_index.erase(it);
            hash_to_item_index[bucket_item.hash_code] = index;
        }
    }

    void RemoveItem(size_t at)
    {
        const auto it = hash_to_item_index.find(at);
        AssertThrow(it != hash_to_item_index.end());

        const size_t index = it->second;
        AssertThrow(index < items.size());

        hash_to_item_index.erase(it);
        //items.erase(items.begin() + index);

        // refrain from erasing in the middle of the vector,
        // which would lose indexes

        items[index].alive = false;

        if (index == items.size() - 1) {
            items.pop_back();
        }
    }

    void ClearAll()
    {
        items.clear();
    }

private:

    std::vector<BucketItem> items;
    std::map<std::size_t, std::size_t> hash_to_item_index;
};

using Bucket_t = std::vector<BucketItem>;

class Shader;

class Renderer {
public:
    Renderer(const RenderWindow &);
    Renderer(const Renderer &other) = delete;
    Renderer &operator=(const Renderer &other) = delete;
    ~Renderer();

    void Collect(Camera *cam, Entity *top);
    void Begin(Camera *cam);
    void Render(Camera *cam);
    void End(Camera * cam);

    void RenderBucket(Camera *cam, Bucket &bucket, Shader *override_shader = nullptr, bool enable_frustum_culling = true);
    void RenderAll(Camera *cam, Framebuffer2D *fbo = nullptr);
    void RenderPost(Camera *cam, Framebuffer2D *fbo);

    inline bool IsDeferred() const { return m_is_deferred; }
    void SetDeferred(bool deferred);

    inline const Framebuffer2D *GetFramebuffer() const { return m_fbo; }
 
    inline PostProcessing *GetPostProcessing() { return m_post_processing; }
    inline const PostProcessing *GetPostProcessing() const { return m_post_processing; }
    inline RenderWindow &GetRenderWindow() { return m_render_window; }
    inline const RenderWindow &GetRenderWindow() const { return m_render_window; }

    inline Bucket &GetBucket(Renderable::RenderBucket bucket) { return m_buckets[bucket]; }

    Bucket m_buckets[Renderable::RB_MAX];

private:
    PostProcessing *m_post_processing;
    Framebuffer2D *m_fbo;
    RenderWindow m_render_window;
    bool m_is_deferred;

    std::map<Entity*, std::size_t> m_hash_cache;
    std::map<std::size_t, Renderable::RenderBucket> m_hash_to_bucket;

    void ClearRenderables();
    void FindRenderables(Camera *cam, Entity *top, bool frustum_culled = false, bool is_root = false);
    bool MemoizedFrustumCheck(Camera *cam, const BoundingBox &aabb);

    void SetRendererDefaults();

    std::map<MemoizedFrustumCheckKey, bool> m_memoized_frustum_checks; // TODO: some sort of deque map
};
} // namespace hyperion

#endif
