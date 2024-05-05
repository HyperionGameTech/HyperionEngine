/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_LOADABLE_HPP
#define HYPERION_FBOM_LOADABLE_HPP

#include <asset/serialization/fbom/FBOMType.hpp>

#include <memory>

namespace hyperion::fbom {

class FBOMLoadable
{
public:
    FBOMLoadable(const FBOMType &loadable_type)
        : m_loadable_type(loadable_type)
    {
    }

    virtual ~FBOMLoadable() = default;

    const FBOMType &GetLoadableType() const { return m_loadable_type; }

protected:
    FBOMType    m_loadable_type;
};

} // namespace hyperion::fbom

#endif
