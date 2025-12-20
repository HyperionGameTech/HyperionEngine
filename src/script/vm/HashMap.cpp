#include <script/vm/HashMap.hpp>

#include <core/debug/Debug.hpp>

#include <cmath>
#include <cstring>
#include <sstream>

namespace hyperion {

Script_HashMap::Script_HashMap()
{
}

Script_HashMap::Script_HashMap(Script_HashMap&& other) noexcept
    : m_map(std::move(other.m_map))
{
}

Script_HashMap& Script_HashMap::operator=(Script_HashMap&& other) noexcept
{
    if (&other == this)
    {
        return *this;
    }

    m_map = std::move(other.m_map);

    return *this;
}

Script_HashMap::~Script_HashMap()
{
}

void Script_HashMap::SetElement(VMMapKey&& key, BoxedValue&& value)
{
    m_map.Set(std::move(key), std::move(value));
}

BoxedValue* Script_HashMap::GetElement(const VMMapKey& key)
{
    auto it = m_map.Find(key);

    if (it == m_map.End())
    {
        return nullptr;
    }

    return &it->second;
}

const BoxedValue* Script_HashMap::GetElement(const VMMapKey& key) const
{
    auto it = m_map.Find(key);

    if (it == m_map.End())
    {
        return nullptr;
    }

    return &it->second;
}

} // namespace hyperion
