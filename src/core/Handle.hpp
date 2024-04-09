#ifndef HYPERION_V2_CORE_HANDLE_HPP
#define HYPERION_V2_CORE_HANDLE_HPP

#include <core/Core.hpp>
#include <core/ID.hpp>
#include <core/ObjectPool.hpp>

namespace hyperion::v2 {

template <class T>
class ObjectContainer;

template <class T>
static inline ObjectContainer<T> &GetContainer()
{
    return GetObjectPool().template GetContainer<T>();
}

template <class T>
struct Handle
{
    using IDType = ID<T>;

    static_assert(has_opaque_handle_defined<T>, "Type does not support handles");
    
    static ObjectContainer<T> *s_container;

    static const Handle empty;

    uint index;

    Handle()
        : index(0)
    {
    }

    explicit Handle(IDType id)
        : index(id.Value())
    {
        if (index != 0) {
            GetContainer<T>().IncRefStrong(index - 1);
        }
    }

    Handle(const Handle &other)
        : index(other.index)
    {
        if (index != 0) {
            GetContainer<T>().IncRefStrong(index - 1);
        }
    }

    Handle &operator=(const Handle &other)
    {
        if (other.index == index) {
            return *this;
        }

        if (index != 0) {
            GetContainer<T>().DecRefStrong(index - 1);
        }

        index = other.index;

        if (index != 0) {
            GetContainer<T>().IncRefStrong(index - 1);
        }

        return *this;
    }

    Handle(Handle &&other) noexcept
        : index(other.index)
    {
        other.index = 0;
    }

    Handle &operator=(Handle &&other) noexcept
    {
        if (other.index == index) {
            return *this;
        }

        if (index != 0) {
            GetContainer<T>().DecRefStrong(index - 1);
        }

        index = other.index;
        other.index = 0;

        return *this;
    }

    ~Handle()
    {
        if (index != 0) {
            GetContainer<T>().DecRefStrong(index - 1);
        }
    }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    T *operator->() const
        { return Get(); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    T &operator*()
        { return *Get(); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    const T &operator*() const
        { return *Get(); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator!() const
        { return !IsValid(); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    explicit operator bool() const
        { return IsValid(); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator==(std::nullptr_t) const
        { return !IsValid(); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator!=(std::nullptr_t) const
        { return IsValid(); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator==(const Handle &other) const
        { return index == other.index; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator!=(const Handle &other) const
        { return index != other.index; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator<(const Handle &other) const
        { return index < other.index; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool IsValid() const
        { return index != 0; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    IDType GetID() const
        { return { uint(index) }; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    T *Get() const
    {
        if (index == 0) {
            return nullptr;
        }

        return &GetContainer<T>().Get(index - 1);
    }
    
    HYP_FORCE_INLINE
    void Reset()
    {
        auto &container = GetContainer<T>();  

        if (index != 0) {
            container.DecRefStrong(index - 1);
        }

        index = 0;
    }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    static Name GetTypeName()
        { return HandleDefinition<T>::GetNameForType(); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    static constexpr const char *GetClassNameString()
        { return HandleDefinition<T>::GetClassNameString(); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(GetTypeName().GetHashCode());
        hc.Add(index);

        return hc;
    }
};

template <class T>
struct WeakHandle
{
    using IDType = ID<T>;

    static_assert(has_opaque_handle_defined<T>, "Type does not support handles");
    
    static ObjectContainer<T> *s_container;

    uint index;

    WeakHandle()
        : index(0)
    {
    }

    WeakHandle(const Handle<T> &other)
        : index(other.index)
    {
        if (index != 0) {
            GetContainer<T>().IncRefWeak(index - 1);
        }
    }

    WeakHandle &operator=(const Handle<T> &other)
    {
        if (index != 0) {
            GetContainer<T>().DecRefWeak(index - 1);
        }

        index = other.index;

        if (index != 0) {
            GetContainer<T>().IncRefWeak(index - 1);
        }

        return *this;
    }

    WeakHandle(const WeakHandle &other)
        : index(other.index)
    {
        if (index != 0) {
            GetContainer<T>().IncRefWeak(index - 1);
        }
    }

    WeakHandle &operator=(const WeakHandle &other)
    {
        if (index != 0) {
            GetContainer<T>().DecRefWeak(index - 1);
        }

        index = other.index;

        if (index != 0) {
            GetContainer<T>().IncRefWeak(index - 1);
        }

        return *this;
    }

    WeakHandle(WeakHandle &&other) noexcept
        : index(other.index)
    {
        other.index = 0;
    }

    WeakHandle &operator=(WeakHandle &&other) noexcept
    {
        if (index != 0) {
            GetContainer<T>().DecRefWeak(index - 1);
        }

        index = other.index;
        other.index = 0;

        return *this;
    }

    ~WeakHandle()
    {
        if (index != 0) {
            GetContainer<T>().DecRefWeak(index - 1);
        }
    }

    [[nodiscard]]
    Handle<T> Lock() const
    {
        if (index == 0) {
            return Handle<T>();
        }

        return GetContainer<T>().GetObjectBytes(index - 1).GetRefCountStrong() != 0
            ? Handle<T>(IDType { index })
            : Handle<T>();
    }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator!() const
        { return !IsValid(); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    explicit operator bool() const
        { return IsValid(); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator==(std::nullptr_t) const
        { return !IsValid(); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator!=(std::nullptr_t) const
        { return IsValid(); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator==(const WeakHandle &other) const
        { return index == other.index; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator!=(const WeakHandle &other) const
        { return index == other.index; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator<(const WeakHandle &other) const
        { return index < other.index; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator==(const Handle<T> &other) const
        { return index == other.index; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator!=(const Handle<T> &other) const
        { return index == other.index; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator<(const Handle<T> &other) const
        { return index < other.index; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool IsValid() const
        { return index != 0; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    IDType GetID() const
        { return { uint(index) }; }

    void Reset()
    {
        auto &container = GetContainer<T>();  

        if (index != 0) {
            container.DecRefWeak(index - 1);
        }

        index = 0;
    }
    
    [[nodiscard]]
    HYP_FORCE_INLINE static Name GetTypeName()
        { return HandleDefinition<T>::GetNameForType(); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE static constexpr const char *GetClassNameString()
        { return HandleDefinition<T>::GetClassNameString(); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(String(HandleDefinition<T>::class_name).GetHashCode());
        hc.Add(index);

        return hc;
    }
};

template <class T>
const Handle<T> Handle<T>::empty = { };

template <class T>
ObjectContainer<T> *Handle<T>::s_container = nullptr;

template <class T>
ObjectContainer<T> *WeakHandle<T>::s_container = nullptr;

template <class T, class Engine, class ...Args>
static Handle<T> CreateObjectIntern(Engine *engine, Args &&... args)
{
    // Struct to set the container for Handle<T> on first call
    static struct Initializer
    {
        Initializer()
        {
            Handle<T>::s_container = &GetContainer<T>();
            WeakHandle<T>::s_container = &GetContainer<T>();
        }
    } initializer;

    return engine->template CreateObject<T>(std::forward<Args>(args)...);
}

/**
 * \brief Creates a new object of type T. The object is not initialized until InitObject() is called with the object as a parameter.
 */
template <class T, class ...Args>
static HYP_FORCE_INLINE Handle<T> CreateObject(Args &&... args)
{
    return CreateObjectIntern<T>(GetEngine(), std::forward<Args>(args)...);
}

} // namespace hyperion::v2

#endif