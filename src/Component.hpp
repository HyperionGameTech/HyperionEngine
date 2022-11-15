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

        if (!handle->GetID()) {
            handle->SetID(HandleID<T>(GetIDCreator<Normalized>().NextID()));
        }

        auto &registered_components = GetRegisteredComponents<Normalized>();

        std::lock_guard guard(registered_components.mutex);
        registered_components.map[handle->GetID()] = WeakHandle<Normalized>(handle);

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
        std::mutex free_id_mutex;
        Queue<IDBase> free_ids;

        IDBase NextID()
        {
            std::lock_guard guard(free_id_mutex);

            if (free_ids.Empty()) {
                return IDBase { ++id_counter };
            }

            return free_ids.Pop();
        }

        void FreeID(const IDBase &id)
        {
            std::lock_guard guard(free_id_mutex);

            free_ids.Push(id);
        }
    };

    template <class T>
    IDCreator &GetIDCreator()
    {
        static IDCreator id_creator { TypeID::ForType<T>() };

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