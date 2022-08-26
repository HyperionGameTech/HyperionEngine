#ifndef HYPERION_V2_CORE_CONTAINERS_H
#define HYPERION_V2_CORE_CONTAINERS_H

#include <core/lib/FlatMap.hpp>
#include <core/lib/TypeMap.hpp>
#include <core/lib/StaticMap.hpp>
#include <core/lib/FlatSet.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/FixedArray.hpp>
#include <core/lib/HeapArray.hpp>
#include <core/lib/FixedString.hpp>
#include <core/lib/String.hpp>
#include <core/lib/Queue.hpp>
#include <core/lib/Proc.hpp>
#include <math/MathUtil.hpp>
#include <Types.hpp>

#include <vector>
#include <memory>
#include <algorithm>
#include <queue>
#include <unordered_map>
#include <functional>
#include <tuple>
#include <atomic>
#include <mutex>
#include <thread>

namespace hyperion::v2 {

class Engine;

template <class Group>
struct CallbackRef
{
    UInt id;
    Group *group;
    typename Group::ArgsTuple bound_args;

    CallbackRef()
        : id(0),
          group(nullptr),
          bound_args{}
    {
    }

    CallbackRef(UInt id, Group *group)
        : id(id),
          group(group),
          bound_args{}
    {
    }

    CallbackRef(const CallbackRef &other) = delete;
    CallbackRef &operator=(const CallbackRef &other) = delete;

    CallbackRef(CallbackRef &&other) noexcept
        : id(other.id),
          group(other.group),
          bound_args(std::move(other.bound_args))
    {
        other.id = 0;
        other.group = nullptr;
    }
    
    CallbackRef &operator=(CallbackRef &&other) noexcept
    {
        if (&other == this) {
            return *this;
        }

        if (Valid()) {
            Remove();
        }

        id = other.id;
        group = other.group;
        bound_args = std::move(other.bound_args);

        other.id = 0;
        other.group = nullptr;

        return *this;
    }

    ~CallbackRef() = default;

    bool Valid() const { return id != 0 && group != nullptr; }

    bool Remove()
    {
        const bool result = Valid() && group->Remove(id);

        id = 0;
        group = nullptr;
        bound_args = {};

        return result;
    }

    CallbackRef &Bind(typename Group::ArgsTuple &&args)
    {
        bound_args = std::move(args);

        return *this;
    }

    bool Trigger()
    {
        if (!Valid()) {
            return false;
        }

        /* expand bound_args tuple into Trigger() function args */
        return std::apply([this]<class ...Args> (Args ... args) -> bool {
            return group->Trigger(id, std::forward<Args>(args)...);
        }, bound_args);
    }

    bool TriggerRemove()
    {
        if (!Valid()) {
            return false;
        }

        const auto id_before = id;

        id = 0; // invalidate

        /* expand bound_args tuple into Trigger() function args */
        const bool result = std::apply([&]<class ...Args> (Args ... args) -> bool {
            return group->Trigger(id_before, std::forward<Args>(args)...);
        }, bound_args);

        group->Remove(id_before);

        group = nullptr;
        bound_args = {};

        return result;
    }
};

// relly don't like this interface anymore.
// will refactor. the only reason it is still needed
// is so that stuff that uses certain descriptor sets gets
// created only after the descriptor sets are created.

template <class Enum, class ...Args>
class Callbacks
{
    struct CallbackInstance
    {
        using Function = std::function<void(Args...)>;

        using Id = UInt;
        static constexpr Id empty_id = 0;

        Id id{empty_id};
        Function fn;
        UInt num_calls{0};

        bool Valid() const        { return id != empty_id; }
        void Reset()              { id = empty_id; }

        UInt NumCalls() const { return num_calls; }
        
        template <class ...OtherArgs>
        void Call(OtherArgs &&... args)
        {
            AssertThrow(Valid());

            fn(std::forward<OtherArgs>(args)...);

            ++num_calls;
        }
    };

public:
    struct CallbackGroup {
        using ArgsTuple = std::tuple<Args...>;

        std::vector<CallbackInstance> once_callbacks;
        std::vector<CallbackInstance> on_callbacks;

