/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_TYPE_HPP
#define HYPERION_FBOM_TYPE_HPP

#include <asset/serialization/fbom/FBOMInterfaces.hpp>

#include <core/containers/String.hpp>

#include <core/utilities/StringView.hpp>
#include <core/utilities/UniqueID.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

#include <cstddef>

namespace hyperion {

enum class FBOMTypeFlags : uint8
{
    NONE        = 0x0,
    RAW_DATA    = 0x1,
    CONTAINER   = 0x2, // needs custom handling

    DEFAULT     = RAW_DATA
};

HYP_MAKE_ENUM_FLAGS(FBOMTypeFlags)

namespace fbom {

class HYP_API FBOMType : public IFBOMSerializable
{
public:
    ANSIString                  name;
    SizeType                    size;
    EnumFlags<FBOMTypeFlags>    flags;
    FBOMType                    *extends;

    FBOMType();
    FBOMType(const ANSIStringView &name, SizeType size);
    FBOMType(const ANSIStringView &name, SizeType size, const FBOMType &extends);
    FBOMType(const ANSIStringView &name, SizeType size, EnumFlags<FBOMTypeFlags> flags);
    FBOMType(const ANSIStringView &name, SizeType size, EnumFlags<FBOMTypeFlags> flags, const FBOMType &extends);
    FBOMType(const FBOMType &other);
    FBOMType &operator=(const FBOMType &other);
    FBOMType(FBOMType &&other) noexcept;
    FBOMType &operator=(FBOMType &&other) noexcept;
    virtual ~FBOMType();

    FBOMType Extend(const FBOMType &object) const;

    HYP_NODISCARD
    bool HasAnyFlagsSet(EnumFlags<FBOMTypeFlags> flags, bool include_parents = true) const;

    HYP_NODISCARD
    bool Is(const FBOMType &other, bool allow_unbounded = true) const;

    HYP_NODISCARD
    bool IsOrExtends(const ANSIStringView &name) const;

    HYP_NODISCARD
    bool IsOrExtends(const FBOMType &other, bool allow_unbounded = true) const;

    HYP_NODISCARD
    bool Extends(const FBOMType &other, bool allow_unbounded = true) const;

    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsUnbounded() const
        { return size == SizeType(-1); }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsUnset() const
        { return name == "UNSET"; }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool operator==(const FBOMType &other) const
    {
        return name == other.name
            && size == other.size
            && extends == other.extends;
    }

    FBOMResult Visit(FBOMWriter *writer, ByteWriter *out, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE) const
        { return Visit(GetUniqueID(), writer, out, attributes); }

    virtual FBOMResult Visit(UniqueID id, FBOMWriter *writer, ByteWriter *out, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE) const override;

    HYP_NODISCARD
    virtual String ToString(bool deep = true) const override;

    HYP_NODISCARD
    virtual UniqueID GetUniqueID() const override
        { return UniqueID(GetHashCode()); }
    
    HYP_NODISCARD
    virtual HashCode GetHashCode() const override
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

} // namespace fbom
} // namespace hyperion

#endif
