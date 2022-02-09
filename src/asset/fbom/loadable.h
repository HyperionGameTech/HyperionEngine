#ifndef FBOM_LOADABLE_H
#define FBOM_LOADABLE_H

#include "../loadable.h"
#include "type.h"

#include <memory>

namespace hyperion {
namespace fbom {

class FBOMLoadable : public Loadable {
public:
    FBOMLoadable(const FBOMType &loadable_type)
        : m_loadable_type(loadable_type)
    {
    }

    virtual ~FBOMLoadable() = default;

    inline const FBOMType &GetLoadableType() const { return m_loadable_type; }

    virtual std::shared_ptr<Loadable> Clone() = 0;

protected:
    FBOMType m_loadable_type;
};

using FBOMDeserialized = std::shared_ptr<FBOMLoadable>;

} // namespace fbom
} // namespace hyperion

#endif
