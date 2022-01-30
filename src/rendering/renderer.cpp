#include "renderer.h"
#include "shader_manager.h"
#include "postprocess/filters/deferred_rendering_filter.h"

#include "../util.h"

namespace hyperion {
Renderer::Renderer(const RenderWindow &render_window)
    : m_render_window(render_window),
      m_fbo(nullptr),
      m_is_deferred(false)
{
    m_post_processing = new PostProcessing();

    m_buckets[Renderable::RB_SKY].enable_culling = false;
    m_buckets[Renderable::RB_PARTICLE].enable_culling = false; // TODO
    m_buckets[Renderable::RB_SCREEN].enable_culling = false;
    m_buckets[Renderable::RB_DEBUG].enable_culling = false;
}

Renderer::~Renderer()
{
    delete m_post_processing;
    delete m_fbo;
}

void Renderer::Begin(Camera *cam, Entity *top)
{
    FindRenderables(cam, top, false, true);
}

void Renderer::Render(Camera *cam)
{
    if (m_fbo == nullptr) {
        m_fbo = new Framebuffer2D(
            m_render_window.GetScaledWidth(),
            m_render_window.GetScaledHeight(),
            true, // color
            true, // depth
            true, // normals
            true, // positions
            true, // data
            true, // ao
            true, // tangents
            true  // bitangents
        );
    }

    RenderAll(cam, m_fbo);
}

void Renderer::End(Camera *cam, Entity *top)
{
    RenderPost(cam, m_fbo);

    CoreEngine::GetInstance()->Disable(CoreEngine::GLEnums::CULL_FACE);
    RenderBucket(cam, m_buckets[Renderable::RB_SCREEN]);
    CoreEngine::GetInstance()->Enable(CoreEngine::GLEnums::CULL_FACE);
}

void Renderer::ClearRenderables()
{
   for (int i = 0; i < sizeof(m_buckets) / sizeof(Bucket); i++) {
       m_buckets[i].ClearAll();
   }
}

void Renderer::FindRenderables(Camera *cam, Entity *top, bool frustum_culled, bool is_root)
{
    const size_t entity_hash = top->GetHashCode().Value();
    size_t previous_hash = 0;
    const auto it = m_hash_cache.find(top);
    bool recalc = true;

    Bucket *new_bucket = nullptr;

    if (it != m_hash_cache.end()) {
        previous_hash = it->second;

        if (previous_hash == entity_hash) {
            recalc = false;
        }
    }

    if (recalc) {
        if (top->PendingRemoval()) {
            // std::cout << "[RENDERER] " << top->GetName() << " pending removal, erasing from hash cache\n";
            m_hash_cache.erase(top);
        } else {
            m_hash_cache[top] = entity_hash;
        }
    }

    if (top->GetRenderable() != nullptr) { // NOTE: this currently doesnt handle renderable being set to null.
        const Renderable::RenderBucket new_bucket_index = top->GetRenderable()->GetRenderBucket();

        hard_assert(new_bucket_index < sizeof(m_buckets) / sizeof(Bucket));

        new_bucket = &m_buckets[new_bucket_index];
    
        if (recalc) {
            bool push_new = true;

            if (previous_hash != 0) {
                auto item_in_existing_bucket = m_hash_to_bucket.find(previous_hash);

                Bucket *previous_bucket = nullptr;

                // IF bucket has changed, we have to remove the entry from that bucket first.
                if (item_in_existing_bucket != m_hash_to_bucket.end()) {
                    const Renderable::RenderBucket previous_bucket_index = item_in_existing_bucket->second;

                    hard_assert(previous_bucket_index < sizeof(m_buckets) / sizeof(Bucket));

                    // remove from prev bucket
                    previous_bucket = &m_buckets[previous_bucket_index];

                    m_hash_to_bucket.erase(item_in_existing_bucket);
                }

                if (previous_bucket != nullptr) {
                    if (top->PendingRemoval() || new_bucket != previous_bucket) {
                        previous_bucket->RemoveItem(previous_hash);
                    } else {
                        BucketItem bucket_item;
                        bucket_item.renderable = top->GetRenderable().get();
                        bucket_item.material = &top->GetMaterial();
                        bucket_item.aabb = top->GetAABB();
                        bucket_item.transform = top->GetGlobalTransform();
                        bucket_item.hash_code = entity_hash;

                        previous_bucket->SetItem(previous_hash, bucket_item);

                        push_new = false;
                    }
                }
            }

            if (!top->PendingRemoval()) {
                m_hash_to_bucket[entity_hash] = new_bucket_index;

                if (push_new) {
                    BucketItem bucket_item;
                    bucket_item.renderable = top->GetRenderable().get();
                    bucket_item.material = &top->GetMaterial();
                    bucket_item.aabb = top->GetAABB();
                    bucket_item.transform = top->GetGlobalTransform();
                    bucket_item.hash_code = entity_hash;

                    new_bucket->AddItem(bucket_item);
                }
            }
        }
    }

#if RENDERER_FRUSTUM_CULLING
    if (!frustum_culled && !is_root) {
        frustum_culled = !MemoizedFrustumCheck(cam, top->GetAABB());
    }
#endif

    if (new_bucket != nullptr) {
        if (BucketItem *bucket_item_ptr = new_bucket->GetItemPtr(entity_hash)) {
            bucket_item_ptr->frustum_culled = frustum_culled;
        }
    }

    for (size_t i = 0; i < top->NumChildren(); i++) {
        Entity *child = top->GetChild(i).get();
        FindRenderables(cam, child, frustum_culled);
    }

    for (size_t i = 0; i < top->NumChildrenPendingRemoval(); i++) {
        Entity *child = top->GetChildPendingRemoval(i).get();
        FindRenderables(cam, child, frustum_culled);
    }
}

bool Renderer::MemoizedFrustumCheck(Camera *cam, const BoundingBox &aabb)
{
    return cam->GetFrustum().BoundingBoxInFrustum(aabb);
}

void Renderer::RenderBucket(Camera *cam, Bucket &bucket, Shader *override_shader, bool enable_frustum_culling)
{
    enable_frustum_culling = enable_frustum_culling && bucket.enable_culling;

    Shader *shader = nullptr;
    Shader *last_shader = nullptr;

    for (const BucketItem &it : bucket.GetItems()) {
        if (!it.alive) {
            continue;
        }

        if (enable_frustum_culling && it.frustum_culled) {
            continue;
        }

        // TODO: group by same shader
        shader = (override_shader ? override_shader : it.renderable->m_shader.get());

        if (shader) {
            shader->ApplyMaterial(*it.material);
            shader->ApplyTransforms(it.transform, cam);

#if RENDERER_SHADER_GROUPING
            if (shader != last_shader) {
                if (last_shader != nullptr) {
                    last_shader->End();
                }

                last_shader = shader;

                shader->Use(); // this will call ApplyUniforms() as well
            } else {
                shader->ApplyUniforms(); // re-using shader; have to apply uniforms manually
            }
#else
            shader->Use();
#endif

            it.renderable->Render(this, cam);

#if !RENDERER_SHADER_GROUPING
            shader->End();
#endif

            SetRendererDefaults();
        }
    }

#if RENDERER_SHADER_GROUPING
    if (last_shader != nullptr) {
        last_shader->End();
    }
#endif
}

void Renderer::SetRendererDefaults()
{
    CoreEngine::GetInstance()->BlendFunc(CoreEngine::GLEnums::ONE, CoreEngine::GLEnums::ZERO);
    CoreEngine::GetInstance()->Enable(CoreEngine::GLEnums::DEPTH_TEST);
    CoreEngine::GetInstance()->DepthMask(true);
    CoreEngine::GetInstance()->Enable(CoreEngine::GLEnums::CULL_FACE);
    CoreEngine::GetInstance()->CullFace(CoreEngine::GLEnums::BACK);
    CoreEngine::GetInstance()->BindTexture(CoreEngine::GLEnums::TEXTURE_2D, 0);
    CoreEngine::GetInstance()->BindTexture(CoreEngine::GLEnums::TEXTURE_CUBE_MAP, 0);
}

void Renderer::RenderAll(Camera *cam, Framebuffer2D *fbo)
{
    if (fbo) {
        fbo->Use();
    } else {
        CoreEngine::GetInstance()->Viewport(0, 0, cam->GetWidth(), cam->GetHeight());
    }

    if (!m_buckets[Renderable::RB_BUFFER].IsEmpty()) {
        CoreEngine::GetInstance()->Clear(CoreEngine::GLEnums::COLOR_BUFFER_BIT | CoreEngine::GLEnums::DEPTH_BUFFER_BIT);

        RenderBucket(cam, m_buckets[Renderable::RB_BUFFER]); // PRE
    }

    CoreEngine::GetInstance()->Clear(CoreEngine::GLEnums::COLOR_BUFFER_BIT | CoreEngine::GLEnums::DEPTH_BUFFER_BIT);

    CoreEngine::GetInstance()->Disable(CoreEngine::GLEnums::CULL_FACE);
    RenderBucket(cam, m_buckets[Renderable::RB_SKY]);
    CoreEngine::GetInstance()->Enable(CoreEngine::GLEnums::CULL_FACE);
    RenderBucket(cam, m_buckets[Renderable::RB_OPAQUE]);
    RenderBucket(cam, m_buckets[Renderable::RB_TRANSPARENT]);
    RenderBucket(cam, m_buckets[Renderable::RB_PARTICLE]);
    RenderBucket(cam, m_buckets[Renderable::RB_DEBUG]);

    if (fbo) {
        fbo->End();
    }
}

void Renderer::RenderPost(Camera *cam, Framebuffer2D *fbo)
{
    if (m_post_processing->GetFilters().empty()) {
        return;
    }

    m_post_processing->Render(cam, fbo);
}

void Renderer::SetDeferred(bool deferred)
{
    if (m_is_deferred == deferred) {
        return;
    }

    if (deferred) {
        m_post_processing->AddFilter<DeferredRenderingFilter>("deferred", 10);
    } else {
        m_post_processing->RemoveFilter("deferred");
    }

    ShaderManager::GetInstance()->SetBaseShaderProperties(
        ShaderProperties().Define("DEFERRED", deferred)
    );

    m_is_deferred = deferred;
}
} // namespace hyperion
