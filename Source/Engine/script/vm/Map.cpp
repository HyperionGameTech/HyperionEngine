#include <ScriptPch.hpp>

#include <script/vm/Map.hpp>

#include <Core/debug/Debug.hpp>

#include <cmath>
#include <cstring>
#include <sstream>

namespace Hyperion {

ScriptMap::ScriptMap()
{
}

ScriptMap::ScriptMap(ScriptMap&& other) noexcept
    : m_map(std::move(other.m_map))
{
}

ScriptMap& ScriptMap::operator=(ScriptMap&& other) noexcept
{
    if (&other == this)
    {
        return *this;
    }

    m_map = std::move(other.m_map);

    return *this;
}

ScriptMap::~ScriptMap()
{
}

void ScriptMap::SetElement(VMMapKey&& key, BoxedValue&& value)
{
    m_map.Set(std::move(key), std::move(value));
}

BoxedValue* ScriptMap::GetElement(const VMMapKey& key)
{
    auto it = m_map.Find(key);

    if (it == m_map.End())
    {
        return nullptr;
    }

    return &it->second;
}

const BoxedValue* ScriptMap::GetElement(const VMMapKey& key) const
{
    auto it = m_map.Find(key);

    if (it == m_map.End())
    {
        return nullptr;
    }

    return &it->second;
}

} // namespace Hyperion