        struct TriggerState {
            bool triggered{false};
            ArgsTuple args;
        } trigger_state = {};

        ~CallbackGroup()
        {
            std::lock_guard guard(m_mtx);

            for (auto &once_callback : once_callbacks) {
                once_callback.Reset();
            }

            for (auto &on_callback : on_callbacks) {
                on_callback.Reset();
            }
        }

        CallbackInstance *GetCallbackInstance(typename CallbackInstance::Id id)
        {
            std::lock_guard guard(m_mtx);

            auto once_it = Find(id, once_callbacks);

            if (once_it != once_callbacks.end()) {
                return &(*once_it);
            }

            auto on_it = Find(id, on_callbacks);

            if (on_it != on_callbacks.end()) {
                return &(*on_it);
            }

            return nullptr;
        }

        bool CheckValid(typename CallbackInstance::Id id)
        {
            if (CallbackInstance *instance = GetCallbackInstance(id)) {
                return instance->Valid();
            }

            return false;
        }

        bool Remove(typename CallbackInstance::Id id)
        {
            std::lock_guard guard(m_mtx);

            auto once_it = Find(id, once_callbacks);

            if (once_it != once_callbacks.end()) {
                once_it->Reset(); // invalidate; remove later

                return true;
            }

            auto on_it = Find(id, on_callbacks);

            if (on_it != on_callbacks.end()) {
                on_it->Reset(); // invalidate; remove later

                return true;
            }

            return false;
        }

        /*! \brief Trigger a specific callback, removing it if it is a `once` callback. */
        bool Trigger(UInt id, Args &&... args)
        {
            std::lock_guard guard(m_mtx);

            auto once_it = Find(id, once_callbacks);

            if (once_it != once_callbacks.end()) {
                if (!once_it->Valid()) {
                    return false;
                }

                AssertThrowMsg(once_it->NumCalls() == 0, "'once' callback has already been called!");

                once_it->Call(std::forward<Args>(args)...);
                once_it->Reset();

                return true;
            }

            auto on_it = Find(id, on_callbacks);

            if (on_it != on_callbacks.end()) {
                if (!on_it->Valid()) {
                    return false;
                }

                on_it->Call(std::forward<Args>(args)...);

                return true;
            }

            return false;
        }

        void ClearInvalidatedCallbacks()
        {
            std::lock_guard guard(m_mtx);

            once_callbacks.erase(
                std::remove_if(once_callbacks.begin(), once_callbacks.end(), [](const auto &item) {
                    return !item.Valid();
                }),
                once_callbacks.end()
            );

            on_callbacks.erase(
                std::remove_if(on_callbacks.begin(), on_callbacks.end(), [](const auto &item) {
                    return !item.Valid();
                }),
                on_callbacks.end()
            );
        }

        void AddOnceCallback(CallbackInstance &&instance)
        {
            std::lock_guard guard(m_mtx);

            once_callbacks.push_back(std::move(instance));
        }

        void AddOnCallback(CallbackInstance &&instance)
        {
            std::lock_guard guard(m_mtx);

            on_callbacks.push_back(std::move(instance));
        }

    private:
        auto Find(typename CallbackInstance::Id id, std::vector<CallbackInstance> &callbacks) -> typename std::vector<CallbackInstance>::iterator
        {
            return std::find_if(
                callbacks.begin(),
                callbacks.end(),
                [id](const auto &other) {
                    return other.id == id;
                }
            );
        }

        std::recursive_mutex m_mtx; // eugh
    };

    using ArgsTuple = std::tuple<Args...>;

    Callbacks() = default;
    Callbacks(const Callbacks &other) = delete;
    Callbacks &operator=(const Callbacks &other) = delete;
    ~Callbacks() = default;

    CallbackRef<CallbackGroup>
    Once(Enum key, typename CallbackInstance::Function &&function)
    {
        std::lock_guard guard(rw_callbacks_mutex);

        auto &holder = m_holders[key];

        const auto id = ++m_id_counter;
        
        CallbackInstance callback_instance {
            .id = id,
            .fn = std::move(function)
        };

        if (holder.trigger_state.triggered) {
            callback_instance.Call(std::get<Args>(holder.trigger_state.args)...);

            return CallbackRef<CallbackGroup>();
        }

        holder.AddOnceCallback(std::move(callback_instance));

        return CallbackRef<CallbackGroup>(id, &holder);
    }

