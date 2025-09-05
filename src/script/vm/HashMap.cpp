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

void Script_HashMap::SetElement(VMMapKey&& key, Script_Value&& value)
{
    m_map.Set(std::move(key), std::move(value));
}

Script_Value* Script_HashMap::GetElement(const VMMapKey& key)
{
    auto it = m_map.Find(key);

    if (it == m_map.End())
    {
        return nullptr;
    }

    return &it->second;
}

const Script_Value* Script_HashMap::GetElement(const VMMapKey& key) const
{
    auto it = m_map.Find(key);

    if (it == m_map.End())
    {
        return nullptr;
    }

    return &it->second;
}

void Script_HashMap::GetRepresentation(
    std::stringstream& ss,
    bool addTypeName,
    int depth) const
{
    if (depth == 0)
    {
        ss << "{...}";

        return;
    }

    // convert hashmap to string
    const char sepStr[3] = ", ";

    ss << '{';

    // all elements
    for (auto it = m_map.Begin(); it != m_map.End(); ++it)
    {
        // convert key to string
        it->first.key.ToRepresentation(
            ss,
            addTypeName,
            depth - 1);

        ss << " => ";

        // convert value to string
        it->second.ToRepresentation(
            ss,
            addTypeName,
            depth - 1);

        auto next = it;
        ++next;

        if (next != m_map.end())
        {
            ss << sepStr;
        }
    }

    ss << '}';
}

} // namespace hyperion
