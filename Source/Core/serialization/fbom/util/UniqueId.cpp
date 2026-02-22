#include <Core/serialization/fbom/util/UniqueId.hpp>
#include <Core/utilities/Uuid.hpp>

namespace Hyperion {
namespace utilities {

UniqueId::UniqueId()
    : m_value(Generate().m_value)
{
}

UniqueId UniqueId::Generate()
{
    return UniqueId { UUID {}.GetHashCode() };
}

UniqueId UniqueId::FromUUID(const UUID& uuid)
{
    return UniqueId { HashCode::GetHashCode(uuid.data0).Combine(HashCode::GetHashCode(uuid.data1)) };
}

} // namespace utilities
} // namespace Hyperion