    CallbackRef<CallbackGroup>
    On(Enum key, typename CallbackInstance::Function &&function)
    {
        std::lock_guard guard(rw_callbacks_mutex);

        auto &holder = m_holders[key];

        const auto id = ++m_id_counter;
        
        CallbackInstance callback_instance{
            .id = id,
            .fn = std::move(function)
        };

        if (holder.trigger_state.triggered) {
            callback_instance.Call(std::get<Args>(holder.trigger_state.args)...);
        }

        holder.AddOnceCallback(std::move(callback_instance));

        return CallbackRef<CallbackGroup>(id, &holder);
    }

    void Trigger(Enum key, Args &&... args)
    {
        TriggerCallbacks(false, key, std::move(args)...);
    }

    /* \brief Trigger all `once` and `on` events for the given key,
     * keeping the holder of all callbacks in that key in the triggered state,
     * so that any newly added callbacks will be executed immediately.
     */
    void TriggerPersisted(Enum key, Args &&... args)
    {
        TriggerCallbacks(true, key, std::move(args)...);
    }


private:
    UInt m_id_counter { 0 };
    std::unordered_map<Enum, CallbackGroup> m_holders;

    std::recursive_mutex rw_callbacks_mutex; // Not ideal having this as a recursive_mutex but at the moment there is no other option as objects can be
                                             // created nested within other Init() objects. 
    
    void TriggerCallbacks(bool persist, Enum key, Args &&... args)
    {
        std::lock_guard guard(rw_callbacks_mutex);

        auto &holder = m_holders[key];

        auto &once_callbacks = holder.once_callbacks;
        auto &on_callbacks = holder.on_callbacks;

        const bool previous_triggered_state = holder.trigger_state.triggered;

        holder.trigger_state.triggered = true;
        holder.trigger_state.args = std::tie(args...);

        for (CallbackInstance &callback_instance : once_callbacks) {
            if (callback_instance.Valid()) {
                callback_instance.Call(std::forward<Args>(args)...);
                callback_instance.Reset();
            }
        }

        for (CallbackInstance &callback_instance : on_callbacks) {
            if (callback_instance.Valid()) {
                callback_instance.Call(std::forward<Args>(args)...);
            }
        }

        holder.trigger_state.triggered = previous_triggered_state || persist;

        holder.ClearInvalidatedCallbacks();
    }
};

template <class CallbacksClass>
class CallbackTrackable
{
public:
    using Callbacks = CallbacksClass;
    using CallbackRef = CallbackRef<typename Callbacks::CallbackGroup>;

    CallbackTrackable() = default;
    CallbackTrackable(const CallbackTrackable &other) = delete;
    CallbackTrackable &operator=(const CallbackTrackable &other) = delete;
    ~CallbackTrackable() = default;

protected:

    /*! \brief Triggers the destroy callback (if present) and
     * removes all existing callbacks from the callback holder
     */
    void Teardown()
    {
        if (m_init_callback.Valid()) {
            m_init_callback.Remove();
        }

        for (auto &callback : m_owned_callbacks) {
            if (callback.Valid()) {
                callback.Remove();
            }
        }

        if (m_teardown) {
            m_teardown();
        }
    }

    /*! \brief Add the given callback reference to this object's internal
     * list of callback refs it is responsible for. On destructor call, all remaining
     * callback refs will be removed, so as to not have a dangling callback.
     * @param callback_ref The callback reference, obtained via Once() or On()
     */
    void OnInit(CallbackRef &&callback_ref)
    {
        if (m_init_callback.Valid()) {
            DebugLog(LogType::Warn, "OnInit callback overwritten.\n");

            AssertThrow(m_init_callback.Remove());
        }

        m_init_callback = std::move(callback_ref);
    }

    void OnTeardown(Proc<void> &&teardown)
    {
        m_teardown = std::move(teardown);
    }

