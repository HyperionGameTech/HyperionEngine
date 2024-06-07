/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_TYPE_HPP
#define HYPERION_FBOM_TYPE_HPP

#include <core/containers/String.hpp>
#include <core/utilities/StringView.hpp>
#include <core/utilities/UniqueID.hpp>
#include <HashCode.hpp>
#include <Types.hpp>

#include <cstddef>

namespace hyperion::fbom {

struct FBOMType
{
    ANSIString  name;
    SizeType    size;
    FBOMType    *extends;

    FBOMType();
    FBOMType(const ANSIStringView &name, SizeType size);
    FBOMType(const ANSIStringView &name, SizeType size, const FBOMType &extends);
    FBOMType(const FBOMType &other);
    FBOMType &operator=(const FBOMType &other);
    FBOMType(FBOMType &&other) noexcept;
    FBOMType &operator=(FBOMType &&other) noexcept;
    ~FBOMType();

    FBOMType Extend(const FBOMType &object) const;
    
    [[nodiscard]]
    bool IsOrExtends(const ANSIStringView &name) const;

    [[nodiscard]]
    bool IsOrExtends(const FBOMType &other, bool allow_unbounded = true) const;

    [[nodiscard]]
    bool Extends(const FBOMType &other) const;

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool IsUnbouned() const
        { return size == SizeType(-1); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator==(const FBOMType &other) const
    {
        return name == other.name
            && size == other.size
            && extends == other.extends;
    }

    [[nodiscard]]
    String ToString() const;

    [[nodiscard]]
    HYP_FORCE_INLINE
    UniqueID GetUniqueID() const
        { return UniqueID(GetHashCode()); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(name);
        hc.Add(size);

        if (extends != nullptr) {
            hc.Add(extends->GetHashCode());
        }

        return hc;
    }
};

} // namespace hyperion::fbom

#endif
