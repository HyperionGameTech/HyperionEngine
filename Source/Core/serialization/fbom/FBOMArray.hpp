/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/containers/String.hpp>
#include <Core/containers/Array.hpp>

#include <Core/serialization/fbom/FBOMBaseTypes.hpp>
#include <Core/serialization/fbom/FBOMData.hpp>
#include <Core/serialization/fbom/FBOMInterfaces.hpp>
#include <Core/serialization/fbom/util/UniqueId.hpp>

#include <Core/Types.hpp>
#include <Core/Constants.hpp>

#include <type_traits>

namespace Hyperion {
namespace serialization {

class HYP_API FBOMArray final : public FBOMSerializableBase
{
public:
    FBOMArray();

    explicit FBOMArray(const Array<FBOMData>& values);
    explicit FBOMArray(Array<FBOMData>&& values);

    FBOMArray(const FBOMArray& other);
    FBOMArray& operator=(const FBOMArray& other);

    FBOMArray(FBOMArray&& other) noexcept;
    FBOMArray& operator=(FBOMArray&& other) noexcept;

    virtual ~FBOMArray() override;

    HYP_FORCE_INLINE size_t Size() const
    {
        return m_values.Size();
    }

    FBOMArray& AddElement(const FBOMData& value);
    FBOMArray& AddElement(FBOMData&& value);

    FBOMData& GetElement(size_t index);
    const FBOMData& GetElement(size_t index) const;
    const FBOMData* TryGetElement(size_t index) const;

    FBOMResult Visit(FBOMWriter* writer, ByteWriter* out, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE) const
    {
        return Visit(GetUniqueID(), writer, out, attributes);
    }

    virtual FBOMResult Visit(UniqueId id, FBOMWriter* writer, ByteWriter* out, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE) const override;

    virtual String ToString(bool deep = true) const override;
    virtual UniqueId GetUniqueID() const override;
    virtual HashCode GetHashCode() const override;

private:
    Array<FBOMData> m_values;
};

} // namespace serialization
} // namespace Hyperion