    /*! \brief Add an owned callback to this object. Note, this does not create a callback,
     * but is instead used to track ownership. This is necessary if you have a callback that
     * needs to perform some operation on this object.
     * @param callback_ref The callback reference, obtained via Once() or On()
     */
    void AttachCallback(CallbackRef &&callback_ref)
    {
        m_owned_callbacks.push_back(std::move(callback_ref));
    }

private:
    CallbackRef m_init_callback;
    Proc<void> m_teardown;
    std::vector<CallbackRef> m_owned_callbacks;
};

enum class EngineCallback
{
    NONE,

    CREATE_ANY,
    DESTROY_ANY,

    CREATE_SCENES,
    DESTROY_SCENES,

    CREATE_ENVIRONMENTS,
    DESTROY_ENVIRONMENTS,

    CREATE_SPATIALS,
    DESTROY_SPATIALS,

    CREATE_MESHES,
    DESTROY_MESHES,

    CREATE_TEXTURES,
    DESTROY_TEXTURES,

    CREATE_MATERIALS,
    DESTROY_MATERIALS,

    CREATE_LIGHTS,
    DESTROY_LIGHTS,

    CREATE_SKELETONS,
    DESTROY_SKELETONS,

    CREATE_SHADERS,
    DESTROY_SHADERS,

    CREATE_RENDER_PASSES,
    DESTROY_RENDER_PASSES,

    CREATE_FRAMEBUFFERS,
    DESTROY_FRAMEBUFFERS,

    CREATE_DESCRIPTOR_SETS,
    DESTROY_DESCRIPTOR_SETS,

    CREATE_GRAPHICS_PIPELINES,
    DESTROY_GRAPHICS_PIPELINES,

    CREATE_COMPUTE_PIPELINES,
    DESTROY_COMPUTE_PIPELINES,

    CREATE_RAYTRACING_PIPELINES,
    DESTROY_RAYTRACING_PIPELINES,
    
    CREATE_ACCELERATION_STRUCTURES,
    DESTROY_ACCELERATION_STRUCTURES,

    CREATE_VOXELIZER,
    DESTROY_VOXELIZER
};

using EngineCallbacks = Callbacks<EngineCallback, Engine *>;

/* v1 callback utility used by octree */
template <class CallbacksClass>
struct ComponentEvents
{
    struct CallbackGroup
    {
        using CallbackFunction = typename CallbacksClass::CallbackFunction;

        std::vector<CallbackFunction> callbacks;

        template <class ...Args>
        void operator()(Args &&... args)
        {
            for (auto &callback : callbacks) {
                callback(args...);
            }
        }

        CallbackGroup &operator+=(const CallbackFunction &callback)
        {
            callbacks.push_back(callback);

            return *this;
        }

        CallbackGroup &operator+=(CallbackFunction &&callback)
        {
            callbacks.push_back(std::move(callback));

            return *this;
        }

        inline void Clear() { callbacks.clear(); }
    };

    CallbackGroup on_init,
                  on_deinit,
                  on_update;
};

/* New ObjectHolder class that does not call Destroy,
 * as new component types will not have Destroy() rather using
 * a teardown method which is defined in CallbackTrackable.
 */
template <class T>
class ObjectVector
{
    static constexpr bool use_free_slots = true;

public:
    std::vector<T *> objects;
    std::queue<SizeType> free_slots;

    ObjectVector() = default;
    ObjectVector(const ObjectVector &other) = delete;
    ObjectVector &operator=(const ObjectVector &other) = delete;
    ~ObjectVector()
    {
        RemoveAll();
    }

    constexpr SizeType Size() const
        { return objects.size(); }

    auto &Objects() { return objects; }
    const auto &Objects() const { return objects; }

    constexpr T *Get(typename T::ID id)
    {
        return MathUtil::InRange(id.value, { 1, objects.size() + 1 })
            ? objects[id.value - 1]
            : nullptr;
    }

    constexpr const T *Get(typename T::ID id) const
        { return const_cast<ObjectVector<T> *>(this)->Get(id); }

    constexpr T *operator[](typename T::ID id) { return Get(id); }
    constexpr const T *operator[](typename T::ID id) const { return Get(id); }

