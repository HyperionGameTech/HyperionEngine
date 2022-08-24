#ifndef HYPERION_V2_RESOURCE_MAP_HPP
#define HYPERION_V2_RESOURCE_MAP_HPP

#include <core/Containers.hpp>
#include <core/Handle.hpp>

namespace hyperion::v2 {

class Engine;

struct ResourceList {
    FlatMap<HandleID, HandleBase> object_map;
};

class ResourceMap
{
public:

    template <class T>
    bool Add(Handle<T> &&handle)
    {
        if (!handle) {
            return false;
        }

        TypeMap<ResourceList>::Iterator resources_it = GetIterator<T>();

        resources_it->second.object_map.Set(handle.GetID(), std::move(handle));

        return true;
    }

    template <class T>
    bool Remove(const Handle<T> &handle)
    {
        if (!handle) {
            return false;
        }

        TypeMap<ResourceList>::Iterator resources_it = GetIterator<T>();

        return resources_it->second.object_map.Erase(handle.GetID());
    }

private:
    template <class T>
    TypeMap<ResourceList>::Iterator GetIterator()
    {
        TypeMap<ResourceList>::Iterator resources_it = m_resources.Find<T>();

        if (resources_it != m_resources.End()) {
            return resources_it;

        TypeMap<ResourceList>::InsertResult insert_result = m_resources.Set<T>(ResourceList { });

        return insert_result.first;
    }

    TypeMap<ResourceList> m_resources;
};

} // namespace hyperion::v2

#endif