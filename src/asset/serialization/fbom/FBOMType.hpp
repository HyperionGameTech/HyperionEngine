/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_TYPE_HPP
#define HYPERION_FBOM_TYPE_HPP

#include <asset/serialization/fbom/FBOMInterfaces.hpp>

#include <core/containers/String.hpp>

#include <core/utilities/StringView.hpp>
#include <core/utilities/UniqueID.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/TypeID.hpp>

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
    TypeID                      type_id;
    EnumFlags<FBOMTypeFlags>    flags;
    FBOMType                    *extends;

    FBOMType();
    FBOMType(const ANSIStringView &name, SizeType size, TypeID type_id);
    FBOMType(const ANSIStringView &name, SizeType size, TypeID type_id, const FBOMType &extends);
    FBOMType(const ANSIStringView &name, SizeType size, TypeID type_id, EnumFlags<FBOMTypeFlags> flags);
    FBOMType(const ANSIStringView &name, SizeType size, TypeID type_id, EnumFlags<FBOMTypeFlags> flags, const FBOMType &extends);
    FBOMType(const FBOMType &other);
    FBOMType &operator=(const FBOMType &other);
    FBOMType(FBOMType &&other) noexcept;
    FBOMType &operator=(FBOMType &&other) noexcept;
    virtual ~FBOMType();

    FBOMType Extend(const FBOMType &object) const;

    bool HasAnyFlagsSet(EnumFlags<FBOMTypeFlags> flags, bool include_parents = true) const;

    bool Is(const FBOMType &other, bool allow_unbounded = true, bool allow_void_type_id = true) const;
    bool IsOrExtends(const ANSIStringView &name, bool allow_unbounded = true, bool allow_void_type_id = true) const;
    bool IsOrExtends(const FBOMType &other, bool allow_unbounded = true, bool allow_void_type_id = true) const;
    bool Extends(const FBOMType &other, bool allow_unbounded = true, bool allow_void_type_id = true) const;

    HYP_FORCE_INLINE bool IsUnbounded() const
        { return size == SizeType(-1); }

    HYP_FORCE_INLINE bool IsUnset() const
        { return name == "UNSET"; }

    HYP_FORCE_INLINE bool operator==(const FBOMType &other) const
    {
        return name == other.name
            && size == other.size
            && type_id == other.type_id
            && extends == other.extends;
    }

    /*! \brief Get the C++ TypeID of this type object.
     *  \note Not all types will give a valid TypeID, which is OK - not all types correspond
     *  directly to a C++ type. */
    HYP_FORCE_INLINE TypeID GetTypeID() const
        { return type_id; }

    HYP_FORCE_INLINE bool HasNativeTypeID() const
        { return type_id != TypeID::Void(); }

    FBOMResult Visit(FBOMWriter *writer, ByteWriter *out, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE) const
        { return Visit(GetUniqueID(), writer, out, attributes); }

    virtual FBOMResult Visit(UniqueID id, FBOMWriter *writer, ByteWriter *out, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE) const override;

    virtual String ToString(bool deep = true) const override;

    virtual UniqueID GetUniqueID() const override
        { return UniqueID(GetHashCode()); }
    
    virtual HashCode GetHashCode() const override
    {
        HashCode hc;

        hc.Add(name);
        hc.Add(size);
        hc.Add(type_id);

        if (extends != nullptr) {
            hc.Add(extends->GetHashCode());
        }

        return hc;
    }
};

} // namespace fbom
} // namespace hyperion

#endif
