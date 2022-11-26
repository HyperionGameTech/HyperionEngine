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


template <class T, SizeType MaxItems = 16384>
class ObjectContainer
{
public:
    struct ObjectBytes
    {
        alignas(T) UByte bytes[sizeof(T)];
        bool has_value;

        UInt16 ref_count;

        ObjectBytes()
            : has_value(false),
              ref_count(0)
        {
            Memory::Set(bytes, 0, sizeof(T));
        }

        ~ObjectBytes()
        {
            if (has_value) {
                Get().~T();
                Memory::Set(bytes, 0, sizeof(T));
                has_value = false;
            }
        }

        template <class ...Args>
        void Construct(Args &&... args)
        {
            AssertThrow(!has_value);

            new (bytes) T(std::forward<Args>(args)...);
            has_value = true;
        }

        void IncRef()
        {
            ++ref_count;
        }

        void DecRef()
        {
            AssertThrow(ref_count != 0);

            if (--ref_count == 0) {
                Get().~T();
                Memory::Set(bytes, 0, sizeof(T));
                has_value = false;
            }
        }

        T &Get()
        {
            AssertThrow(has_value);

            return *reinterpret_cast<T *>(bytes);
        }
    };

    ObjectContainer()
        : m_size(0)
    {
    }

    ObjectContainer(const ObjectContainer &other) = delete;
    ObjectContainer &operator=(const ObjectContainer &other) = delete;

    ~ObjectContainer()
    {
        // for (SizeType index = 0; index < m_size; index++) {
        //     if (m_data[index].has_value) {
        //         m_data[index].Destruct();
        //     }
        // }
    }

    SizeType NextIndex()
    {
        static IDCreator id_creator;

        return SizeType(id_creator.NextID().Value());
    }

    void IncRef(SizeType index)
    {
        AssertThrow(m_data[index].has_value);

        return m_data[index].IncRef();
    }

    void DecRef(SizeType index)
    {
        AssertThrow(m_data[index].has_value);

        return m_data[index].DecRef();
    }

    T &Get(SizeType index)
    {
        // AssertThrow(index < m_size);
        AssertThrow(m_data[index].has_value);

        return m_data[index].Get();
    }
    
    template <class ...Args>
    void ConstructAtIndex(SizeType index, Args &&... args)
    {
        AssertThrow(!m_data[index].has_value);

        return m_data[index].Construct(std::forward<Args>(args)...);
    }

    // void Set(SizeType index, T &&element)
    // {
    //     AssertThrow()
    // }

private:
    HeapArray<ObjectBytes, MaxItems> m_data;
    SizeType m_size;
};

class ComponentSystem
{
    // template <class T>
    // struct RegisteredComponentSet
    // {
    //     FlatMap<typename Handle<T>::ID, WeakHandle<T>> map;
    //     std::mutex mutex;
    // };

public:

    template <class T, SizeType Size = 16384>
    ObjectContainer<T, Size> &GetContainer()
    {
        static ObjectContainer<T, Size> container;

        return container;
    }


    // template <class T>
    // bool Attach(Handle<T> &handle)
    // {
    //     using Normalized = NormalizedType<T>;

    //     if (!handle) {
    //         return false;
    //     }

    //     if (!handle->GetID()) {
    //         handle->SetID(HandleID<T>(GetIDCreator<Normalized>().NextID()));
    //     }

    //     auto &registered_components = GetRegisteredComponents<Normalized>();

    //     std::lock_guard guard(registered_components.mutex);
    //     registered_components.map[handle->GetID()] = WeakHandle<Normalized>(handle);

    //     return true;
    // }

    // template <class T>
    // Handle<T> Lookup(const typename Handle<T>::ID &id)
    // {
    //     // using Normalized = NormalizedType<T>;

    //     // if (!id) {
    //     //     return Handle<T>();
    //     // }

    //     // auto &registered_components = GetRegisteredComponents<Normalized>();

    //     // std::lock_guard guard(registered_components.mutex);
    //     // return registered_components.map[id].Lock();

    //     return Handle<T> handle(id);
    // }

    /*! \brief Release an ID from use. Called on destructor of EngineComponentBase. */
    // template <class T>
    // bool Release(const typename Handle<T>::ID &id)
    // {
    //     if (!id) {
    //         return false;
    //     }

    //     GetIDCreator<NormalizedType<T>>().FreeID(id);

    //     return true;
    // }

private:
    

    // template <class T>
    // RegisteredComponentSet<T> &GetRegisteredComponents()
    // {
    //     static RegisteredComponentSet<T> registered_components { };

    //     return registered_components;
    // }

};

} // namespace hyperion::v2

#endif