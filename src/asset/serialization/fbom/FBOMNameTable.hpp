/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_NAME_TABLE_HPP
#define HYPERION_FBOM_NAME_TABLE_HPP

#include <asset/serialization/fbom/FBOMInterfaces.hpp>

#include <core/containers/TypeMap.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/String.hpp>
#include <core/utilities/Variant.hpp>
#include <core/utilities/UniqueID.hpp>
#include <core/Name.hpp>

#include <util/fs/FsUtil.hpp>

#include <Constants.hpp>
#include <Types.hpp>
#include <HashCode.hpp>

#include <type_traits>

namespace hyperion {
namespace fbom {

struct FBOMNameTable : public IFBOMSerializable
{
    HashMap<WeakName, ANSIString>   values;

    virtual ~FBOMNameTable() = default;

    HYP_FORCE_INLINE
    WeakName Add(const ANSIStringView &str)
    {
        const WeakName name = CreateWeakNameFromDynamicString(str);

        return Add(str, name);
    }

    HYP_FORCE_INLINE
    WeakName Add(const ANSIStringView &str, WeakName name)
    {
        values.Insert(name, str);

        return name;
    }

    void RegisterAllNamesGlobally()
    {
        for (const auto &it : values) {
            CreateNameFromDynamicString(it.second);
        }
    }

    FBOMResult Visit(FBOMWriter *writer, ByteWriter *out) const
        { return Visit(GetUniqueID(), writer, out); }

    virtual FBOMResult Visit(UniqueID id, FBOMWriter *writer, ByteWriter *out) const override;

    [[nodiscard]]
    virtual String ToString(bool deep = true) const override
    {
        String str;

        for (const auto &pair : values) {
            str += String::ToString(pair.first.GetID()) + " : " + String(pair.second) + "\n";
        }

        return str;
    }

    [[nodiscard]]
    virtual UniqueID GetUniqueID() const override
        { return UniqueID(GetHashCode()); }

    [[nodiscard]]
    virtual HashCode GetHashCode() const override
        { return values.GetHashCode(); }
};

} // namespace fbom
} // namespace hyperion

#endif
