#include <core/utilities/UniqueId.hpp>
#include <core/utilities/Uuid.hpp>

namespace hyperion {
namespace utilities {

UniqueId::UniqueId()
    : m_value(Generate().m_value)
{
}

UniqueId UniqueId::Generate()
{
    return UniqueId { Uuid {}.GetHashCode() };
}

UniqueId UniqueId::FromUUID(const Uuid& uuid)
{
    return UniqueId { HashCode::GetHashCode(uuid.data0).Combine(HashCode::GetHashCode(uuid.data1)) };
}

} // namespace utilities
} // namespace hyperion