    template <class LambdaFunction>
    constexpr const T *Find(LambdaFunction lambda) const
    {
        const auto it = std::find_if(objects.begin(), objects.end(), lambda);

        if (it != objects.end()) {
            return *it;
        }

        return nullptr;
    }
    
    T *Add(T *object)
    {
        AssertThrow(object != nullptr);
        AssertThrowMsg(object->GetId() == T::empty_id, "Adding object that already has id set");

        typename T::ID next_id;

        if constexpr (use_free_slots) {
            if (!free_slots.empty()) {
                auto front = free_slots.front();

                next_id = typename T::ID(TypeID::ForType<NormalizedType<T>>(), typename T::ID::ValueType(front + 1));
                object->SetId(next_id);

                objects[front] = object;
                free_slots.pop();

                return object;
            }
        }

        next_id = typename T::ID(TypeID::ForType<NormalizedType<T>>(), typename T::ID::ValueType(objects.size() + 1));
        object->SetId(next_id);

        objects.push_back(object);

        return object;
    }
    
    void Remove(typename T::ID id)
    {
        AssertThrowMsg(id.value != 0, "Value was not initialized!\n");
        
        DisposeObjectAtIndex(id.value - 1);

        if (id.value == objects.size()) {
            objects.pop_back();
        } else {
            /* Cannot simply erase from vector, as that would invalidate existing IDs */
            if constexpr (use_free_slots) {
                free_slots.push(id.value - 1);
            }
        }
    }

    void Destroy(typename T::ID id)
    {
        AssertThrowMsg(id.value != 0, "Value was not initialized!\n");
        DisposeObjectAtIndex(id.value - 1);
    }
    
    void RemoveAll()
    {
        for (SizeType i = objects.size(); i > 0; --i) {
            DisposeObjectAtIndex(i - 1);
        }

        objects.clear();
        free_slots = {};
    }


    void DisposeObjectAtIndex(SizeType index)
    {
        AssertThrow(index < objects.size());

        if (!objects[index]) {
            return;
        }

        // objects[index]->OnDisposed();

        delete objects[index];
        objects[index] = nullptr;
    }

};

template <class T, class ...Args>
class RefCounter
{
    using ArgsTuple = std::tuple<Args...>;

    struct RefCount
    {
        RefCounter *ref_manager = nullptr;
        std::atomic<UInt> count { 0 };
    };

    FlatMap<typename T::ID, RefCount *> m_ref_count_holder;

    ObjectVector<T> m_holder;
    ArgsTuple m_init_args { };

public:
    class RefWrapper
    {
    protected:
        RefWrapper(T *ptr, RefCount *ref_count)
            : ptr(ptr),
              m_ref_count(ref_count)
        {
        }

        RefWrapper(RefWrapper &&other) noexcept
            : ptr(other.ptr),
              m_ref_count(other.m_ref_count)
        {
            other.ptr         = nullptr;
            other.m_ref_count = nullptr;
        }

    public:
        operator T const * () const    { return ptr; }
        explicit operator bool() const { return Valid(); }

        T *operator->()                { return ptr; }
        const T *operator->() const    { return ptr; }

        T &operator*()                 { return *ptr; }
        const T &operator*() const     { return *ptr; }
        
        bool operator==(std::nullptr_t) const
            { return ptr == nullptr; }

        bool operator!=(std::nullptr_t) const
            { return ptr != nullptr; }

        bool operator==(const RefWrapper &other) const
            { return ptr == other.ptr; }

        bool operator!=(const RefWrapper &other) const
            { return !operator==(other); }

        bool operator<(const RefWrapper &other) const
            { return intptr_t(static_cast<const void *>(ptr)) < intptr_t(static_cast<const void *>(other.ptr)); }
        
        /*! \brief _If_ ptr has not had Init() performed yet (checked via IsInit()), call it, using
         * the pre-bound arguments from the reference counter this wrapper was acquired from.
         * Throws an assertion error if state is invalid.
         */
        void Init()
        {
            AssertState();
            
            if (!ptr->IsInitCalled()) {
                /* Call ptr->Init() with the ref counter's pre-bound args */
                std::apply(
                    &T::Init,
                    std::tuple_cat(
                        std::make_tuple(ptr),
                        m_ref_count->ref_manager->m_init_args
                    )
                );
            }
        }

