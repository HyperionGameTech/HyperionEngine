/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_ARRAY_HPP
#define HYPERION_FBOM_ARRAY_HPP

#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/FlatMap.hpp>

#include <core/utilities/Optional.hpp>
#include <core/utilities/UniqueID.hpp>

#include <core/Name.hpp>

#include <asset/serialization/fbom/FBOMBaseTypes.hpp>
#include <asset/serialization/fbom/FBOMData.hpp>
#include <asset/serialization/fbom/FBOMInterfaces.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#include <type_traits>

namespace hyperion {
namespace fbom {

class FBOMArray : public IFBOMSerializable
{
public:
    FBOMArray();
    FBOMArray(const Array<FBOMData> &values);
    FBOMArray(Array<FBOMData> &&values);
    FBOMArray(const FBOMArray &other);
    FBOMArray &operator=(const FBOMArray &other);
    FBOMArray(FBOMArray &&other) noexcept;
    FBOMArray &operator=(FBOMArray &&other) noexcept;
    virtual ~FBOMArray();

    FBOMArray &AddElement(const FBOMData &value);
    FBOMArray &AddElement(FBOMData &&value);

    HYP_NODISCARD
    const FBOMData &GetElement(SizeType index) const;

    HYP_NODISCARD
    const FBOMData *TryGetElement(SizeType index) const;

    HYP_NODISCARD HYP_FORCE_INLINE
    SizeType Size() const
        { return m_values.Size(); }

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
        hc.Add(m_values.GetHashCode());

        return hc;
    }

private:
    Array<FBOMData> m_values;
};

} // namespace fbom
} // namespace hyperion

#endif
