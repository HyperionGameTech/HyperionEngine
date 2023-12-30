#include <rendering/SafeDeleter.hpp>
#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {


struct RENDER_COMMAND(RemoveTextureFromBindlessStorage) : renderer::RenderCommand
{
    ID<Texture> id;

    RENDER_COMMAND(RemoveTextureFromBindlessStorage)(ID<Texture> id) : id(id)
    {
    }

    virtual Result operator()()
    {
        g_engine->GetRenderData()->textures.RemoveResource(id);

        HYPERION_RETURN_OK;
    }
};

void SafeDeleter::PerformEnqueuedDeletions()
{
    if (auto deletion_flags = m_render_resource_deletion_flag.Get(MemoryOrder::ACQUIRE)) {
        Mutex::Guard guard(m_render_resource_deletion_mutex);

        if (deletion_flags & RENDERABLE_DELETION_BUFFERS_OR_IMAGES) {
            if (DeleteEnqueuedBuffersAndImages()) {
                deletion_flags &= ~RENDERABLE_DELETION_BUFFERS_OR_IMAGES;
            }
        }

        if (deletion_flags & RENDERABLE_DELETION_TEXTURES) {
            if (DeleteEnqueuedHandlesOfType<Texture>()) {
                deletion_flags &= ~RENDERABLE_DELETION_TEXTURES;
            }
        }

        if (deletion_flags & RENDERABLE_DELETION_MATERIALS) {
            if (DeleteEnqueuedHandlesOfType<Material>()) {
                deletion_flags &= ~RENDERABLE_DELETION_MATERIALS;
            }
        }

        if (deletion_flags & RENDERABLE_DELETION_MESHES) {
            if (DeleteEnqueuedHandlesOfType<Mesh>()) {
                deletion_flags &= ~RENDERABLE_DELETION_MESHES;
            }
        }

        if (deletion_flags & RENDERABLE_DELETION_SKELETONS) {
            if (DeleteEnqueuedHandlesOfType<Skeleton>()) {
                deletion_flags &= ~RENDERABLE_DELETION_SKELETONS;
            }
        }

        if (deletion_flags & RENDERABLE_DELETION_SHADERS) {
            if (DeleteEnqueuedHandlesOfType<Shader>()) {
                deletion_flags &= ~RENDERABLE_DELETION_SHADERS;
            }
        }

        m_render_resource_deletion_flag.Set(deletion_flags, MemoryOrder::RELEASE);
    }
}

void SafeDeleter::ForceReleaseAll()
{
    if (auto deletion_flags = m_render_resource_deletion_flag.Get(MemoryOrder::ACQUIRE)) {
        Mutex::Guard guard(m_render_resource_deletion_mutex);

        if (deletion_flags & RENDERABLE_DELETION_BUFFERS_OR_IMAGES) {
            ForceDeleteBuffersAndImages();
            deletion_flags &= ~RENDERABLE_DELETION_BUFFERS_OR_IMAGES;
        }

        if (deletion_flags & RENDERABLE_DELETION_TEXTURES) {
            ForceDeleteHandlesOfType<Texture>();
            deletion_flags &= ~RENDERABLE_DELETION_TEXTURES;
        }

        if (deletion_flags & RENDERABLE_DELETION_MATERIALS) {
            ForceDeleteHandlesOfType<Material>();
            deletion_flags &= ~RENDERABLE_DELETION_MATERIALS;
        }

        if (deletion_flags & RENDERABLE_DELETION_MESHES) {
            ForceDeleteHandlesOfType<Mesh>();
            deletion_flags &= ~RENDERABLE_DELETION_MESHES;
        }

        if (deletion_flags & RENDERABLE_DELETION_SKELETONS) {
            ForceDeleteHandlesOfType<Skeleton>();
            deletion_flags &= ~RENDERABLE_DELETION_SKELETONS;
        }

        if (deletion_flags & RENDERABLE_DELETION_SHADERS) {
            ForceDeleteHandlesOfType<Shader>();
            deletion_flags &= ~RENDERABLE_DELETION_SHADERS;
        }

        m_render_resource_deletion_flag.Set(deletion_flags, MemoryOrder::RELEASE);
    }
}

void SafeDeleter::EnqueueTextureBindlessStorageRemoval(ID<Texture> id)
{
    if (g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures()) {
        PUSH_RENDER_COMMAND(RemoveTextureFromBindlessStorage, id);
    }
}

} // namespace hyperion::v2