        /*! \brief _If_ ptr has not had Init() performed yet (checked via IsInit()), call it.
         * Throws an assertion error if state is invalid.
         * @param args The arguments to be forwarded to Init()
         */
        template <class ...InitArgs>
        void Init(InitArgs &&... args)
        {
            AssertState();
            
            if (!ptr->IsInitCalled()) {
                /* Call ptr->Init() with the provided args */
                ptr->Init(std::forward<InitArgs>(args)...);
            }
        }

        bool Valid() const
            { return ptr != nullptr && m_ref_count != nullptr; }

        T *ptr;
        RefCount *m_ref_count = nullptr;

    protected:
        void AssertState()
        {
            AssertThrowMsg(ptr != nullptr,           "underlying pointer was null");
            AssertThrowMsg(m_ref_count != nullptr,   "ref counter not set");
        }
    };

    // A simple ref-counted handle to a resource, deriving EngineComponent
    class Ref : public RefWrapper
    {
    public:
        Ref()
            : Ref(nullptr)
        {
        }

        Ref(std::nullptr_t)
            : Ref(nullptr, nullptr)
        {
        }

        explicit Ref(T *_ptr, RefCount *_ref_count)
            : RefWrapper(_ptr, _ref_count)
        {
            if (RefWrapper::m_ref_count != nullptr) {
                ++RefWrapper::m_ref_count->count;
            }
        }

        Ref(const Ref &other) = delete;
        Ref &operator=(const Ref &other) = delete;

        Ref(Ref &&other) noexcept : RefWrapper(std::move(other)) {}

        Ref &operator=(Ref &&other) noexcept
        {
            if (RefWrapper::operator!=(other)) {
                if (RefWrapper::Valid()) {
                    Release();
                }

                RefWrapper::ptr = other.ptr;
                RefWrapper::m_ref_count = other.m_ref_count;
            }

            other.ptr = nullptr;
            other.m_ref_count = nullptr;

            return *this;
        }

        ~Ref()
        {
            if (RefWrapper::m_ref_count != nullptr) {
                Release();
            }
        }

        /*template <class Other>
        auto Cast() const
        {
            static_assert(std::is_convertible_v<const T *, const Other *>, "T must be convertible to Other");
            
            RefWrapper::AssertState();

            return RefCounter<Other, CallbacksClass>::Ref(static_cast<const Other *>(RefWrapper::ptr), RefWrapper::m_ref_count);
        }*/

        /*! \brief Acquire a new reference from this one. Increments the reference counter. */
        auto IncRef()
        {
            RefWrapper::AssertState();

            return Ref(RefWrapper::ptr, RefWrapper::m_ref_count);
        }

        SizeType GetRefCount() const
        {
            if (!RefWrapper::Valid()) {
                return 0;
            }

            return RefWrapper::m_ref_count->count.load();
        }

        HYP_FORCE_INLINE void Reset()
        {
            if (RefWrapper::Valid()) {
                Release();
            }
        }

    private:
        void Release()
        {
            RefWrapper::AssertState();

            AssertThrowMsg(RefWrapper::m_ref_count->count != 0, "Cannot decrement refcount when already at zero");
            
            if (!--RefWrapper::m_ref_count->count) {
                RefWrapper::m_ref_count->ref_manager->Release(RefWrapper::ptr);
            }

            RefWrapper::ptr = nullptr;
            RefWrapper::m_ref_count = nullptr;
        }
    };

    class WeakRef : public RefWrapper
    {
    public:
        WeakRef() : WeakRef(nullptr) {}
        WeakRef(std::nullptr_t) : WeakRef(nullptr, nullptr) {}

        WeakRef(const Ref &strong_ref)
            : RefWrapper(strong_ref.ptr, strong_ref.m_ref_count)
        {
        }

        WeakRef(T *ptr, RefCount *ref_count)
            : RefWrapper(ptr, ref_count)
        {

        }

        WeakRef(const WeakRef &other) = delete;
        WeakRef &operator=(const WeakRef &other) = delete;

