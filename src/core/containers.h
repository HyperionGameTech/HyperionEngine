#ifndef HYPERION_V2_CORE_CONTAINERS_H
#define HYPERION_V2_CORE_CONTAINERS_H

#include <math/math_util.h>

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

template <class T>
struct ObjectIdHolder {
    std::vector<typename T::ID> ids;

    inline constexpr size_t Size() const
        { return ids.size(); }
    
    void Add(typename T::ID id)
    {
        ids.push_back(id);
    }

    void Remove(typename T::ID id)
    {
        const auto it = std::find(ids.begin(), ids.end(), id);

        if (it != ids.end()) {
            ids.erase(it);
        }
    }

    bool Has(typename T::ID id) const
    {
        return std::any_of(ids.begin(), ids.end(), [id](auto &it) { return it == id; });
    }
};

template <class Group>
struct CallbackRef {
    uint32_t id;
    Group *group;
    typename Group::ArgsTuple bound_args;

    CallbackRef()
        : id(0),
          group(nullptr),
          bound_args{}
    {
    }

    CallbackRef(uint32_t id, Group *group)
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

    auto *GetCallbackInstance() const { return id != 0 && group != nullptr ? group->GetCallbackInstance(id) : nullptr; }
    bool Valid() const                { return id != 0 && group != nullptr && group->CheckValid(id); }

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

template <class Enum, class ...Args>
class Callbacks {
    struct CallbackInstance {
        using Function = std::function<void(Args...)>;

        using Id = uint32_t;
        static constexpr Id empty_id = 0;

        Id id{empty_id};
        Function fn;
        uint32_t num_calls{0};

        bool Valid() const        { return id != empty_id; }
        void Reset()              { id = empty_id; }

        uint32_t NumCalls() const { return num_calls; }
        
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

