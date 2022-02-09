#ifndef FBOM_TYPE_H
#define FBOM_TYPE_H

#include "../../hash_code.h"

#include <string>
#include <cstddef>

namespace hyperion {
namespace fbom {

struct FBOMType {
    std::string name;
    size_t size;
    FBOMType *extends = nullptr;

    FBOMType()
        : name("UNSET"),
          size(0),
          extends(nullptr)
    {
    }

    FBOMType(const std::string &name, size_t size, const FBOMType *extends = nullptr)
        : name(name),
          size(size),
          extends(nullptr)
    {
        if (extends != nullptr) {
            this->extends = new FBOMType(*extends);
        }
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

    inline FBOMType &operator=(const FBOMType &other)
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

    ~FBOMType()
    {
        if (extends != nullptr) {
            delete extends;
        }
    }

    FBOMType Extend(const FBOMType &object) const;

    bool IsOrExtends(const std::string &name) const
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

    inline bool Extends(const FBOMType &other) const
    {
        if (extends == nullptr) {
            return false;
        }

        if (*extends == other) {
            return true;
        }

        return extends->Extends(other);
    }

    inline bool IsUnbouned() const { return size == 0; }

    inline bool operator==(const FBOMType &other) const
    {
        return name == other.name
            && size == other.size
            && extends == other.extends;
    }

    std::string ToString() const
    {
        std::string str = name + " (" + std::to_string(size) + ") ";

        if (extends != nullptr) {
            str += "[" + extends->ToString() + "]";
        }

        return str;
    }

    inline HashCode GetHashCode() const
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
