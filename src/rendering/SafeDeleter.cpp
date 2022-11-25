#include "SafeDeleter.hpp"

namespace hyperion::v2 {

void SafeDeleter::PerformEnqueuedDeletions()
{
    if (auto deletion_flags = m_render_resource_deletion_flag.load()) {
        std::lock_guard guard(m_render_resource_deletion_mutex);

        if (deletion_flags & RENDERABLE_DELETION_BUFFERS_OR_IMAGES) {
            if (DeleteEnqueuedBuffersAndImages(engine)) {
                deletion_flags &= ~RENDERABLE_DELETION_BUFFERS_OR_IMAGES;
            }
        }

        if (deletion_flags & RENDERABLE_DELETION_TEXTURES) {
            if (DeleteEnqueuedHandlesOfType<Texture>(engine)) {
                deletion_flags &= ~RENDERABLE_DELETION_TEXTURES;
            }
        }

        if (deletion_flags & RENDERABLE_DELETION_MESHES) {
            if (DeleteEnqueuedHandlesOfType<Mesh>(engine)) {
                deletion_flags &= ~RENDERABLE_DELETION_MESHES;
            }
        }

        if (deletion_flags & RENDERABLE_DELETION_SKELETONS) {
            if (DeleteEnqueuedHandlesOfType<Skeleton>(engine)) {
                deletion_flags &= ~RENDERABLE_DELETION_SKELETONS;
            }
        }

        if (deletion_flags & RENDERABLE_DELETION_SHADERS) {
            if (DeleteEnqueuedHandlesOfType<Shader>(engine)) {
                deletion_flags &= ~RENDERABLE_DELETION_SHADERS;
            }
        }

        m_render_resource_deletion_flag.store(deletion_flags);
    }
}

void SafeDeleter::ForceReleaseAll()
{
    if (auto deletion_flags = m_render_resource_deletion_flag.load()) {
        std::lock_guard guard(m_render_resource_deletion_mutex);

        if (deletion_flags & RENDERABLE_DELETION_BUFFERS_OR_IMAGES) {
            ForceDeleteBuffersAndImages(engine);
            deletion_flags &= ~RENDERABLE_DELETION_BUFFERS_OR_IMAGES;
        }

        if (deletion_flags & RENDERABLE_DELETION_TEXTURES) {
            ForceDeleteHandlesOfType<Texture>(engine);
            deletion_flags &= ~RENDERABLE_DELETION_TEXTURES;
        }

        if (deletion_flags & RENDERABLE_DELETION_MESHES) {
            ForceDeleteHandlesOfType<Mesh>(engine);
            deletion_flags &= ~RENDERABLE_DELETION_MESHES;
        }

        if (deletion_flags & RENDERABLE_DELETION_SKELETONS) {
            ForceDeleteHandlesOfType<Skeleton>(engine);
            deletion_flags &= ~RENDERABLE_DELETION_SKELETONS;
        }

        if (deletion_flags & RENDERABLE_DELETION_SHADERS) {
            ForceDeleteHandlesOfType<Shader>(engine);
            deletion_flags &= ~RENDERABLE_DELETION_SHADERS;
        }

        m_render_resource_deletion_flag.store(deletion_flags);
    }
}

} // namespace hyperion::v2