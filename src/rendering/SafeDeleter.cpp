#include "SafeDeleter.hpp"

namespace hyperion::v2 {

void SafeDeleter::PerformEnqueuedDeletions()
{
    if (auto deletion_flags = m_render_resource_deletion_flag.load()) {
        std::lock_guard guard(m_render_resource_deletion_mutex);

        if (deletion_flags & RENDERABLE_DELETION_TEXTURES) {
            if (DeleteEnqueuedItemsOfType<Texture>()) {
                deletion_flags &= ~RENDERABLE_DELETION_TEXTURES;
            }
        }

        if (deletion_flags & RENDERABLE_DELETION_MESHES) {
            if (DeleteEnqueuedItemsOfType<Mesh>()) {
                deletion_flags &= ~RENDERABLE_DELETION_MESHES;
            }
        }

        if (deletion_flags & RENDERABLE_DELETION_SKELETONS) {
            if (DeleteEnqueuedItemsOfType<Skeleton>()) {
                deletion_flags &= ~RENDERABLE_DELETION_SKELETONS;
            }
        }

        if (deletion_flags & RENDERABLE_DELETION_SHADERS) {
            if (DeleteEnqueuedItemsOfType<Shader>()) {
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

        if (deletion_flags & RENDERABLE_DELETION_TEXTURES) {
            if (DeleteAllItemsOfType<Texture>()) {
                deletion_flags &= ~RENDERABLE_DELETION_TEXTURES;
            }
        }

        if (deletion_flags & RENDERABLE_DELETION_MESHES) {
            if (DeleteAllItemsOfType<Mesh>()) {
                deletion_flags &= ~RENDERABLE_DELETION_MESHES;
            }
        }

        if (deletion_flags & RENDERABLE_DELETION_SKELETONS) {
            if (DeleteAllItemsOfType<Skeleton>()) {
                deletion_flags &= ~RENDERABLE_DELETION_SKELETONS;
            }
        }

        if (deletion_flags & RENDERABLE_DELETION_SHADERS) {
            if (DeleteAllItemsOfType<Shader>()) {
                deletion_flags &= ~RENDERABLE_DELETION_SHADERS;
            }
        }

        m_render_resource_deletion_flag.store(deletion_flags);
    }
}

} // namespace hyperion::v2