        WeakRef(WeakRef &&other) noexcept : RefWrapper(std::move(other)) {}
        WeakRef &operator=(WeakRef &&other) noexcept
        {
            if (std::addressof(other) == this || *this == other) {
                return *this;
            }

            RefWrapper::ptr = other.ptr;
            RefWrapper::m_ref_count = other.m_ref_count;

            RefWrapper::other.ptr = nullptr;
            RefWrapper::other.m_ref_count = nullptr;

            return *this;
        }

        ~WeakRef() = default;

        bool Expired() const
        {
            return RefWrapper::m_ref_count == nullptr
                || RefWrapper::m_ref_count->count.load() == 0;
        }

        /*! \brief Acquire a new reference from this one. Increments the reference counter. */
        auto IncRef()
        {
            RefWrapper::AssertState();

            return Ref(RefWrapper::ptr, RefWrapper::m_ref_count);
        }

        size_t GetRefCount() const
        {
            if (!RefWrapper::Valid()) {
                return 0;
            }

            return RefWrapper::m_ref_count->count.load();
        }
    };

    RefCounter()
    {
    }

    RefCounter(ArgsTuple &&bind_init_args)
        : m_init_args(std::move(bind_init_args))
    {
    }

    ~RefCounter()
    {
        for (auto &it : m_ref_count_holder) {
            AssertThrow(it.second != nullptr);
            AssertThrowMsg(it.second->count.load() <= 1, "Ref<%s> with id #%u still in use", typeid(T).name(), it.first.value);
        }
    }
    
    [[nodiscard]] Ref Add(T *object)
    {
        if (object == nullptr) {
            return nullptr;
        }

        std::lock_guard guard(m_mutex);

        T *ptr = m_holder.Add(object);

        auto it = m_ref_count_holder.Find(ptr->GetId());
        AssertThrowMsg(it == m_ref_count_holder.End(), "ptr with id %u already exists!", ptr->GetId().value);

        auto insert_result = m_ref_count_holder.Insert(
            ptr->GetId(),
            new RefCount {
                .ref_manager = this,
                .count = 0
            }
        );

        AssertThrow(insert_result.second);

        auto &iterator = insert_result.first;

        return Ref(ptr, iterator->second);
    }

    /*! \brief Look up an object by ID, returning a ref counted ptr to it.
        Note, this method is expensive as it locks a mutex. Use sparingly. */
    [[nodiscard]] Ref Lookup(typename T::ID id)
    {
        std::lock_guard guard(m_mutex);

        T *ptr = m_holder.Get(id);

        if (ptr == nullptr) {
            return nullptr;
        }

        auto it = m_ref_count_holder.Find(id);

        if (it == m_ref_count_holder.End()) {
            return nullptr;
        }

        return Ref(ptr, it->second);
    }

    auto &Objects() { return m_holder.objects; }
    const auto &Objects() const { return m_holder.objects; }

private:
    friend class Ref;

    void Release(T *ptr)
    {
        AssertThrow(ptr != nullptr);
        const auto id = ptr->GetId();

        DebugLog(
            LogType::Debug,
            "Removing %u from ref count holder map\n",
            id.value
        );

        std::lock_guard guard(m_mutex);

        m_holder.Remove(id);

        auto it = m_ref_count_holder.Find(id);
        AssertThrowMsg(it != m_ref_count_holder.End(), "%u not found in ref count holder map!", id.value);

        delete it->second;

        m_ref_count_holder.Erase(id);
    }

    std::mutex m_mutex;
};

template <class T>
using Ref = typename RefCounter<T, Engine *>::Ref;

template <class T>
using WeakRef = typename RefCounter<T, Engine *>::WeakRef;

template <class T>
WeakRef<T> MakeWeakRef(const Ref<T> &strong_ref)
{
    return WeakRef<T>(strong_ref);
}

/*template <class To, class From>
Ref<To> CastRef(Ref<From> &&ref)
{
    static_assert(std::is_convertible_v<From *, To *>, "From* must be convertible to To*");

    return Ref<To>(static_cast<To *>(ref.ptr), static_cast<RefCounter<To, EngineCallbacks> *>(ref.m_ref_count));
}*/

} // namespace hyperion::v2

#endif
