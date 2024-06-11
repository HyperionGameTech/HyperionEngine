#ifndef HYPERION_FBOM_INTERFACES_HPP
#define HYPERION_FBOM_INTERFACES_HPP

#include <core/utilities/UniqueID.hpp>

#include <core/containers/String.hpp>

#include <HashCode.hpp>

namespace hyperion::fbom {

class IFBOMSerializable
{
public:
    virtual ~IFBOMSerializable() = default;

    [[nodiscard]]
    virtual UniqueID GetUniqueID() const = 0;

    [[nodiscard]]
    virtual HashCode GetHashCode() const = 0;

    [[nodiscard]]
    virtual String ToString() const = 0;
};

} // namespace hyperion::fbom

#endif