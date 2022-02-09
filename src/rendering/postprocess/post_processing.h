#ifndef POST_PROCESSING_H
#define POST_PROCESSING_H

#include <memory>
#include <map>
#include <string>
#include <array>
#include <vector>
#include <algorithm>

#include "../../math/vector2.h"
#include "../mesh.h"
#include "../framebuffer_2d.h"
#include "../camera/camera.h"
#include "./post_filter.h"

namespace hyperion {

class PostProcessing {
public:
    struct Filter;
    struct RenderScale;

    PostProcessing();
    PostProcessing(const PostProcessing &) = delete;
    PostProcessing(PostProcessing &&) = delete;
    PostProcessing &operator=(const PostProcessing &) = delete;
    ~PostProcessing();

    template<typename T>
    void AddFilter(const std::string &tag, int rank = 0)
    {
        static_assert(std::is_base_of<PostFilter, T>::value,
            "Must be a derived class of PostFilter");

        Filter filter(std::make_shared<T>(), tag, rank);

        m_filters.push_back(filter);

        std::sort(m_filters.begin(), m_filters.end(), [](const Filter &a, const Filter &b) {
            return a.rank < b.rank;
        });
    }

    inline const Vector2 &GetRenderScale() const { return m_render_scale; }
    inline void SetRenderScale(const Vector2 &render_scale) { m_render_scale = render_scale; }

    void RemoveFilter(const std::string &tag);
    inline std::vector<Filter> &GetFilters() { return m_filters; }
    inline const std::vector<Filter> &GetFilters() const { return m_filters; }

    void Render(Camera *cam, Framebuffer2D *fbo);

    struct Filter {
        int rank;
        std::string tag;
        std::shared_ptr<PostFilter> filter;

        Filter(const std::shared_ptr<PostFilter> &filter, const std::string &tag, int rank)
            : filter(filter),
                tag(tag),
                rank(rank)
        {
        }

        Filter(const Filter &other)
            : filter(other.filter),
                tag(other.tag),
                rank(other.rank)
        {
        }
    };

private:
    Framebuffer::FramebufferAttachments_t m_chained_textures;
    bool m_chained_textures_initialized;

    Framebuffer2D *m_blit_framebuffer;

    std::vector<Filter> m_filters;

    std::shared_ptr<Mesh> m_quad;
    Vector2 m_render_scale;
};

} // namespace hyperion

#endif
