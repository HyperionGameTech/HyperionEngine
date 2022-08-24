#ifndef HYPERION_V2_COMPONENT_HPP
#define HYPERION_V2_COMPONENT_HPP

#include <core/Containers.hpp>
#include <core/lib/TypeMap.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/Handle.hpp>
#include <Types.hpp>
#include <Constants.hpp>

#include <system/Debug.hpp>
#include <mutex>

namespace hyperion::v2 {

class Engine;

class ComponentSystem
{
    template <class T>
    struct RegisteredComponentSet
    {
        FlatMap<typename Handle<T>::ID, WeakHandle<T>> map;
        std::mutex mutex;
    };

public:
    template <class T>
    bool Attach(Handle<T> &handle)
    {
        using Normalized = NormalizedType<T>;

        if (!handle) {
            return false;
        }

        if (!handle->GetId()) {
            handle->SetId(GetIDCreator<Normalized>().NextID());
        }

        auto &registered_components = GetRegisteredComponents<Normalized>();

        registered_components.mutex.lock();
        registered_components.map[handle->GetId()] = WeakHandle<Normalized>(handle);
        registered_components.mutex.unlock();

        return true;
    }

    template <class T>
    Handle<T> Lookup(const typename Handle<T>::ID &id)
    {
        using Normalized = NormalizedType<T>;

        if (!id) {
            return Handle<T>();
        }

        auto &registered_components = GetRegisteredComponents<Normalized>();

        std::lock_guard guard(registered_components.mutex);
        return registered_components.map[id].Lock();
    }

    /*! \brief Release an ID from use. Called on destructor of EngineComponentBase. */
    template <class T>
    bool Release(const typename Handle<T>::ID &id)
    {
        if (!id) {
            return false;
        }

        GetIDCreator<NormalizedType<T>>().FreeID(id);

        return true;
    }

private:
    struct IDCreator
    {
        TypeID type_id;
        std::atomic<UInt> id_counter { 0u };
        std::atomic_bool has_free_id { false };
        Queue<HandleID> free_ids;
        std::mutex free_id_mutex;

        HandleID NextID()
        {
            if (!has_free_id.load()) {
                return HandleID(type_id, ++id_counter);
            }

            std::lock_guard guard(free_id_mutex);

            auto id = free_ids.Pop();
            has_free_id.store(free_ids.Any());

            return id;
        }

        void FreeID(const HandleID &id)
        {
            std::lock_guard guard(free_id_mutex);

            free_ids.Push(id);
            has_free_id.store(true);
        }
    };

    template <class T>
    IDCreator &GetIDCreator()
    {
        static IDCreator id_creator {
            .type_id = TypeID::ForType<T>()
        };

        return id_creator;
    }

    template <class T>
    RegisteredComponentSet<T> &GetRegisteredComponents()
    {
        static RegisteredComponentSet<T> registered_components { };

        return registered_components;
    }

};

} // namespace hyperion::v2

#endif