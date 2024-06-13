#ifndef HYPERION_FBOM_INTERFACES_HPP
#define HYPERION_FBOM_INTERFACES_HPP

#include <asset/serialization/fbom/FBOMResult.hpp>

#include <core/utilities/UniqueID.hpp>

#include <core/containers/String.hpp>

#include <HashCode.hpp>

namespace hyperion {
class ByteWriter;
namespace fbom {

class FBOMWriter;
class FBOMReader;

class IFBOMSerializable
{
public:
    virtual ~IFBOMSerializable() = default;

    virtual FBOMResult Visit(UniqueID id, FBOMWriter *writer, ByteWriter *out) const = 0;

    [[nodiscard]]
    virtual UniqueID GetUniqueID() const = 0;

    [[nodiscard]]
    virtual HashCode GetHashCode() const = 0;

    [[nodiscard]]
    virtual String ToString(bool deep = true) const = 0;
};

} // namespace fbom
} // namespace hyperion

#endif