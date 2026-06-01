#include <Lang/VM/Map.hpp>

#include <Core/Debug/Debug.hpp>

#include <cmath>
#include <cstring>
#include <sstream>

namespace Hyperion {

void ScriptMap::SetElement(ScriptMapKey&& key, BoxedValue&& value)
{
    auto it = Base::Find(key);

    if (it == Base::End())
    {
        Base::Insert(std::move(key), std::move(value));
        return;
    }

    (void)key;
    it->second = std::move(value);
}

BoxedValue* ScriptMap::GetElement(const ScriptMapKey& key)
{
    auto it = Base::Find(key);

    if (it == Base::End())
    {
        return nullptr;
    }

    return &it->second;
}

const BoxedValue* ScriptMap::GetElement(const ScriptMapKey& key) const
{
    auto it = Base::Find(key);

    if (it == Base::End())
    {
        return nullptr;
    }

    return &it->second;
}

} // namespace Hyperion
