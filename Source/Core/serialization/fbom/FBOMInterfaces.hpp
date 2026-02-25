/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/serialization/fbom/FBOMResult.hpp>
#include <Core/serialization/fbom/FBOMEnums.hpp>
#include <Core/serialization/fbom/util/UniqueId.hpp>

#include <Core/utilities/EnumFlags.hpp>

#include <Core/memory/RefCountedPtr.hpp>

#include <Core/containers/String.hpp>

#include <Core/Defines.hpp>

#include <Core/HashCode.hpp>

namespace Hyperion {
class ByteWriter;

namespace serialization {

class FBOMWriter;
class FBOMReader;
class FBOMLoadContext;

class FBOMSerializableBase
{
public:
    friend class FBOMReader;
    friend class FBOMWriter;

    FBOMSerializableBase() = default;
    FBOMSerializableBase(const FBOMSerializableBase&) = default;
    FBOMSerializableBase& operator=(const FBOMSerializableBase&) = default;
    FBOMSerializableBase(FBOMSerializableBase&&) noexcept = default;
    FBOMSerializableBase& operator=(FBOMSerializableBase&&) noexcept = default;
    virtual ~FBOMSerializableBase() = default;

    virtual FBOMResult Visit(UniqueId id, FBOMWriter* writer, ByteWriter* out, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE) const = 0;

    virtual UniqueId GetUniqueID() const = 0;
    virtual HashCode GetHashCode() const = 0;
    virtual String ToString(bool deep = true) const = 0;
};

} // namespace serialization
} // namespace Hyperion
