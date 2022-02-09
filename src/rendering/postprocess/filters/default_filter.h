#ifndef DEFAULT_FILTER_H
#define DEFAULT_FILTER_H

#include "../post_filter.h"

namespace hyperion {
class DefaultFilter : public PostFilter {
public:
    DefaultFilter();
    virtual ~DefaultFilter() = default;

    virtual void SetUniforms(Camera *cam) override;
};
} // namespace hyperion

#endif