        CallbackInstance *GetCallbackInstance(typename CallbackInstance::Id id)
        {
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
        bool Trigger(uint32_t id, Args &&... args)
        {
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
        
        CallbackInstance callback_instance{
            .id = id,
            .fn = std::move(function)
        };

        if (holder.trigger_state.triggered) {
            callback_instance.Call(std::get<Args>(holder.trigger_state.args)...);

            return CallbackRef<CallbackGroup>();
        }

        holder.once_callbacks.push_back(std::move(callback_instance));

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

        holder.on_callbacks.push_back(std::move(callback_instance));

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
    uint32_t m_id_counter{0};
    std::unordered_map<Enum, CallbackGroup> m_holders;

    std::recursive_mutex rw_callbacks_mutex; // Not ideal having this as a recursive_mutex but at the moment there is no other option as objects can be
                                             // created nested within other Init() objects. 
    
    void TriggerCallbacks(bool persist, Enum key, Args &&... args)
    {
        std::lock_guard guard(rw_callbacks_mutex);

        auto &holder = m_holders[key];
        
        auto &once_callbacks = holder.once_callbacks;
        auto &on_callbacks   = holder.on_callbacks;

        const bool previous_triggered_state = holder.trigger_state.triggered;

        holder.trigger_state.triggered = true;
        holder.trigger_state.args      = std::tie(args...);

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
class CallbackTrackable {
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

        if (m_destroy_callback.Valid()) {
            m_destroy_callback.TriggerRemove();
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

    /*! \brief Set the action to be triggered on teardown
     * @param callback_ref The callback reference, obtained via Once() or On()
     * @param bind_args Arguments to bind to the call
     */
    template <class ...Args>
    void OnTeardown(CallbackRef &&callback_ref, Args &&... bind_args)
    {
        if (m_destroy_callback.Valid()) {
            DebugLog(LogType::Warn, "OnTeardown callback overwritten! This may be unintentional and ideally should not happen.\n");

#if HYP_DEBUG_MODE
            HYP_BREAKPOINT;
#endif

            AssertThrow(m_destroy_callback.Remove());
        }

        callback_ref.Bind(std::tie(std::forward<Args>(bind_args)...));

        m_destroy_callback = std::move(callback_ref);
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
    CallbackRef              m_init_callback;
    CallbackRef              m_destroy_callback;
    std::vector<CallbackRef> m_owned_callbacks;
};

enum class EngineCallback {
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

enum class AssetCallback {
    NONE,

    ON_LOAD_START,
    ON_LOAD_FAIL,
    ON_LOAD,

    ON_UNLOAD
};

template <class Result, class T>
using AssetCallbacks = Callbacks<AssetCallback, Result, T *>;

/* v1 callback utility used by octree */
template <class CallbacksClass>
struct ComponentEvents {
    struct CallbackGroup {
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



/* Map from ObjectType::ID to another resource */
template <class ObjectType, class ValueType>
class ObjectMap {
public:
    ObjectMap() = default;
    ObjectMap(const ObjectMap &other) = delete;
    ObjectMap &operator=(const ObjectMap &other) = delete;
    ObjectMap(ObjectMap &&other) noexcept
        : m_map(std::move(other.m_map))
    {}
    ~ObjectMap() = default;

    ObjectMap &operator=(ObjectMap &&other) noexcept
    {
        m_map = std::move(other.m_map);

        return *this;
    }

    bool Has(typename ObjectType::ID id) const
    {
        return m_map.find(id.value) != m_map.end();
    }

    ValueType &Get(typename ObjectType::ID id)
        { return m_map.at(id.value); }

    const ValueType &Get(typename ObjectType::ID id) const
        { return m_map.at(id.value); }

    void Set(typename ObjectType::ID id, ValueType &&value)
    {
        m_map[id.value] = std::move(value);
    }

    void Remove(typename ObjectType::ID id)
    {
        m_map.erase(id.value);
    }

    void Clear()
    {
        m_map.clear();
    }

    size_t Size() const
    {
        return m_map.size();
    }

    inline ValueType &operator[](typename ObjectType::ID id)
        { return m_map[id.value]; }

    auto begin() { return m_map.begin(); }
    auto end() { return m_map.end(); }

private:
    std::unordered_map<typename ObjectType::ID::ValueType, ValueType> m_map;
};


/* New ObjectHolder class that does not call Destroy,
 * as new component types will not have Destroy() rather using
 * a teardown method which is defined in CallbackTrackable.
 */
template <class T, class CallbacksClass>
struct ObjectVector {
    std::vector<std::unique_ptr<T>> objects;
    std::queue<size_t> free_slots;
    CallbacksClass &callbacks;

    ObjectVector(CallbacksClass &callbacks)
        : callbacks(callbacks)
    {
    }

    ObjectVector(const ObjectVector &other) = delete;
    ObjectVector &operator=(const ObjectVector &other) = delete;
    ~ObjectVector() {}

    constexpr size_t Size() const
        { return objects.size(); }

    auto &Objects() { return objects; }
    const auto &Objects() const { return objects; }

    constexpr T *Get(typename T::ID id)
    {
        return MathUtil::InRange(id.value, {1, objects.size() + 1})
            ? objects[id.value - 1].get()
            : nullptr;
    }

    constexpr const T *Get(typename T::ID id) const
        { return const_cast<ObjectVector<T, CallbacksClass> *>(this)->Get(id); }

    constexpr T *operator[](typename T::ID id)             { return Get(id); }
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
    
    T *Add(std::unique_ptr<T> &&object)
    {
        AssertThrow(object != nullptr);
        AssertThrowMsg(object->GetId() == T::empty_id, "Adding object that already has id set");

        typename T::ID next_id;

        if (!free_slots.empty()) {
            auto front = free_slots.front();

            next_id = typename T::ID{typename T::ID::ValueType(front + 1)};
            object->SetId(next_id);

            objects[front] = std::move(object);
            free_slots.pop();
        } else {
            next_id = typename T::ID{typename T::ID::ValueType(objects.size() + 1)};
            object->SetId(next_id);

            objects.push_back(std::move(object));
        }

        return objects[next_id.value - 1].get();
    }
    
    void Remove(typename T::ID id)
    {
        objects[id.value - 1].reset();

        if (id.value == objects.size()) {
            objects.pop_back();
        } else {
            /* Cannot simply erase from vector, as that would invalidate existing IDs */
            
            free_slots.push(id.value - 1);
        }
    }

    void Destroy(typename T::ID id)
    {
        objects[id.value - 1].reset();
    }
    
    void RemoveAll() { objects.clear(); }

};

template <class T, class CallbacksClass>
class RefCounter {
    using ArgsTuple = typename CallbacksClass::ArgsTuple;

    struct RefCount {
        std::atomic_uint32_t count{0};
    };

    ObjectVector<T, CallbacksClass>   m_holder;
    mutable ObjectMap<T, RefCount>    m_ref_map;

    ArgsTuple                         m_init_args{};

public:
    class Ref {
    public:
        Ref() : Ref(nullptr)
        {
        }

        Ref(std::nullptr_t) : Ref(nullptr, nullptr)
        {
        }

        Ref(T *ptr, RefCounter *ref_counter)
            : ptr(ptr),
              m_ref_counter(ref_counter)
        {
        }

        Ref(const Ref &other) = delete;
        Ref &operator=(const Ref &other) = delete;

        Ref(Ref &&other) noexcept
            : ptr(other.ptr),
              m_ref_counter(other.m_ref_counter)
        {
            other.ptr = nullptr;
            other.m_ref_counter = nullptr;
        }

        Ref &operator=(Ref &&other) noexcept
        {
            if (std::addressof(other) == this || *this == other) {
                return *this;
            }

            if (Valid()) {
                Release();
            }

            ptr = other.ptr;
            m_ref_counter = other.m_ref_counter;

            other.ptr           = nullptr;
            other.m_ref_counter = nullptr;

            return *this;
        }

        ~Ref()
        {
            if (ptr != nullptr) {
                Release();
            }
        }

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

        bool operator==(const Ref &other) const
            { return &other == this || (ptr == other.ptr && m_ref_counter == other.m_ref_counter); }

        bool operator!=(const Ref &other) const
            { return !operator==(other); }
        
        /*! \brief _If_ ptr has not had Init() performed yet (checked via IsInit()), call it, using
         * the pre-bound arguments from the reference counter this wrapper was acquired from.
         * Throws an assertion error if state is invalid.
         */
        void Init()
        {
            AssertState();
            
            if (!ptr->IsInitCalled()) {
                /* Call ptr->Init() with the ref counter's pre-bound args */
                std::apply(&T::Init, std::tuple_cat(std::make_tuple(ptr), m_ref_counter->m_init_args));
            }
        }

        /*! \brief _If_ ptr has not had Init() performed yet (checked via IsInit()), call it.
         * Throws an assertion error if state is invalid.
         * @param args The arguments to be forwarded to Init()
         */
        template <class ...Args>
        void Init(Args &&... args)
        {
            AssertState();
            
            if (!ptr->IsInitCalled()) {
                /* Call ptr->Init() with the provided args */
                ptr->Init(std::forward<Args>(args)...);
            }
        }

        /*! \brief Acquire a new reference from this one. Increments the reference counter. */
        auto IncRef()
        {
            AssertState();

            return m_ref_counter->IncRef(ptr);
        }

        size_t GetRefCount() const
        {
            if (!Valid()) {
                return 0;
            }

            return m_ref_counter->GetRefCount(ptr->GetId());
        }

        T *ptr;

    private:
        bool Valid() const
            { return ptr != nullptr && m_ref_counter != nullptr; }

        void AssertState()
        {
            AssertThrowMsg(ptr != nullptr,           "underlying pointer was null");
            AssertThrowMsg(m_ref_counter != nullptr, "ref counter not set");
        }

        void Release()
        {
            AssertState();

            m_ref_counter->Release(ptr);

            ptr = nullptr;
            m_ref_counter = nullptr;
        }

        RefCounter *m_ref_counter;
    };

    RefCounter(CallbacksClass &callbacks)
        : m_holder(callbacks),
          m_ref_map{}
    {
    }

    RefCounter(CallbacksClass &callbacks, ArgsTuple &&bind_init_args)
        : m_holder(callbacks),
          m_ref_map{},
          m_init_args(std::move(bind_init_args))
    {
    }

    ~RefCounter()
    {
        for (auto &it : m_ref_map) {
            auto &rc = it.second;

            if (rc.count == 0) { /* not yet initialized */
                DebugLog(
                    LogType::Warn,
                    "Ref to object of type %s was never initialized\n",
                    typeid(T).name()
                );
            } else {
                --rc.count;
            }

            AssertThrowMsg(rc.count == 0, "Destructor called while object still in use elsewhere");
        }
    }

    /*! \brief Sets the args tuple that is passed to Init() for any newly acquired object. */
    void BindInitArguments(ArgsTuple &&args)
    {
        m_init_args = std::move(args);
    }
    
    [[nodiscard]] Ref Add(std::unique_ptr<T> &&object)
    {
        if (object == nullptr) {
            return nullptr;
        }

        //std::lock_guard guard(m_mutex);

        T *ptr = m_holder.Add(std::move(object));
        m_ref_map[ptr->GetId()].count = 1;
        
        return Ref(ptr, this);
    }

    [[nodiscard]] Ref Lookup(typename T::ID id)
    {
        std::lock_guard guard(m_mutex);

        T *ptr = m_holder.Get(id);

        if (ptr == nullptr) {
            return nullptr;
        }

        return IncRef(ptr);//ptr.IncRef();
    }

    size_t GetRefCount(typename T::ID id) const
    {
        if (!m_ref_map.Has(id)) {
            return 0;
        }

        return m_ref_map.Get(id).count;
    }

    auto &Objects() { return m_holder.objects; }
    const auto &Objects() const { return m_holder.objects; }

private:
    friend class Ref;

    [[nodiscard]] Ref IncRef(T *ptr)
    {
        AssertThrow(ptr != nullptr);

        ++m_ref_map.Get(ptr->GetId()).count;
        
        return Ref(ptr, this);
    }

    void Release(const T *ptr)
    {
        AssertThrow(ptr != nullptr);
        const auto id = ptr->GetId();

        //std::lock_guard guard(m_mutex);
        
        AssertThrowMsg(m_ref_map.Has(id), "Refcount not set");

        auto &counter = m_ref_map.Get(id);
        AssertThrowMsg(counter.count != 0, "Cannot decrement refcount when already at zero");
        
        if (!--counter.count) {
            m_ref_map.Remove(id);
            m_holder.Remove(id);
        }
    }

    std::mutex m_mutex;
};

template <class T>
using Ref = typename RefCounter<T, EngineCallbacks>::Ref;

} // namespace hyperion::v2

#endif
