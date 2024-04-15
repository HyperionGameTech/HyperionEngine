/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_FBOM_TYPE_HPP
#define HYPERION_V2_FBOM_TYPE_HPP

#include <core/lib/String.hpp>
#include <core/lib/UniqueID.hpp>
#include <HashCode.hpp>
#include <Types.hpp>

#include <string>
#include <cstddef>

namespace hyperion::v2::fbom {

struct FBOMType
{
    String name;
    SizeType size;
    FBOMType *extends = nullptr;

    FBOMType()
        : name("UNSET"),
          size(0),
          extends(nullptr)
    {
    }

    FBOMType(const String &name, SizeType size)
        : name(name),
          size(size),
          extends(nullptr)
    {
    }

    FBOMType(const String &name, SizeType size, const FBOMType &extends)
        : name(name),
          size(size),
          extends(new FBOMType(extends))
    {
    }

    FBOMType(const FBOMType &other)
        : name(other.name),
          size(other.size),
          extends(nullptr)
    {
        if (other.extends != nullptr) {
            extends = new FBOMType(*other.extends);
        }
    }

    FBOMType &operator=(const FBOMType &other)
    {
        if (extends != nullptr) {
            delete extends;
        }

        name = other.name;
        size = other.size;
        extends = nullptr;

        if (other.extends != nullptr) {
            extends = new FBOMType(*other.extends);
        }

        return *this;
    }

    FBOMType(FBOMType &&other) noexcept
        : name(std::move(other.name)),
          size(other.size),
          extends(other.extends)
    {
        other.size = 0;
        other.extends = nullptr;
    }

    FBOMType &operator=(FBOMType &&other) noexcept
    {
        if (extends != nullptr) {
            delete extends;
        }

        name = std::move(other.name);
        size = other.size;
        extends = other.extends;

        other.size = 0;
        other.extends = nullptr;

        return *this;
    }

    ~FBOMType()
    {
        if (extends != nullptr) {
            delete extends;
        }
    }

    FBOMType Extend(const FBOMType &object) const;

    bool IsOrExtends(const String &name) const
    {
        if (this->name == name) {
            return true;
        }

        if (extends == nullptr) {
            return false;
        }

        return extends->IsOrExtends(name);
    }

    bool IsOrExtends(const FBOMType &other, bool allow_unbounded = true) const
    {
        if (operator==(other)) {
            return true;
        }

        if (allow_unbounded && (IsUnbouned() || other.IsUnbouned())) {
            if (name == other.name && extends == other.extends) { // don't size check
                return true;
            }
        }

        return Extends(other);
    }

    bool Extends(const FBOMType &other) const
    {
        if (extends == nullptr) {
            return false;
        }

        if (*extends == other) {
            return true;
        }

        return extends->Extends(other);
    }

    bool IsUnbouned() const { return size == SizeType(-1); }

    bool operator==(const FBOMType &other) const
    {
        return name == other.name
            && size == other.size
            && extends == other.extends;
    }

    String ToString() const
    {
        String str = name + " (" + String::ToString(size) + ") ";

        if (extends != nullptr) {
            str += "[" + extends->ToString() + "]";
        }

        return str;
    }

    UniqueID GetUniqueID() const
        { return UniqueID(GetHashCode()); }

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

} // namespace hyperion::v2::fbom

#endif
