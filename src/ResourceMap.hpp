#ifndef HYPERION_V2_RESOURCE_MAP_HPP
#define HYPERION_V2_RESOURCE_MAP_HPP

#include <core/Containers.hpp>
#include <core/Handle.hpp>

#include <mutex>

namespace hyperion::v2 {

class Engine;

struct ResourceOwnership
{
    void *self;
    DynArray<HandleID> owners;
    DynArray<HandleID> children;
};

struct ResourceList
{
    FlatMap<HandleID, ResourceOwnership> object_map;
};

#if 0
class ResourceMap
{
public:

    template <class T>
    void Add(typename Handle<T>::ID handle_id, T *ptr)
    {
        AssertThrow(bool(handle_id));
        AssertThrow(ptr);
        AssertThrow(handle_id.type_id == TypeID::ForType<NormalizedType<T>>());

        TypeMap<ResourceList>::Iterator resources_it = GetIterator<NormalizedType<T>>();
        auto &object_map = resources_it->second.object_map;
        AssertThrow(!object_map.Contains(handle_id));

        object_map.Set(handle_id, ResourceOwnership {
            .self = ptr,
            .owners = { }
        });
    }

    template <class T>
    void AddRelationship(typename Handle<T>::ID handle_id, HandleID parent)
    {
        AssertThrow(bool(handle_id));
        AssertThrow(ptr);
        AssertThrow(handle_id.type_id == TypeID::ForType<NormalizedType<T>>());

        TypeMap<ResourceList>::Iterator resources_it = GetIterator<NormalizedType<T>>();
        auto &object_map = resources_it->second.object_map;
        AssertThrow(object_map.Contains(handle_id));

        object_map.At(handle_id).owners.PushBack(parent);
    }

    template <class T>
    bool Remove(typename Handle<T>::ID handle_id)
    {
        if (!handle_id) {
            return false;
        }

        // TypeMap<ResourceList>::Iterator resources_it = GetIterator<T>();
        // auto &object_map = resources_it->second.object_map;
        // AssertThrow(object_map.Contains(handle_id));

        AssertThrow(m_resources.object_map.Contains(handle_id));

        return m_resources.object_map.Erase(handle_id);
    }

    // template <class T>
    // TypeMap<ResourceList>::Iterator GetIterator()
    // {
    //     TypeMap<ResourceList>::Iterator resources_it = m_resources.Find<T>();

    //     if (resources_it != m_resources.End()) {
    //         return resources_it;

    //     TypeMap<ResourceList>::InsertResult insert_result = m_resources.Set<T>(ResourceList { });

    //     return insert_result.first;
    // }

    // TypeMap<ResourceList> m_resources;

    ResourceList m_resources;
};
#endif

} // namespace hyperion::v2

#endif