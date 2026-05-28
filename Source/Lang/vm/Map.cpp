#include <Lang/vm/Map.hpp>

#include <Core/debug/Debug.hpp>

#include <cmath>
#include <cstring>
#include <sstream>

namespace Hyperion {

void ScriptMap::SetElement(ScriptMapKey&& key, BoxedValue&& value)
{
    Base::Set(std::move(key), std::move(value));
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
